#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>

// Pak 文件常量
#define OFFSET_KEY 0xD74AF37FAA6B020D
#define SIZE_KEY 0x8924B0E3298B7069
#define CHUNK_SIZE 65536

// Pak 文件头结构体，位于文件末尾
typedef struct {
    uint8_t encrypted;
    uint32_t magic;
    uint32_t version;
    uint8_t hash[20];
    uint64_t size;
    uint64_t offset;
} __attribute__((packed)) PakInfo;

// 压缩块结构体
typedef struct {
    uint64_t start;
    uint64_t end;
} __attribute__((packed)) CompressionBlock;

// 单个文件条目元数据结构体
typedef struct {
    uint8_t FileHash[20];
    uint64_t FileOffset;
    uint64_t FileSize;
    uint32_t CompressionMethod;
    uint64_t CompressedLength;
    uint8_t Dummy[21];
    uint32_t NumOfBlocks;
    CompressionBlock *blocks;
    uint32_t CompressedBlockSize;
    uint8_t Encrypted;
    char *filename; // 用于更新时存储文件名
} __attribute__((packed)) Entry;

// 文件计数器，用于生成类似 bms 脚本的文件名
typedef struct {
    int file_index;
    int folder_index;
    int files_in_folder;
} FileCounters;

FileCounters counters = { .file_index = 0, .folder_index = 0, .files_in_folder = 0 };

// 声明全局变量，用于在不同函数间共享文件条目信息
Entry *file_entries = NULL;
uint32_t num_of_entries = 0;

// 解密数据函数
void DecryptData(uint8_t *data, uint32_t size) {
    for (uint32_t index = 0; index < size; index++) {
        data[index] ^= 0x79u;
    }
}

// 创建文件并处理目录结构函数
int create_file(const char *fullPath) {
    const char *lastSlash = strrchr(fullPath, '/');
    if (lastSlash != NULL) {
        size_t length = (size_t)(lastSlash - fullPath);
        char path[length + 1];
        strncpy(path, fullPath, length);
        path[length] = '\0';
        char *token = strtok(path, "/");
        char currentPath[1024] = "";
        while (token != NULL) {
            strcat(currentPath, token);
            strcat(currentPath, "/");
            mkdir(currentPath, 0777);
            token = strtok(NULL, "/");
        }
    }
    return open(fullPath, O_WRONLY | O_CREAT | O_TRUNC, 0644); 
}

// ZLIB 解压缩函数
unsigned int ZLIB_decompress(unsigned char *InData, unsigned int InSize, unsigned char *OutData, unsigned int OutSize) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = InData;
    strm.avail_in = InSize;
    strm.next_out = OutData;
    strm.avail_out = OutSize;
    
    if (inflateInit(&strm) != Z_OK) {
        fprintf(stderr, "Failed to initialize zlib.\n");
        return 0;
    }
    
    if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
        fprintf(stderr, "inflate failed: %s\n", strm.msg);
        inflateEnd(&strm);
        return 0;
    }
    
    if (inflateEnd(&strm) != Z_OK) {
        fprintf(stderr, "inflateEnd failed: %s\n", strm.msg);
        return 0;
    }
    return strm.total_out;
}

// ZLIB 压缩函数
int ZLIB_compress(const uint8_t *in_data, size_t in_size, uint8_t *out_data, size_t *out_size) {
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        return ret;
    }

    strm.avail_in = in_size;
    strm.next_in = (uint8_t *)in_data;
    strm.avail_out = *out_size;
    strm.next_out = out_data;

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        return ret == Z_OK ? Z_BUF_ERROR : ret;
    }

    *out_size = strm.total_out;
    deflateEnd(&strm);
    return Z_OK;
}

// 全局变量，用于在读索引数据时进行偏移
uint64_t current_index_offset = 0;

// 从源数据读取指定长度的数据到目标
void read_data(void *destination, const uint8_t *source, size_t length) {
    memcpy(destination, source + current_index_offset, length);
    current_index_offset += length;
}

