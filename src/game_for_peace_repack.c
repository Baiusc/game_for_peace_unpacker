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

// ========== SHA1 函数实现（从 sha1.h 复制并集成）==========
// SHA1 哈希函数所需的所有定义和函数都已集成到此文件中，无需外部依赖。

#define SHA1_BLOCK_SIZE 20

typedef struct {
    unsigned char data[64];
    unsigned int datalen;
    unsigned long long bitlen;
    unsigned int state[5];
    unsigned int k[4];
} SHA1_CTX;

#define ROTLEFT(a, b) ((a << b) | (a >> (32 - b)))

void sha1_transform(SHA1_CTX *ctx, const unsigned char data[])
{
    unsigned int a, b, c, d, e, i, j, t, m[80];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) + (data[j + 1] << 16) + (data[j + 2] << 8) + (data[j + 3]);
    for ( ; i < 80; ++i) {
        m[i] = (m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16]);
        m[i] = (m[i] << 1) | (m[i] >> 31);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 20; ++i) {
        t = ROTLEFT(a, 5) + ((b & c) ^ (~b & d)) + e + ctx->k[0] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 40; ++i) {
        t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[1] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 60; ++i) {
        t = ROTLEFT(a, 5) + ((b & c) ^ (b & d) ^ (c & d))  + e + ctx->k[2] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }
    for ( ; i < 80; ++i) {
        t = ROTLEFT(a, 5) + (b ^ c ^ d) + e + ctx->k[3] + m[i];
        e = d;
        d = c;
        c = ROTLEFT(b, 30);
        b = a;
        a = t;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

void SHA1_Init(SHA1_CTX *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xc3d2e1f0;
    ctx->k[0] = 0x5a827999;
    ctx->k[1] = 0x6ed9eba1;
    ctx->k[2] = 0x8f1bbcdc;
    ctx->k[3] = 0xca62c1d6;
}

void SHA1_Update(SHA1_CTX *ctx, const unsigned char data[], size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha1_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void SHA1_Final(unsigned char hash[], SHA1_CTX *ctx)
{
    unsigned int i;

    i = ctx->datalen;

    // Pad whatever data is left in the buffer.
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0x00;
    }
    else {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0x00;
        sha1_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    // Append to the padding the total message's length in bits and transform.
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    sha1_transform(ctx, ctx->data);

    // Since this implementation uses little endian byte ordering and MD uses big endian,
    // reverse all the bytes when copying the final state to the output hash.
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
    }
}

// 调整 SHA1_Final 的参数顺序以适应常用的库签名
// 这只是一个方便的包装器，确保与你提供的代码一致
void SHA1(const unsigned char *data, size_t size, unsigned char hash[]) {
    SHA1_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, data, size);
    SHA1_Final(hash, &ctx);
}
// ========== SHA1 函数实现结束 ==========

// 解包路径常量
#define UNPACK_PATH "../unpack/ShadowTrackerExtra_Mobile_qingxu_line"

// 用于文件索引信息混淆（加密）的密钥
// 这些密钥用于异或操作，以混淆文件偏移量和大小。
#define OFFSET_KEY 0xD74AF37FAA6B020D
#define SIZE_KEY 0x8924B0E3298B7069

// 读写和压缩的缓冲区大小
#define CHUNK_SIZE 65536

// Pak文件头结构体，位于文件末尾
// 使用 __attribute__((packed)) 确保结构体按照紧凑方式打包，没有额外的填充字节。
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
    uint8_t Dummy[21]; // 未知用途的填充数据
    uint32_t NumOfBlocks;
    CompressionBlock *blocks;
    uint32_t CompressedBlockSize;
    uint8_t Encrypted;
} __attribute__((packed)) Entry;

// 文件信息结构体，用于收集待打包的文件
typedef struct {
    char *filepath; // 文件完整路径
    char *relative_path; // 相对路径（用于在pak中的路径）
    uint64_t file_size; // 文件大小
    Entry entry; // 对应的Entry结构
} FileInfo;

