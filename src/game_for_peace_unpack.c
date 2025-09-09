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

#define OFFSET_KEY 0xD74AF37FAA6B020D
#define SIZE_KEY 0x8924B0E3298B7069
#define CHUNK_SIZE 65536

// Pak文件头结构体，位于文件末尾
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

// 单个文件条目的元数据结构体
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
} __attribute__((packed)) Entry;

// 文件计数器结构体，用于生成类似bms脚本的命名
typedef struct {
    int file_index;
    int folder_index;
    int files_in_folder;
} FileCounters;

FileCounters counters = { .file_index = 0, .folder_index = 0, .files_in_folder = 0 };

void DecryptData(uint8_t *data, uint32_t size) {
    for (uint32_t index = 0; index < size; index++) {
        data[index] ^= 0x79u;
    }
}

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

uint64_t current_index_offset = 0;

void read_data(void *destination, const uint8_t *source, size_t length) {
    memcpy(destination, source + current_index_offset, length);
    current_index_offset += length;
}

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

// 提取函数，现在接受输出目录作为参数
void extract(int PakFile, Entry entry, const char* output_dir) {
    char filename[1024];
    
    if (counters.files_in_folder >= 1000) {
        counters.folder_index++;
        counters.files_in_folder = 0;
    }

    snprintf(filename, 1024, "%s/file_%d/%08d.dat", output_dir, counters.folder_index, counters.files_in_folder);
    counters.files_in_folder++;
    
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
    // 调试信息：确认收到的文件路径
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

        printf("\n");
        // 打印文件偏移量、原始大小、压缩方法和压缩后大小
        printf("FileOffset: 0x%llx (%llu)\n", entry[Files].FileOffset, entry[Files].FileOffset); // 同时打印十六进制和十进制
        printf("FileSize: %llu\n", entry[Files].FileSize);
        printf("CompressionMethod: %u\n", entry[Files].CompressionMethod);
        printf("CompressedLength: %llu\n", entry[Files].CompressedLength);
        printf("------------------------\n");

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
                read_data(Filename, IndexData, -FilenameSize * 2);
                if (unicode_to_utf8(Filename, -FilenameSize * 2, Filename, sizeof(Filename)) == -1) {
                    printf("failed to convert UTF-16LE filename into UTF-8!\n");
                    exit(1);
                }
            }
            
            read_data(&ENTRY, IndexData, 4);
            extract(PakFile, entry[ENTRY], output_dir);
            counters.file_index++;
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

// 打包功能函数
void repack_pak(const char *input_dir, const char *output_pak_file) {
    printf("Repacking files from '%s' into '%s'...\n", input_dir, output_pak_file);

    int OutFile = open(output_pak_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (OutFile == -1) {
        perror("Failed to open output pak file");
        return;
    }

    DIR *dir = opendir(input_dir);
    if (!dir) {
        perror("Failed to open input directory");
        close(OutFile);
        return;
    }

    struct dirent *entry_dir;
    struct stat st;
    off_t current_offset = 0;
    Entry *entries = NULL;
    int num_files = 0;
    char filepath[2048];

    // 第一遍遍历: 收集文件列表和元数据
    while ((entry_dir = readdir(dir)) != NULL) {
        if (strcmp(entry_dir->d_name, ".") == 0 || strcmp(entry_dir->d_name, "..") == 0) {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", input_dir, entry_dir->d_name);
        if (stat(filepath, &st) == -1) {
            perror("Failed to get file stats");
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            entries = realloc(entries, (num_files + 1) * sizeof(Entry));
            if (!entries) {
                perror("Memory allocation failed");
                closedir(dir);
                close(OutFile);
                return;
            }
            
            memset(&entries[num_files], 0, sizeof(Entry));
            entries[num_files].FileOffset = current_offset;
            entries[num_files].FileSize = st.st_size;
            entries[num_files].CompressedLength = st.st_size;
            entries[num_files].CompressionMethod = 0; // 未压缩
            entries[num_files].NumOfBlocks = 0;
            entries[num_files].Encrypted = 0;
            
            current_offset += st.st_size;
            num_files++;
        }
    }
    closedir(dir);

    // 第二遍遍历: 将文件数据写入pak
    dir = opendir(input_dir);
    if (!dir) {
        perror("Failed to re-open input directory");
        free(entries);
        close(OutFile);
        return;
    }
    
    int file_idx = 0;
    while ((entry_dir = readdir(dir)) != NULL) {
        if (strcmp(entry_dir->d_name, ".") == 0 || strcmp(entry_dir->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(filepath, sizeof(filepath), "%s/%s", input_dir, entry_dir->d_name);
        stat(filepath, &st);
        
        if (S_ISREG(st.st_mode)) {
            int InFile = open(filepath, O_RDONLY);
            if (InFile == -1) {
                perror("Failed to open input file for reading");
                continue;
            }

            uint8_t buffer[CHUNK_SIZE];
            ssize_t bytesRead;
            while ((bytesRead = read(InFile, buffer, CHUNK_SIZE)) > 0) {
                write(OutFile, buffer, bytesRead);
            }
            close(InFile);
        }
    }
    closedir(dir);

    // 构建并写入索引
    uint64_t index_offset = lseek(OutFile, 0, SEEK_CUR);
    
    // 写入挂载点 (简化处理)
    uint32_t mount_point_len = 0;
    write(OutFile, &mount_point_len, 4);

    // 写入文件数量
    write(OutFile, &num_files, 4);
    
    // 写入文件条目元数据
    for (int i = 0; i < num_files; i++) {
        write(OutFile, entries[i].FileHash, 20);
        write(OutFile, &entries[i].FileOffset, 8);
        write(OutFile, &entries[i].FileSize, 8);
        write(OutFile, &entries[i].CompressionMethod, 4);
        write(OutFile, &entries[i].CompressedLength, 8);
        write(OutFile, entries[i].Dummy, 21);
        write(OutFile, &entries[i].CompressedBlockSize, 4);
        write(OutFile, &entries[i].Encrypted, 1);
    }
    
    // 写入目录和文件映射表 (简化处理)
    uint64_t dir_count = 0;
    write(OutFile, &dir_count, 8);
    
    uint64_t toc_size = lseek(OutFile, 0, SEEK_CUR) - index_offset;

    // 构建并写入文件头
    PakInfo info;
    memset(&info, 0, sizeof(PakInfo));
    info.encrypted = 0x6c;
    info.magic = 0x5a434150;
    info.version = 3;
    info.offset = index_offset ^ OFFSET_KEY;
    info.size = toc_size;

    write(OutFile, &info, sizeof(PakInfo));

    free(entries);
    close(OutFile);
    printf("Repacking completed successfully.\n");
}


int main(void) {
    // 您可以更改此变量来切换模式
    // 设置为 true 来执行打包
    // 设置为 false 来执行解包
    bool repack_mode = true;

    // const char* pak_path = "../paks/map_lobby_1.33.12.14210.pak";
    const char* pak_path = "../paks/game_patch_1.33.12.14226.pak";
    const char* dat_path = "../paks/dat_temp";

    if (repack_mode) {
        // --- 打包模式 ---
        repack_pak(dat_path, pak_path);
    } else {
        // --- 解包模式 ---
        unpack_pak(pak_path, dat_path);
    }

    return 0;
}