// Unicode 转 UTF-8 函数
int unicode_to_utf8(const char *input, size_t input_len, char *out, size_t output_len) {
    char output[output_len];
    size_t i = 0, j = 0;
    
    while (i < input_len && j < output_len) {
        unsigned int unicode_char;
        unicode_char = (unsigned char)input[i++];
        unicode_char |= (unsigned char)input[i++] << 8;
        
        if (unicode_char <= 0x7F) {
            output[j++] = (char)unicode_char;
        } else if (unicode_char <= 0x7FF) {
            if (j + 1 >= output_len) return -1;
            output[j++] = 0xC0 | (unicode_char >> 6);
            output[j++] = 0x80 | (unicode_char & 0x3F);
        } else {
            if (j + 2 >= output_len) return -1;
            output[j++] = 0xE0 | (unicode_char >> 12);
            output[j++] = 0x80 | ((unicode_char >> 6) & 0x3F);
            output[j++] = 0x80 | (unicode_char & 0x3F);
        }
    }
    if (i < input_len) return -1;
    memcpy(out, output, j);
    return 0;
}

// 提取单个文件函数，现在接受输出目录作为参数
void extract(int PakFile, Entry entry, const char *output_dir)
{
    char filename[1024];

    // 根据文件总数来更新文件夹索引。例如，当文件总数达到1000、2000等时，更新文件夹名称。
    if (counters.file_index > 0 && counters.file_index % 1000 == 0)
    {
        counters.folder_index = counters.file_index;
    }

    // 目录命名规范: file_0, file_1000, etc.
    // 文件名则使用8位零填充的绝对文件索引。
    snprintf(filename, 1024, "%s/file_%d/%08d.dat", output_dir, counters.folder_index, counters.file_index);

    // 递增文件总数，以便下次使用。
    counters.file_index++;
    
    int OutFile = create_file(filename);
    
    if (OutFile == -1) {
        printf("Failed to open output file: %s\n", filename);
        exit(1);
    }
    
    if (entry.NumOfBlocks > 0) {
        for (int x = 0; x < entry.NumOfBlocks; x++) {
            uint64_t compressed_chunk_size = entry.blocks[x].end - entry.blocks[x].start;
            uint8_t *compressed_data = (uint8_t*)malloc(compressed_chunk_size);
            if (!compressed_data) {
                printf("Memory allocation failed.\n");
                exit(1);
            }
            if (pread(PakFile, compressed_data, compressed_chunk_size, entry.blocks[x].start) != compressed_chunk_size) {
                printf("Failed to read compressed chunk at %lx\n", entry.blocks[x].start);
                exit(1);
            }
            if (entry.Encrypted) {
                DecryptData(compressed_data, compressed_chunk_size);
            }
            uint8_t *decompressed_data = (uint8_t*)malloc(CHUNK_SIZE);
            if (!decompressed_data) {
                printf("Memory allocation failed.\n");
                exit(1);
            }
            
            size_t DecompressLength = 0;
            if (entry.CompressionMethod == 1) { // ZLIB
                DecompressLength = ZLIB_decompress(compressed_data, compressed_chunk_size, decompressed_data, CHUNK_SIZE);
                if (DecompressLength == 0) {
                    fprintf(stderr, "ZLIB Decompression failed\n");
                    exit(1);
                }
            } else {
                printf("Unknown compression method %u\n", entry.CompressionMethod);
                close(OutFile);
                exit(1);
            }
            
            write(OutFile, decompressed_data, DecompressLength);
            printf("offset %016lx size %lu\tfilename %s\n", entry.blocks[x].start, DecompressLength, filename);
            free(compressed_data);
            free(decompressed_data);
        }
    } else { // 如果文件未压缩
        lseek(PakFile, entry.FileOffset + 74, SEEK_SET);
        ssize_t bytesRead, bytesWritten;
        uint64_t remaining_size = entry.FileSize;
        uint8_t *buffer = (uint8_t*)malloc(CHUNK_SIZE);
        if (!buffer) {
            printf("Memory allocation failed.\n");
            exit(1);
        }
        
        while (remaining_size > 0) {
            size_t bytesToRead = remaining_size < CHUNK_SIZE ? remaining_size : CHUNK_SIZE;
            bytesRead = read(PakFile, buffer, bytesToRead);
            if (bytesRead < 0) {
                printf("Error reading from file\n");
                break;
            }
            if (entry.Encrypted) {
                DecryptData(buffer, bytesRead);
            }
            bytesWritten = write(OutFile, buffer, bytesRead);
            if (bytesWritten != bytesRead) {
                printf("Error writing to output file\n");
                break;
            }
            remaining_size -= bytesRead;
            printf("offset %016lx size %zd\tfilename %s\n", entry.FileOffset, bytesWritten, filename);
        }
        free(buffer);
    }
    
    close(OutFile);
    return;
}