// 目录信息结构体
typedef struct {
    char *dir_name; // 目录名
    FileInfo **files; // 目录下的文件指针列表
    int file_count; // 文件数量
} DirInfo;

// 全局变量
uint8_t CompressedData[CHUNK_SIZE * 2];
uint8_t DecompressedData[CHUNK_SIZE * 2];
FileInfo *all_files = NULL;
int total_files = 0;
DirInfo *directories = NULL;
int total_dirs = 0;

// 使用异或进行数据加密
void EncryptData(uint8_t *data, uint32_t size) {
    for (uint32_t index = 0; index < size; index++) {
        data[index] ^= 0x79u;
    }
}

// 使用 ZLIB 库压缩数据
unsigned int ZLIB_compress(unsigned char *InData, unsigned int InSize, unsigned char *OutData, unsigned int OutMaxSize) {
    z_stream strm;
    
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = InData;
    strm.avail_in = InSize;
    strm.next_out = OutData;
    strm.avail_out = OutMaxSize;

    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        fprintf(stderr, "Failed to initialize zlib for compression.\n");
        return 0;
    }

    if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
        fprintf(stderr, "deflate failed: %s\n", strm.msg);
        deflateEnd(&strm);
        return 0;
    }

    if (deflateEnd(&strm) != Z_OK) {
        fprintf(stderr, "deflateEnd failed: %s\n", strm.msg);
        return 0;
    }

    return strm.total_out;
}

// 计算文件的SHA1哈希
void calculate_sha1(const char *filepath, uint8_t *hash) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        // 如果文件不存在，哈希值设为0
        memset(hash, 0, 20);
        return;
    }

    SHA1_CTX sha_ctx;
    SHA1_Init(&sha_ctx);
    
    uint8_t buffer[CHUNK_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        SHA1_Update(&sha_ctx, buffer, bytes_read);
    }
    
    SHA1_Final(hash, &sha_ctx);
    fclose(file);
}