// 解包函数
void unpack_pak(const char *pak_file, const char *output_dir) {
    // 调试信息: 确认接收到的文件路径
    printf("Received pak file path: %s\n", pak_file);
    printf("Received output directory: %s\n", output_dir);
    
    clock_t t0 = clock();
    PakInfo info;
    
    int PakFile = open(pak_file, O_RDONLY);
    
    if (PakFile == -1) {
        printf("Unable to open %s file\n", pak_file);
        return;
    }
    
    off_t total = lseek(PakFile, 0, SEEK_END);
    lseek(PakFile, 0, SEEK_SET);
    
    if (lseek(PakFile, -45, SEEK_END) == -1) {
        printf("failed to seek file position\n");
        return;
    }
    
    if (read(PakFile, &info, 45) != 45) {
        printf("Failed to read pak header at -45\n");
        return;
    }
    
    info.offset ^= OFFSET_KEY;
    info.encrypted ^= 0x6C;
    
    int64_t size = lseek(PakFile, -info.offset, SEEK_END); 
    size -= 45;
    
    if (size > 52428800) {
        fprintf(stderr, "Index data size is not compatible.\n");
        close(PakFile);
        return;
    }
    
    uint8_t *IndexData = (uint8_t*)malloc(size);
    if (!IndexData) {
        printf("Memory allocation failed.\n");
        close(PakFile);
        return;
    }

    if (pread(PakFile, IndexData, size, info.offset) != size) {
        fprintf(stderr, "Failed to load index data\n");
        return;
    }
    
    if (info.encrypted) {
        DecryptData(IndexData, size);
    }
    
    uint32_t MountPointLength;
    char MountPoint[1024];
    int NumOfEntry;
    int32_t FilenameSize;
    char Filename[1024];
    
    printf("offset\t\tfilesize\t\tfilename\n");
    printf("--------------------------------------\n");
    
    read_data(&MountPointLength, IndexData, 4);
    read_data(MountPoint, IndexData, MountPointLength);

    read_data(&NumOfEntry, IndexData, 4);

    Entry *entry = (Entry*)malloc(NumOfEntry * sizeof(Entry));
    if (!entry) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(IndexData);
        close(PakFile);
        return;
    }
    
    for (uint32_t Files = 0; Files < NumOfEntry; Files++) {
        read_data(entry[Files].FileHash, IndexData, 20);
        read_data(&entry[Files].FileOffset, IndexData, 8);
        read_data(&entry[Files].FileSize, IndexData, 8);
        read_data(&entry[Files].CompressionMethod, IndexData, 4);
        read_data(&entry[Files].CompressedLength, IndexData, 8);
        read_data(entry[Files].Dummy, IndexData, 21);

        if (entry[Files].CompressionMethod != 0) {
            read_data(&entry[Files].NumOfBlocks, IndexData, 4);
            entry[Files].blocks = (CompressionBlock*)malloc(entry[Files].NumOfBlocks * sizeof(CompressionBlock));
            
            if (!entry[Files].blocks) {
                fprintf(stderr, "Memory allocation for blocks failed\n");
                return;
            }
            
            for (uint32_t i = 0; i < entry[Files].NumOfBlocks; i++) {
                read_data(&entry[Files].blocks[i].start, IndexData, 8);
                read_data(&entry[Files].blocks[i].end, IndexData, 8);
            }
        } else {
            entry[Files].NumOfBlocks = 0;
        }

        read_data(&entry[Files].CompressedBlockSize, IndexData, 4);
        read_data(&entry[Files].Encrypted, IndexData, 1);
    }
    
    uint64_t ENTRIES = 0;
    uint64_t DIR_COUNT = 0;
    
    read_data(&ENTRIES, IndexData, 8);
    read_data(&DIR_COUNT, IndexData, 8);
    
    int32_t DIR_LEN = 0;
    char DIR_NAME[1024];
    uint64_t DIR_FILES = 0;
    int32_t ENTRY;
    
    for (int files = 0; files < DIR_COUNT; files++) {
        read_data(&DIR_LEN, IndexData, 4);
        read_data(DIR_NAME, IndexData, DIR_LEN);
        read_data(&DIR_FILES, IndexData, 8);
        
        for (int x = 0; x < DIR_FILES; x++) {
            read_data(&FilenameSize, IndexData, 4);
            
            if (FilenameSize > 0) {
                read_data(Filename, IndexData, FilenameSize);
            } else {
                printf("FilenameSize = '%d' \n", FilenameSize);
                read_data(Filename, IndexData, -FilenameSize * 2);
                if (unicode_to_utf8(Filename, -FilenameSize * 2, Filename, sizeof(Filename)) == -1) {
                    printf("failed to convert UTF-16LE filename into UTF-8!\n");
                    exit(1);
                }
            }
            
            read_data(&ENTRY, IndexData, 4);
            extract(PakFile, entry[ENTRY], output_dir);
            // counters.file_index++;
        }
    }
    
    for (uint32_t Files = 0; Files < NumOfEntry; Files++) {
        free(entry[Files].blocks);
    }
    free(entry);
    free(IndexData);
    close(PakFile);
    
    clock_t t1 = clock();
    double time = ((double)(t1 - t0)) / CLOCKS_PER_SEC;
    const double MB = 1024.0 * 1024.0;
    printf("Processed %.2f MB, speed = %.2f MB/s, %u files found in %f seconds\n", total / MB, total / MB / time, NumOfEntry, time);
    return;
}


// 遍历目录并收集文件信息
Entry* get_files_from_dir(const char* dat_dir, uint32_t* num_files) {
    DIR *d;
    struct dirent *dir;
    char path[2048];
    Entry* entries = NULL;
    uint32_t count = 0;

    d = opendir(dat_dir);
    if (!d) {
        perror("Failed to open directory");
        *num_files = 0;
        return NULL;
    }

    while ((dir = readdir(d)) != NULL) {
        // 忽略 "." 和 ".."
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        // 构建完整路径
        snprintf(path, sizeof(path), "%s/%s", dat_dir, dir->d_name);
        
        struct stat st;
        if (stat(path, &st) == -1) {
            perror("Failed to stat file/directory");
            continue;
        }

        if (S_ISDIR(st.st_mode)) { // 检查是否是目录
            // 递归调用此函数来遍历子目录
            uint32_t subdir_num_files = 0;
            Entry* subdir_entries = get_files_from_dir(path, &subdir_num_files);

            if (subdir_entries) {
                // 重新分配内存以容纳新文件
                entries = realloc(entries, (count + subdir_num_files) * sizeof(Entry));
                if (!entries) {
                    perror("Failed to reallocate memory");
                    for (uint32_t i = 0; i < subdir_num_files; ++i) {
                        free(subdir_entries[i].filename);
                    }
                    free(subdir_entries);
                    closedir(d);
                    *num_files = 0;
                    return NULL;
                }
                // 将新文件信息复制到主列表中
                memcpy(entries + count, subdir_entries, subdir_num_files * sizeof(Entry));
                count += subdir_num_files;
                // 注意: subdir_entries 中的 filename 指针已移动，所以只需释放数组本身
                free(subdir_entries);
            }
        } else if (S_ISREG(st.st_mode)) { // 检查是否是常规文件
            // 检查文件名是否以 ".dat" 结尾
            const char* name = dir->d_name;
            size_t name_len = strlen(name);
            const char* ext = ".dat";
            size_t ext_len = strlen(ext);
            
            if (name_len >= ext_len && strcmp(name + name_len - ext_len, ext) == 0) {
                entries = realloc(entries, (count + 1) * sizeof(Entry));
                if (!entries) {
                    perror("Failed to reallocate memory");
                    closedir(d);
                    *num_files = 0;
                    return NULL;
                }

                // 填充文件信息
                entries[count].filename = strdup(path);
                if (!entries[count].filename) {
                    perror("Failed to duplicate string");
                    closedir(d);
                    *num_files = 0;
                    return NULL;
                }
                entries[count].FileSize = st.st_size;
                entries[count].CompressionMethod = 1; 
                entries[count].Encrypted = 1; 
                entries[count].CompressedLength = 0; 
                entries[count].NumOfBlocks = 0;
                entries[count].blocks = NULL;
                
                memset(entries[count].FileHash, 0, sizeof(entries[count].FileHash));
                memset(entries[count].Dummy, 0, sizeof(entries[count].Dummy));

                count++;
            }
        }
    }

    closedir(d);
    *num_files = count;
    return entries;
}