// 递归扫描目录并收集文件信息
void scan_directory(const char *base_path, const char *current_dir) {
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, current_dir);
    
    DIR *dir = opendir(full_path);
    if (!dir) {
        fprintf(stderr, "无法打开目录: %s\n", full_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char item_path[4096];
        snprintf(item_path, sizeof(item_path), "%s/%s", full_path, entry->d_name);
        
        struct stat st;
        if (stat(item_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            char new_current_dir[4096];
            if (strlen(current_dir) == 0) {
                snprintf(new_current_dir, sizeof(new_current_dir), "%s", entry->d_name);
            } else {
                snprintf(new_current_dir, sizeof(new_current_dir), "%s/%s", current_dir, entry->d_name);
            }
            scan_directory(base_path, new_current_dir);
        } else if (S_ISREG(st.st_mode)) {
            all_files = realloc(all_files, (total_files + 1) * sizeof(FileInfo));
            if (!all_files) {
                fprintf(stderr, "内存分配失败\n");
                exit(1);
            }

            FileInfo *file_info = &all_files[total_files];
            file_info->filepath = strdup(item_path);
            
            char relative_path[4096];
            if (strlen(current_dir) == 0) {
                snprintf(relative_path, sizeof(relative_path), "/%s", entry->d_name);
            } else {
                snprintf(relative_path, sizeof(relative_path), "/%s/%s", current_dir, entry->d_name);
            }
            file_info->relative_path = strdup(relative_path);
            file_info->file_size = st.st_size;

            memset(&file_info->entry, 0, sizeof(Entry));
            file_info->entry.FileSize = st.st_size;
            file_info->entry.CompressionMethod = 1; // 使用ZLIB压缩
            file_info->entry.Encrypted = 0; // 不加密文件内容
            
            calculate_sha1(item_path, file_info->entry.FileHash);
            
            total_files++;
        }
    }

    closedir(dir);
}

// 组织文件到目录结构中
// 修复了双重释放（double free）问题，将 dir_name 的内存管理推迟到 cleanup()
void organize_files_by_directory() {
    char **unique_dirs = NULL;
    int unique_dir_count = 0;

    // 第一次遍历：找出所有唯一的目录名
    for (int i = 0; i < total_files; i++) {
        char *dir_part = strdup(all_files[i].relative_path);
        char *last_slash = strrchr(dir_part, '/');
        if (last_slash) {
            *last_slash = '\0';
        } else {
            strcpy(dir_part, "");
        }

        int found = 0;
        for (int j = 0; j < unique_dir_count; j++) {
            if (strcmp(unique_dirs[j], dir_part) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            unique_dirs = realloc(unique_dirs, (unique_dir_count + 1) * sizeof(char*));
            if (!unique_dirs) {
                fprintf(stderr, "内存分配失败\n");
                exit(1);
            }
            unique_dirs[unique_dir_count] = dir_part;
            unique_dir_count++;
        } else {
            free(dir_part); // 如果找到重复的，立即释放
        }
    }

    // 第二次遍历：分配目录结构
    directories = malloc(unique_dir_count * sizeof(DirInfo));
    if (!directories) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    total_dirs = unique_dir_count;

    for (int i = 0; i < unique_dir_count; i++) {
        directories[i].dir_name = unique_dirs[i];
        directories[i].files = NULL;
        directories[i].file_count = 0;
    }
    free(unique_dirs);

    // 第三次遍历：将文件分配到对应目录
    for (int i = 0; i < total_files; i++) {
        char *dir_part = strdup(all_files[i].relative_path);
        char *last_slash = strrchr(dir_part, '/');
        if (last_slash) {
            *last_slash = '\0';
        } else {
            strcpy(dir_part, "");
        }

        for (int j = 0; j < total_dirs; j++) {
            if (strcmp(directories[j].dir_name, dir_part) == 0) {
                directories[j].files = realloc(directories[j].files, (directories[j].file_count + 1) * sizeof(FileInfo*));
                if (!directories[j].files) {
                    fprintf(stderr, "内存分配失败\n");
                    exit(1);
                }
                directories[j].files[directories[j].file_count] = &all_files[i];
                directories[j].file_count++;
                break;
            }
        }
        free(dir_part);
    }
}

// 压缩并写入文件数据
uint64_t write_file_data(int pak_fd, FileInfo *file_info, uint64_t current_offset) {
    FILE *input_file = fopen(file_info->filepath, "rb");
    if (!input_file) {
        fprintf(stderr, "无法打开文件: %s\n", file_info->filepath);
        exit(1);
    }

    file_info->entry.FileOffset = current_offset;
    uint64_t total_compressed_size = 0;
    uint32_t block_count = 0;
    
    // 计算需要的块数量
    uint64_t remaining_size = file_info->file_size;
    while (remaining_size > 0) {
        block_count++;
        remaining_size = remaining_size > CHUNK_SIZE ? remaining_size - CHUNK_SIZE : 0;
    }
    
    file_info->entry.NumOfBlocks = block_count;
    file_info->entry.blocks = malloc(block_count * sizeof(CompressionBlock));
    if (!file_info->entry.blocks) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    file_info->entry.CompressedBlockSize = CHUNK_SIZE;

    remaining_size = file_info->file_size;
    uint64_t block_offset = current_offset;
    
    for (uint32_t block = 0; block < block_count; block++) {
        size_t read_size = remaining_size > CHUNK_SIZE ? CHUNK_SIZE : remaining_size;
        
        if (fread(DecompressedData, 1, read_size, input_file) != read_size) {
            fprintf(stderr, "读取文件失败: %s\n", file_info->filepath);
            exit(1);
        }

        uint32_t compressed_size = ZLIB_compress(DecompressedData, read_size, CompressedData, sizeof(CompressedData));
        
        if (compressed_size == 0) {
            fprintf(stderr, "压缩失败: %s\n", file_info->filepath);
            exit(1);
        }

        file_info->entry.blocks[block].start = block_offset;
        file_info->entry.blocks[block].end = block_offset + compressed_size;

        if (write(pak_fd, CompressedData, compressed_size) != compressed_size) {
            fprintf(stderr, "写入压缩数据失败\n");
            exit(1);
        }

        block_offset += compressed_size;
        total_compressed_size += compressed_size;
        remaining_size -= read_size;

        printf("压缩块 %d: %s (原始大小: %zu -> 压缩后: %u 字节)\n", block, file_info->relative_path, read_size, compressed_size);
    }

    file_info->entry.CompressedLength = total_compressed_size;
    fclose(input_file);
    
    return block_offset;
}

// 写入索引数据
uint64_t write_index_data(int pak_fd, uint64_t current_offset, uint8_t **index_buffer_ptr) {
    uint8_t *index_buffer = malloc(1024 * 1024);
    if (!index_buffer) {
        fprintf(stderr, "索引缓冲区内存分配失败\n");
        exit(1);
    }
    uint64_t index_size = 0;
    uint64_t buffer_capacity = 1024 * 1024;
    
    #define WRITE_TO_BUFFER(data, size) do { \
        if (index_size + (size) > buffer_capacity) { \
            buffer_capacity *= 2; \
            index_buffer = realloc(index_buffer, buffer_capacity); \
            if (!index_buffer) { \
                fprintf(stderr, "索引缓冲区内存分配失败\n"); \
                exit(1); \
            } \
        } \
        memcpy(index_buffer + index_size, (data), (size)); \
        index_size += (size); \
    } while(0)

    // 写入挂载点信息
    // 挂载点为 ../../../ShadowTrackerExtra/
    const char *mount_prefix = "../../../";
    const char *mount_path_suffix = "ShadowTrackerExtra_Mobile_qingxu_line/";
    uint32_t mount_point_length = strlen(mount_prefix) + strlen(mount_path_suffix) + 1;
    WRITE_TO_BUFFER(&mount_point_length, 4);
    
    char full_mount_point[256];
    snprintf(full_mount_point, sizeof(full_mount_point), "%s%s", mount_prefix, mount_path_suffix);
    WRITE_TO_BUFFER(full_mount_point, mount_point_length);

    // 写入文件条目数量
    WRITE_TO_BUFFER(&total_files, 4);

    // 写入所有文件条目
    for (int i = 0; i < total_files; i++) {
        Entry *entry = &all_files[i].entry;
        
        WRITE_TO_BUFFER(entry->FileHash, 20);
        WRITE_TO_BUFFER(&entry->FileOffset, 8);
        WRITE_TO_BUFFER(&entry->FileSize, 8);
        WRITE_TO_BUFFER(&entry->CompressionMethod, 4);
        WRITE_TO_BUFFER(&entry->CompressedLength, 8);
        WRITE_TO_BUFFER(entry->Dummy, 21);

        if (entry->CompressionMethod != 0) {
            WRITE_TO_BUFFER(&entry->NumOfBlocks, 4);
            for (uint32_t j = 0; j < entry->NumOfBlocks; j++) {
                WRITE_TO_BUFFER(&entry->blocks[j].start, 8);
                WRITE_TO_BUFFER(&entry->blocks[j].end, 8);
            }
        }

        WRITE_TO_BUFFER(&entry->CompressedBlockSize, 4);
        WRITE_TO_BUFFER(&entry->Encrypted, 1);
    }

    // 写入目录映射信息
    uint64_t entries_count = total_files;
    WRITE_TO_BUFFER(&entries_count, 8);
    
    uint64_t dir_count = total_dirs;
    WRITE_TO_BUFFER(&dir_count, 8);

    // 写入每个目录的信息
    for (int i = 0; i < total_dirs; i++) {
        DirInfo *dir = &directories[i];
        
        int32_t dir_name_len = strlen(dir->dir_name) + 1;
        WRITE_TO_BUFFER(&dir_name_len, 4);
        
        char dir_name_with_slash[512];
        snprintf(dir_name_with_slash, sizeof(dir_name_with_slash), "%s/", dir->dir_name);
        WRITE_TO_BUFFER(dir_name_with_slash, dir_name_len);

        uint64_t files_in_dir = dir->file_count;
        WRITE_TO_BUFFER(&files_in_dir, 8);

        for (int j = 0; j < dir->file_count; j++) {
            FileInfo *file = dir->files[j];
            
            char *filename = strrchr(file->relative_path, '/');
            if (filename) {
                filename++;
            } else {
                filename = file->relative_path;
            }

            int32_t filename_size = strlen(filename) + 1;
            WRITE_TO_BUFFER(&filename_size, 4);
            WRITE_TO_BUFFER(filename, filename_size);

            int32_t entry_index = -1;
            for (int k = 0; k < total_files; k++) {
                if (&all_files[k] == file) {
                    entry_index = k;
                    break;
                }
            }
            WRITE_TO_BUFFER(&entry_index, 4);
        }
    }

    *index_buffer_ptr = index_buffer;
    return index_size;
}

// 清理所有动态分配的内存
void cleanup() {
    for (int i = 0; i < total_files; i++) {
        free(all_files[i].filepath);
        free(all_files[i].relative_path);
        free(all_files[i].entry.blocks);
    }
    free(all_files);

    for (int i = 0; i < total_dirs; i++) {
        free(directories[i].dir_name);
        free(directories[i].files);
    }
    free(directories);
}

int main(int argc, const char *argv[]) {
    clock_t t0 = clock();

    if (argc != 2) {
        fprintf(stderr, "用法: %s <output_pak_file>\n", argv[0]);
        return 1;
    }

    printf("扫描目录: %s\n", UNPACK_PATH);
    scan_directory(UNPACK_PATH, "");
    
    if (total_files == 0) {
        fprintf(stderr, "未找到要打包的文件\n");
        return 1;
    }

    printf("找到 %d 个文件需要打包\n", total_files);

    organize_files_by_directory();
    printf("组织到 %d 个目录中\n", total_dirs);

    int pak_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (pak_fd == -1) {
        fprintf(stderr, "无法创建输出文件: %s\n", argv[1]);
        cleanup();
        return 1;
    }

    uint64_t current_offset = 0;
    
    printf("正在压缩并写入文件数据...\n");
    for (int i = 0; i < total_files; i++) {
        current_offset = write_file_data(pak_fd, &all_files[i], current_offset);
    }

    printf("文件数据写入完成，开始写入索引数据...\n");

    uint8_t *index_buffer = NULL;
    uint64_t index_offset = lseek(pak_fd, 0, SEEK_CUR);
    uint64_t index_size = write_index_data(pak_fd, current_offset, &index_buffer);
    
    if (write(pak_fd, index_buffer, index_size) != index_size) {
        fprintf(stderr, "写入索引数据失败\n");
        free(index_buffer);
        cleanup();
        close(pak_fd);
        return 1;
    }

    printf("索引数据写入完成\n");

    uint8_t index_hash[20];
    SHA1_CTX sha_ctx;
    SHA1_Init(&sha_ctx);
    SHA1_Update(&sha_ctx, index_buffer, index_size);
    SHA1_Final(index_hash, &sha_ctx);

    PakInfo info;
    info.encrypted = 0;
    info.encrypted ^= 0x6C; // 异或后为1，表示索引未加密
    info.magic = 0x5a6f12e1;
    info.version = 1;
    memcpy(info.hash, index_hash, 20);
    info.size = index_size;
    info.offset = index_offset;
    info.offset ^= OFFSET_KEY;

    if (write(pak_fd, &info, sizeof(PakInfo)) != sizeof(PakInfo)) {
        fprintf(stderr, "写入pak文件头失败\n");
        free(index_buffer);
        cleanup();
        close(pak_fd);
        return 1;
    }
    
    printf("成功打包 %s。总文件数: %d\n", argv[1], total_files);

    free(index_buffer);
    cleanup();
    close(pak_fd);
    
    clock_t t1 = clock();
    double time = ((double)(t1 - t0)) / CLOCKS_PER_SEC;
    printf("打包完成，耗时 %f 秒\n", time);

    return 0;
}