// 此函数根据 .dat 目录中的文件更新现有的 .pak 文件。
// 它只会替换原始文件的数据区域，而保留索引和头部信息区域。
void update_pak_by_dat(const char* pak_file, const char* dat_dir) {
    // 打印状态信息。
    printf("Updating '%s' with files from '%s'...\n", pak_file, dat_dir);

    // 1. 读取原始 pak 文件的元数据 (PakInfo) 和索引数据
    // 以读写模式打开 pak 文件。
    int pakFile = open(pak_file, O_RDWR);
    if (pakFile == -1) {
        perror("Failed to open pak file for read/write");
        return;
    }

    struct stat st;
    if (stat(pak_file, &st) == -1) {
        perror("Failed to get file size");
        close(pakFile);
        return;
    }

    PakInfo info;
    lseek(pakFile, -sizeof(PakInfo), SEEK_END);
    if (read(pakFile, &info, sizeof(PakInfo)) != sizeof(PakInfo)) {
        perror("Failed to read pak header");
        close(pakFile);
        return;
    }

    // 解密头部信息，以便获取正确的索引偏移量和大小。
    uint64_t index_offset = info.offset ^ OFFSET_KEY;
    uint64_t index_data_size = st.st_size - index_offset - sizeof(PakInfo);

    // 为索引数据分配内存。
    uint8_t* index_data = (uint8_t*)malloc(index_data_size);
    if (!index_data) {
        perror("Memory allocation for index failed");
        close(pakFile);
        return;
    }

    // 读取原始索引数据。
    if (pread(pakFile, index_data, index_data_size, index_offset) != index_data_size) {
        perror("Failed to read index data from pak file");
        free(index_data);
        close(pakFile);
        return;
    }
    
    // 如果 PakInfo 中的加密标志为真，则调用解密函数。
    if ((info.encrypted ^ 0x6c) != 0) {
        DecryptData(index_data, index_data_size);
    }
    
    // 解析原始索引数据以获取文件条目列表
    uint32_t NumOfEntry;
    uint8_t* index_data_ptr = index_data;

    // 跳过挂载点数据
    uint32_t MountPointLength;
    memcpy(&MountPointLength, index_data_ptr, sizeof(MountPointLength));
    index_data_ptr += sizeof(MountPointLength) + MountPointLength;

    // 读取文件条目总数
    memcpy(&NumOfEntry, index_data_ptr, sizeof(NumOfEntry));
    index_data_ptr += sizeof(NumOfEntry);
    
    // 为原始条目列表分配内存
    Entry* original_entries = (Entry*)malloc(NumOfEntry * sizeof(Entry));
    if (!original_entries) {
        fprintf(stderr, "Memory allocation for original entries failed!\n");
        free(index_data);
        close(pakFile);
        return;
    }

    // 遍历索引数据，解析并保存每个文件条目的信息
    for (uint32_t Files = 0; Files < NumOfEntry; Files++) {
        memcpy(original_entries[Files].FileHash, index_data_ptr, 20);
        index_data_ptr += 20;
        memcpy(&original_entries[Files].FileOffset, index_data_ptr, 8);
        index_data_ptr += 8;
        memcpy(&original_entries[Files].FileSize, index_data_ptr, 8);
        index_data_ptr += 8;
        memcpy(&original_entries[Files].CompressionMethod, index_data_ptr, 4);
        index_data_ptr += 4;
        memcpy(&original_entries[Files].CompressedLength, index_data_ptr, 8);
        index_data_ptr += 8;
        memcpy(original_entries[Files].Dummy, index_data_ptr, 21);
        index_data_ptr += 21;

        if (original_entries[Files].CompressionMethod != 0) {
            memcpy(&original_entries[Files].NumOfBlocks, index_data_ptr, 4);
            index_data_ptr += 4;
            // 跳过 CompressionBlock 数据，因为我们只关心偏移量和大小
            index_data_ptr += original_entries[Files].NumOfBlocks * sizeof(CompressionBlock);
        } else {
            original_entries[Files].NumOfBlocks = 0;
        }

        memcpy(&original_entries[Files].CompressedBlockSize, index_data_ptr, 4);
        index_data_ptr += 4;
        memcpy(&original_entries[Files].Encrypted, index_data_ptr, 1);
        index_data_ptr += 1;
    }

    // 2. 遍历 .dat 目录，压缩并写入新数据
    // 将文件截断到旧数据区域的末尾，即索引区域的起始位置。
    ftruncate(pakFile, index_offset);
    lseek(pakFile, 0, SEEK_SET);

    uint32_t dat_file_count;
    // 获取新文件的列表。
    Entry* new_entries = get_files_from_dir(dat_dir, &dat_file_count);
    if (!new_entries) {
        fprintf(stderr, "No files found in directory %s\n", dat_dir);
        free(index_data);
        free(original_entries);
        close(pakFile);
        return;
    }

    // 记录新的文件数据起始偏移量。
    uint64_t current_data_offset = 0;
    
    // 遍历新文件，压缩并写入文件。
    for (uint32_t i = 0; i < dat_file_count; ++i) {
        int dat_file = open(new_entries[i].filename, O_RDONLY);
        if (dat_file == -1) {
            perror("Failed to open .dat file");
            continue;
        }

        uint8_t* in_data = (uint8_t*)malloc(original_entries[i].FileSize);
        if (!in_data) {
            perror("Failed to allocate memory for input data");
            close(dat_file);
            continue;
        }
        read(dat_file, in_data, original_entries[i].FileSize);
        close(dat_file);

        uint8_t* compressed_data = (uint8_t*)malloc(original_entries[i].FileSize);
        if (!compressed_data) {
            perror("Failed to allocate memory for compressed data");
            free(in_data);
            continue;
        }
        size_t compressed_size = original_entries[i].FileSize;
        
        int ret = ZLIB_compress(in_data, original_entries[i].FileSize, compressed_data, &compressed_size);
        if (ret != Z_OK) {
            fprintf(stderr, "ZLIB compression failed for file %s\n", new_entries[i].filename);
            free(in_data);
            free(compressed_data);
            continue;
        }

        // 打印更新前的文件信息
        printf("准备写入文件: %s\n", new_entries[i].filename);
        if (i < NumOfEntry)
        {
            // 计算差值（新值 - 原值）
            long size_diff = (long)(compressed_size - original_entries[i].CompressedLength);
            printf("  - 压缩后大小: 新值=%zu, 原来值=%lu, 差值=%+ld\n",
                   compressed_size,
                   original_entries[i].CompressedLength,
                   size_diff);
        }
        else
        {
            printf("  - 这是新增的文件，无原始条目可供比较。\n");
        }

        // 更新条目信息，这里只更新偏移量和压缩后大小。
        // new_entries[i].FileOffset = current_data_offset;
        // new_entries[i].CompressedLength = compressed_size;
        
        // 写入压缩数据到 .pak 文件。
        write(pakFile, compressed_data, compressed_size);
        current_data_offset += compressed_size;

        free(in_data);
        free(compressed_data);
    }
    
    // 3. 检查新旧数据区大小是否一致
    if (current_data_offset != index_offset) {
        fprintf(stderr, "Error: New data size (%lu) does not match original data size (%lu). This will corrupt the file. Aborting.\n", current_data_offset, index_offset);
    } else {
        printf("New data size matches original data size. The original index and header remain valid.\n");
    }

    // 4. 清理内存和文件句柄
    for (uint32_t i = 0; i < dat_file_count; ++i) {
        free(new_entries[i].filename);
        free(new_entries[i].blocks);
    }
    free(new_entries);
    free(original_entries);
    free(index_data);
    close(pakFile);
    
    if (current_data_offset == index_offset) {
        printf("Update completed successfully.\n");
    }
}

int main(void) {
    // 您可以更改这个变量来切换模式
    // 设置为 true 来执行 update
    // 设置为 false 来执行 unpack
    bool update_mode = false;

    // const char* pak_path = "../paks/map_lobby_1.33.12.14210.pak";
    const char* pak_path = "../paks/game_patch_1.33.12.14267.pak";
    const char* dat_path = "../paks/dat_temp";
    // const char* output_pak = "../paks/game_patch_1.33.12.14226new.pak";
    if (update_mode) {
        // --- 更新模式 ---
        update_pak_by_dat(pak_path, dat_path);
    } else {
        // --- 解包模式 ---
        unpack_pak(pak_path, dat_path);
    }

    return 0;
}
