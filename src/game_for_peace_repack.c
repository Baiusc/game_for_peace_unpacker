/*
 * @Description  : 找到并替换src_pak中最后两个实例为my_pak中两个新实例，并生成一个新的pak文件
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>

// ----------------------------------------------------------------------
// 结构体和宏定义
// ----------------------------------------------------------------------

#define OFFSET_KEY 0xD74AF37FAA6B020D
#define CHUNK_SIZE 65536
#define AES_KEY_SIZE 16

// 占位符密钥
static const uint8_t PAK_AES_KEY[AES_KEY_SIZE] = {
    0x5F, 0x1A, 0xE2, 0x3D, 0x4F, 0x6B, 0x8C, 0x9D,
    0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18
};

// Pak文件头结构体，位于文件末尾
typedef struct {
    uint8_t encrypted;  // 索引是否加密，0x6C 异或后得到 0 或 1
    uint32_t magic;     // 魔数，用于文件格式标识
    uint32_t version;   // 文件版本号
    uint8_t hash[20];   // 索引数据的 SHA1 哈希值
    uint64_t size;      // 索引数据的大小 (本代码未使用)
    uint64_t offset;    // 索引数据在文件中的偏移量
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
    // 索引辅助字段
    uint64_t EntryIndexStart;  // 索引的起始偏移，以索引区开始处（挂载点）为零点
    uint64_t EntryIndexEnd; // 索引的结束偏移，以索引区开始处（挂载点）为零点
} __attribute__((packed)) Entry;

// 文件路径和索引信息结构
typedef struct {
    uint32_t FilenameSize; // 文件名大小（正值表示UTF-8，负值表示UTF-16）
    char Filename[1024]; // 文件名，支持UTF-8和UTF-16
    uint32_t EntryIndex; // 当前路径条目关联的 Entry 索引
    Entry *entry_ptr; // 指向对应的 Entry 结构体
    char *DirPath; // 路径
    uint32_t DirLen; // 路径长度
    uint64_t DirFiles; // 当前目录下的文件数量
    uint64_t PathIndexStart; // 当前 文件 的起始偏移，以索引区开始处（挂载点）为零点
    uint64_t PathIndexEnd; // 当前 文件 的结束偏移，以索引区开始处（挂载点）为零点
    uint64_t DirFilesPtrOffset; // DirFiles 字段在索引区中的偏移位置，以索引区开始处（挂载点）为零点
    uint64_t EntryIndexStart; // 当前 Entry 在索引区中的起始偏移位置，以索引区开始处（挂载点）为零点
    uint64_t EntryIndexEnd; // 当前 Entry 在索引区中的结束偏移位置，以索引区开始处（挂载点）为零点
    uint64_t DirStartOffset; // 当前 文件夹 的起始偏移，以索引区开始处（挂载点）为零点
} FileInstance;

// 新增：单个文件在 Directory Map 中的路径映射信息结构体
typedef struct {

    // 1. 目录信息(head部分，size=4+dir_len+8)
    uint32_t dir_len;              // 原始目录名长度 (4B)
    char dir_path_raw[1024];       // 原始目录名数据 (用于精确序列化)
    uint64_t dir_files;           // 当前目录包含的文件数量 (8B)

    // 2. 文件名和索引信息(body部分，size=4+filename_size+4)
    uint32_t filename_size;        // 原始文件名长度/编码标识 (4B)
    char filename_raw[1024 * 2];  // 原始文件名数据 (用于序列化，最大支持 Unicode/UTF-16LE)
    uint32_t entry_index;          // 关联的 Entry 索引 (4B)
    
    // 序列化辅助字段 (完整块)
    uint8_t *dir_bin;               // 序列化后的完整 DirMap Entry 数据块
    size_t dir_bin_size;             // 序列化后的数据块大小

    // 序列化辅助字段 (Head/Body 分割)
    uint8_t *dir_bin_head;          // 序列化后的 DirMap 目录信息部分
    size_t dir_bin_head_size;        // DirMap 目录信息部分大小
    uint8_t *dir_bin_body;          // 序列化后的 DirMap 文件条目部分
    size_t dir_bin_body_size;        // DirMap 文件条目部分大小

    // 索引偏移字段
    uint64_t dir_start_offset;     // 当前 文件夹 的起始偏移，以索引区开始处（挂载点）为零点
    uint64_t path_index_start;     // 当前 文件 的起始偏移，以索引区开始处（挂载点）为零点
    uint64_t path_index_end;       // 当前 文件 的结束偏移，以索引区开始处（挂载点）为零点

    // DirMap 全局头部信息 (ENTRIES, DIR_COUNT)
    uint64_t global_entries;
    uint64_t global_dir_count;
} DirMapEntry;

// 新实例数据结构
typedef struct {
    Entry entry; // 从索引区解析的索引信息
    char Filename[1024];
    char DirPath[1024];
    uint8_t *Head;      // 头 （内含的信息与entry等效）（大小为 94 字节）
    size_t HeadSize;   // 头大小为 94 字节
    uint8_t *Data;      // 压缩块
    uint64_t DataSize;  // 压缩块大小
    CompressionBlock *blocks; // 压缩块起止偏移量
    // 新增：Directory Map 相关的序列化信息
    DirMapEntry dir_map; 
} NewInstance;

// 完整的解析结果结构
typedef struct {
    PakInfo info;
    char MountPoint[1024];
    uint32_t NumOfEntry;
    Entry *FileEntries;
    FileInstance *AllFileInstances;
    uint32_t NumOfInstances;
    uint8_t *OriginalIndexData; // 索引区的起始位置，以文件开头为零点
    int64_t OriginalIndexSize; // 索引数据的字节大小
    uint64_t EntryListEndOffset; // 索引区的实体列表的结束位置，以索引区开始处（挂载点）为零点
    uint64_t DirMapStartOffset; // 索引区的路径列表的开始位置，以索引区开始处（挂载点）为零点
    uint64_t DirMapEndOffset; // 索引区的路径列表的结束位置，以索引区开始处（挂载点）为零点
    // 新增：DirMap 全局头部信息
    uint64_t dir_map_global_entries; // ENTRIES 总实体数量
    uint64_t dir_map_global_dir_count; // DIR_COUNT 总路径数量
} PakIndexData;

// ----------------------------------------------------------------------
// 辅助函数 (保持不变)
// ----------------------------------------------------------------------
// 🌟 占位符函数：此函数不再执行实际的 AES 解密操作
void DecryptData(uint8_t *data, uint32_t size)
{
    // 避免未使用的变量警告
    (void)data;
    (void)size;
}

int unicode_to_utf8(const char *input, size_t input_len, char *out, size_t output_len) {
    int j = 0;
    for (size_t i = 0; i < input_len && j < output_len - 1; i += 2) {
        uint16_t wc = *(uint16_t*)(input + i);
        if (wc < 0x80) {
            out[j++] = (char)wc;
        } else if (wc < 0x800) {
            if (j + 2 >= output_len) break;
            out[j++] = (wc >> 6) | 0xC0;
            out[j++] = (wc & 0x3F) | 0x80;
        } else {
            if (j + 3 >= output_len) break;
            out[j++] = (wc >> 12) | 0xE0;
            out[j++] = ((wc >> 6) & 0x3F) | 0x80;
            out[j++] = (wc & 0x3F) | 0x80;
        }
    }
    out[j] = '\0';
    return j;
}
// ----------------------------------------------------------------------
// 读取/写入工具函数 (保持不变)
// ----------------------------------------------------------------------

uint64_t current_index_offset = 0;

void read_data(void *destination, const uint8_t *source, size_t length) {
    memcpy(destination, source + current_index_offset, length);
    current_index_offset += length;
}

void write_data(uint8_t *buffer, uint64_t *offset, const void *src, size_t len) {
    memcpy(buffer + *offset, src, len);
    *offset += len;
}

// ----------------------------------------------------------------------
// 读取文件数据（含解密 + 拼接压缩块）
// ----------------------------------------------------------------------

// 调试函数：打印内存中的数据
void print_hex_dump(const uint8_t *data, size_t size) {
    size_t len = (size > 64) ? 64 : size; // 只打印前64字节
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        } else if ((i + 1) % 8 == 0) {
            printf("  "); // 在8字节处添加额外空格
        }
    }
    if (len > 0) printf("\n");
}

// 辅助函数：将 DirMap Entry 路径条目序列化成字节流
int SerializeDirMap(DirMapEntry *dir_map) {
    // raw_name_len 用于处理文件名长度的负数逻辑（表示 UTF-16LE 编码）
    int raw_name_len_int = (int32_t)dir_map->filename_size;
    int raw_name_len = (raw_name_len_int > 0) ? 
                       raw_name_len_int : 
                       -raw_name_len_int * 2;
    
    // 检查是否有文件名数据需要序列化
    if (raw_name_len < 0) {
        fprintf(stderr, "Error: Invalid raw filename length during DirMap serialization.\n");
        return -1;
    }
    
    // --- 1. 计算 Head 和 Body 的大小并赋值 ---
    
    // Head Size: [DirLen: 4B] + [DirPathRaw: DirLen] + [DirFiles: 8B]
    dir_map->dir_bin_head_size = sizeof(uint32_t) + dir_map->dir_len + sizeof(uint64_t);

    // Body Size: [FilenameSize: 4B] + [FilenameRaw: variable] + [EntryIndex: 4B]
    dir_map->dir_bin_body_size = sizeof(uint32_t) + raw_name_len + sizeof(uint32_t);

    // 整体数据块大小
    dir_map->dir_bin_size = dir_map->dir_bin_head_size + dir_map->dir_bin_body_size;
    
    // --- 2. 内存分配 ---
    // 为三个数据块分配内存
    dir_map->dir_bin = (uint8_t*)malloc(dir_map->dir_bin_size);
    dir_map->dir_bin_head = (uint8_t*)malloc(dir_map->dir_bin_head_size);
    dir_map->dir_bin_body = (uint8_t*)malloc(dir_map->dir_bin_body_size);

    if (!dir_map->dir_bin || !dir_map->dir_bin_head || !dir_map->dir_bin_body) {
        fprintf(stderr, "Failed to allocate memory for DirMap serialization parts.\n");
        // 清理已分配的内存
        if (dir_map->dir_bin) free(dir_map->dir_bin);
        if (dir_map->dir_bin_head) free(dir_map->dir_bin_head);
        if (dir_map->dir_bin_body) free(dir_map->dir_bin_body);
        dir_map->dir_bin = NULL;
        dir_map->dir_bin_head = NULL;
        dir_map->dir_bin_body = NULL;
        // 重置大小字段
        dir_map->dir_bin_size = dir_map->dir_bin_head_size = dir_map->dir_bin_body_size = 0;
        return -1;
    }
    
    uint64_t current_offset_total = 0;
    uint64_t current_offset_head = 0;
    uint64_t current_offset_body = 0;
    
    // --- 3. 写入 Head 部分 (目录信息) 到 dir_bin_head ---
    
    // 1. 写入 dir_len (4B)
    write_data(dir_map->dir_bin_head, &current_offset_head, &dir_map->dir_len, sizeof(uint32_t));
    
    // 2. 写入 dir_path_raw (variable)
    write_data(dir_map->dir_bin_head, &current_offset_head, dir_map->dir_path_raw, dir_map->dir_len);
    
    // 3. 写入 dir_files (8B)
    write_data(dir_map->dir_bin_head, &current_offset_head, &dir_map->dir_files, sizeof(uint64_t));

    // 检查 Head 大小是否匹配
    if (current_offset_head != dir_map->dir_bin_head_size) {
        fprintf(stderr, "Serialization size mismatch for DirMap Head. Expected %zu, got %llu.\n",
                dir_map->dir_bin_head_size, (unsigned long long)current_offset_head);
        // 清理所有分配的内存
        if (dir_map->dir_bin) free(dir_map->dir_bin);
        if (dir_map->dir_bin_head) free(dir_map->dir_bin_head);
        if (dir_map->dir_bin_body) free(dir_map->dir_bin_body);
        return -1;
    }

    // --- 4. 写入 Body 部分 (文件条目信息) 到 dir_bin_body ---

    // 4. 写入 filename_size (4B)
    write_data(dir_map->dir_bin_body, &current_offset_body, &dir_map->filename_size, sizeof(uint32_t));
    
    // 5. 写入 filename_raw (variable)
    write_data(dir_map->dir_bin_body, &current_offset_body, dir_map->filename_raw, raw_name_len);
    
    // 6. 写入 entry_index (4B)
    write_data(dir_map->dir_bin_body, &current_offset_body, &dir_map->entry_index, sizeof(uint32_t));
    
    // 检查 Body 大小是否匹配
    if (current_offset_body != dir_map->dir_bin_body_size) {
        fprintf(stderr, "Serialization size mismatch for DirMap Body. Expected %zu, got %llu.\n",
                dir_map->dir_bin_body_size, (unsigned long long)current_offset_body);
        // 清理所有分配的内存
        if (dir_map->dir_bin) free(dir_map->dir_bin);
        if (dir_map->dir_bin_head) free(dir_map->dir_bin_head);
        if (dir_map->dir_bin_body) free(dir_map->dir_bin_body);
        return -1;
    }
    
    // --- 5. 拼接完整数据块 dir_bin ---
    
    // 写入 Head
    write_data(dir_map->dir_bin, &current_offset_total, dir_map->dir_bin_head, dir_map->dir_bin_head_size);
    
    // 写入 Body
    write_data(dir_map->dir_bin, &current_offset_total, dir_map->dir_bin_body, dir_map->dir_bin_body_size);
    
    // 检查 Total 大小是否匹配
    if (current_offset_total != dir_map->dir_bin_size) {
        fprintf(stderr, "Serialization size mismatch for DirMap Total. Expected %zu, got %llu.\n",
                dir_map->dir_bin_size, (unsigned long long)current_offset_total);
        // 清理所有分配的内存
        if (dir_map->dir_bin) free(dir_map->dir_bin);
        if (dir_map->dir_bin_head) free(dir_map->dir_bin_head);
        if (dir_map->dir_bin_body) free(dir_map->dir_bin_body);
        return -1;
    }
    
    return 0;
}

// ----------------------------------------------------------------------
// 核心函数：根据 FileInstance 构造 NewInstance 中的 DirMapEntry
// ----------------------------------------------------------------------
int computeDirMap(const PakIndexData *pak_data, const FileInstance *inst, NewInstance *ni) {
    DirMapEntry *dm = &ni->dir_map;
    
    // 1. 填充全局头部信息
    dm->global_entries = pak_data->dir_map_global_entries;
    dm->global_dir_count = pak_data->dir_map_global_dir_count;
    
    // 2. 填充目录信息
    dm->dir_len = inst->DirLen;
    // 复制目录名原始字节数据
    strncpy(dm->dir_path_raw, inst->DirPath, inst->DirLen);
    dm->dir_path_raw[inst->DirLen] = '\0'; 
    
    // 3.填充当前目录下的文件数量 (DirFiles)
    dm->dir_files = inst->DirFiles;

    // 4. 填充文件名和关联索引信息
    dm->filename_size = inst->FilenameSize;
    dm->entry_index = inst->EntryIndex;
    dm->dir_start_offset = inst->DirStartOffset;
    dm->path_index_start = inst->PathIndexStart;
    dm->path_index_end = inst->PathIndexEnd;

    // 5. 提取 FilenameRaw 字段
    uint8_t *raw_path_ptr = pak_data->OriginalIndexData + inst->PathIndexStart;
    
    int raw_name_len = (dm->filename_size > 0) ? dm->filename_size : -dm->filename_size * 2;
    if (raw_name_len > 0) {
        // FilenameRaw Start Offset = raw_path_ptr + 4 (跳过 FilenameSize)
        // 确保复制不会超出 FilenameRaw 缓冲区
        size_t copy_len = raw_name_len < sizeof(dm->filename_raw) ? raw_name_len : sizeof(dm->filename_raw);
        memcpy(dm->filename_raw, raw_path_ptr + 4, copy_len);
    }
    
    // 6. 序列化 DirMap Entry 路径条目部分
    return SerializeDirMap(dm);
}

// 新增：用于将 Entry 结构体的关键字段序列化为 94 字节的头部
void SerializeHeadData(Entry *entry, uint8_t **out_head, size_t *out_size) {
    // 确保分配了 94 字节的内存
    *out_head = (uint8_t*)malloc(94);
    if (!*out_head) {
        fprintf(stderr, "Error: Failed to allocate memory for 94-byte head.\n");
        return;
    }

    uint8_t *ptr = *out_head;
    
    // 1. 写入 FileHash (20B)
    memcpy(ptr, entry->FileHash, 20); ptr += 20;
    
    // 2. 写入 FileOffset (8B)
    memcpy(ptr, &entry->FileOffset, 8); ptr += 8;
    
    // 3. 写入 FileSize (8B)
    memcpy(ptr, &entry->FileSize, 8); ptr += 8;
    
    // 4. 写入 CompressionMethod (4B)
    memcpy(ptr, &entry->CompressionMethod, 4); ptr += 4;
    
    // 5. 写入 CompressedLength (8B)
    memcpy(ptr, &entry->CompressedLength, 8); ptr += 8; 
    
    // 6. 写入 Dummy (21B)
    memcpy(ptr, entry->Dummy, 21); ptr += 21;
    
    // 7. 写入 NumOfBlocks (4B)
    memcpy(ptr, &entry->NumOfBlocks, 4); ptr += 4;

    // 8. 写入 第一个 CompressionStart 和 End (8+8=16B)
    for (uint32_t i = 0; i < entry->NumOfBlocks; i++) {
        memcpy(ptr, &entry->blocks[i].start, 8); ptr += 8;
        memcpy(ptr, &entry->blocks[i].end, 8); ptr += 8;
    }

    // 9. 写入 CompressedBlockSize (4B)
    memcpy(ptr, &entry->CompressedBlockSize, 4); ptr += 4;
    
    // 10. 写入 Encrypted (1B)
    memcpy(ptr, &entry->Encrypted, 1); ptr += 1;
    
    // 验证总大小
    *out_size = ptr - *out_head;
    if (*out_size != 94) {
        fprintf(stderr, "Fatal Error: Head serialization size mismatch. Expected 94, got %zu.\n", *out_size);
    }
}

// ----------------------------------------------------------------------
// 读取文件数据（仅处理 NumOfBlocks == 1 的特定结构）
// ----------------------------------------------------------------------
int get_data_CompressedLength(int fd, Entry *entry, uint8_t **out_data, uint64_t *out_size) {
    
    // 定义头部大小
    const uint64_t HEAD_SIZE = 94;
    // 1. 检查条件：必须是 NumOfBlocks == 1 且 CompressedLength > 0
    if (entry->NumOfBlocks != 1 || entry->CompressedLength <= 0)
    {
        // 暂不处理的情况
        *out_data = NULL;
        *out_size = 0;
        printf("错误: 不支持的block。\n");
        return -1;
    }
    // 压缩数据流的绝对起始偏移
    uint64_t block_start = entry->FileOffset + HEAD_SIZE;
    // 压缩数据流的实际大小
    uint64_t block_size = entry->CompressedLength;
    // 读取数据
    *out_size = block_size;
    *out_data = malloc(*out_size);
    if (!*out_data) {
        fprintf(stderr, "Error: Failed to allocate memory for block size %llu.\n", (unsigned long long)block_size);
        return -1;
    }
    // 读取数据
    if (pread(fd, *out_data, *out_size, block_start) != *out_size) {
        fprintf(stderr, "Error: get_data_CompressedLength failed to read %llu bytes from offset 0x%llx (FileOffset + 94).\n", 
                (unsigned long long)*out_size, (unsigned long long)block_start);
        free(*out_data);
        return -1;
    }
    if (entry->Encrypted) {
        DecryptData(*out_data, *out_size);
        printf("警告: 进入DecryptData。\n");
    }
    
    // --- 调试打印 START ---
    printf("DEBUG: 读取单块压缩文件（跳过 94 字节头部）完成。\n");
    printf("DEBUG: 压缩块大小 CompressedLength: %llu\n", (unsigned long long)entry->CompressedLength);
    printf("DEBUG: 实际数据流大小: %llu bytes (0x%llx)\n", (unsigned long long)*out_size, (unsigned long long)*out_size);
    printf("DEBUG: 数据（前64字节）:\n");
    print_hex_dump(*out_data, *out_size);
    // --- 调试打印 END ---

    return 0;
}

// ----------------------------------------------------------------------
// 核心辅助函数：PAK 索引解析
// ----------------------------------------------------------------------

int ParsePakIndex(int PakFileDescriptor, PakIndexData *Result) {
    // 移动到文件尾部 45 字节处，读取 PakInfo
    if (lseek(PakFileDescriptor, -45, SEEK_END) == -1 || read(PakFileDescriptor, &Result->info, 45) != 45) {
        fprintf(stderr, "Failed to read pak header at -45\n");
        COND_RETURN: return 1;
    }

    // 解密/异或 PakInfo 中的关键字段
    uint64_t OriginalIndexOffset = Result->info.offset ^ OFFSET_KEY;
    uint8_t EncryptedFlag = Result->info.encrypted ^ 0x6C;

    // 计算索引数据大小
    off_t total_size = lseek(PakFileDescriptor, 0, SEEK_END);
    Result->OriginalIndexSize = total_size - OriginalIndexOffset - 45;

    if (Result->OriginalIndexSize < 0 || Result->OriginalIndexSize > 100 * 1024 * 1024) {
        fprintf(stderr, "Index data size is not compatible or offset error.\n");
        goto COND_RETURN;
    }

    // 分配内存并读取整个索引数据
    Result->OriginalIndexData = (uint8_t*)malloc(Result->OriginalIndexSize);
    if (!Result->OriginalIndexData) {
        fprintf(stderr, "Memory allocation failed for IndexData.\n");
        goto COND_RETURN;
    }

    if (pread(PakFileDescriptor, Result->OriginalIndexData, Result->OriginalIndexSize, OriginalIndexOffset) != Result->OriginalIndexSize) {
        fprintf(stderr, "Failed to load index data\n");
        free(Result->OriginalIndexData); Result->OriginalIndexData = NULL;
        goto COND_RETURN;
    }

    // 如果设置了加密标志，对整个索引数据进行解密
    if (EncryptedFlag) {
        DecryptData(Result->OriginalIndexData, Result->OriginalIndexSize);
    }

    current_index_offset = 0; // 以索引数据开头为零点

    // 读取 MountPoint (挂载点)
    uint32_t MountPointLength;
    read_data(&MountPointLength, Result->OriginalIndexData, 4);
    read_data(Result->MountPoint, Result->OriginalIndexData, MountPointLength);
    Result->MountPoint[MountPointLength] = '\0';

    // 读取文件条目总数
    read_data(&Result->NumOfEntry, Result->OriginalIndexData, 4);
    Result->NumOfInstances = Result->NumOfEntry;

    Result->FileEntries = (Entry*)calloc(Result->NumOfEntry, sizeof(Entry));
    if (!Result->FileEntries) goto COND_RETURN;
    
    // 7. 解析 File Entry 列表 (包含条件读取 NumOfBlocks 的修正逻辑)
    for (uint32_t i = 0; i < Result->NumOfEntry; i++) {
        Entry *e = &Result->FileEntries[i];
        uint64_t entry_start_offset = current_index_offset;
        
        // 1-6. 读取 Entry 的前六个固定字段
        read_data(e->FileHash, Result->OriginalIndexData, 20);
        read_data(&e->FileOffset, Result->OriginalIndexData, 8);
        read_data(&e->FileSize, Result->OriginalIndexData, 8);
        read_data(&e->CompressionMethod, Result->OriginalIndexData, 4);
        read_data(&e->CompressedLength, Result->OriginalIndexData, 8);
        read_data(e->Dummy, Result->OriginalIndexData, 21);
        
        // 7. NumOfBlocks 和块数据读取：仅在存在压缩（CompressionMethod != 0）时读取 NumOfBlocks
        if (e->CompressionMethod != 0)
        {
            // 如果存在压缩（多块），则数据中包含 NumOfBlocks
            read_data(&e->NumOfBlocks, Result->OriginalIndexData, 4);
            
            uint32_t blocks_size = e->NumOfBlocks * sizeof(CompressionBlock);
            e->blocks = (CompressionBlock*)malloc(blocks_size);
            if (!e->blocks) goto COND_RETURN; 
            
            // 读取所有块信息
            read_data(e->blocks, Result->OriginalIndexData, blocks_size);
        }
        else
        {
            // 如果 CompressionMethod == 0 (单块/未压缩)，NumOfBlocks 不存在于数据流中
            e->NumOfBlocks = 0;
            e->blocks = NULL;
        }

        // 8. CompressedBlockSize (4 字节)
        read_data(&e->CompressedBlockSize, Result->OriginalIndexData, 4);
        
        // 9. Encrypted (1 字节)
        read_data(&e->Encrypted, Result->OriginalIndexData, 1);

        e->EntryIndexStart = entry_start_offset;
        e->EntryIndexEnd = current_index_offset;
    }

    Result->EntryListEndOffset = current_index_offset;

    // --- Directory Map 解析 ---

    Result->DirMapStartOffset = current_index_offset;
    uint64_t ENTRIES, DIR_COUNT; // 目录地图的头部信息
    read_data(&ENTRIES, Result->OriginalIndexData, 8);
    read_data(&DIR_COUNT, Result->OriginalIndexData, 8);

    Result->AllFileInstances = (FileInstance*)calloc(Result->NumOfEntry, sizeof(FileInstance));
    uint32_t InstanceCounter = 0;

    // 遍历所有目录
    for (uint64_t dir_idx = 0; dir_idx < DIR_COUNT; dir_idx++) {
        if(dir_idx == 158)
        {
            int debug = 1; // DEBUG
        }
        uint64_t dir_start_offset = current_index_offset;
        int32_t DIR_LEN; char DIR_NAME[1024]; uint64_t DIR_FILES;

        read_data(&DIR_LEN, Result->OriginalIndexData, 4);
        read_data(DIR_NAME, Result->OriginalIndexData, DIR_LEN); DIR_NAME[DIR_LEN] = '\0';
        uint64_t dir_files_offset = current_index_offset;
        read_data(&DIR_FILES, Result->OriginalIndexData, 8); // 当前目录包含的文件数量

        // 遍历当前目录下的所有文件实例
        for (uint64_t x = 0; x < DIR_FILES; x++) {
            if (InstanceCounter >= Result->NumOfEntry) break;

            uint64_t path_start_offset = current_index_offset;
            int32_t FilenameSize; char FilenameBuffer[1024]; int32_t ENTRY_Index;

            read_data(&FilenameSize, Result->OriginalIndexData, 4); // 文件名长度
            int name_len = FilenameSize > 0 ? FilenameSize : -FilenameSize * 2;
            read_data(FilenameBuffer, Result->OriginalIndexData, name_len); // 文件名数据

            // 处理 Unicode 或 UTF-8 编码
            if (FilenameSize < 0) {
                unicode_to_utf8(FilenameBuffer, name_len, FilenameBuffer, sizeof(FilenameBuffer));
            } else {
                FilenameBuffer[name_len] = '\0';
            }

            read_data(&ENTRY_Index, Result->OriginalIndexData, 4); // 关联的 Entry 索引
            uint64_t path_end_offset = current_index_offset;

            // 存储文件实例信息
            FileInstance *inst = &Result->AllFileInstances[InstanceCounter];
            inst->EntryIndex = ENTRY_Index;
            inst->entry_ptr = &Result->FileEntries[ENTRY_Index];
            inst->FilenameSize = FilenameSize;
            strncpy(inst->Filename, FilenameBuffer, 1024);
            inst->DirLen = DIR_LEN;
            inst->DirPath = strdup(DIR_NAME);
            inst->DirFiles = DIR_FILES;
            inst->DirFilesPtrOffset = dir_files_offset;
            inst->PathIndexStart = path_start_offset;
            inst->PathIndexEnd = path_end_offset;
            inst->DirStartOffset = dir_start_offset;
            inst->EntryIndexStart = Result->FileEntries[ENTRY_Index].EntryIndexStart;
            inst->EntryIndexEnd = Result->FileEntries[ENTRY_Index].EntryIndexEnd;

            InstanceCounter++;
        }
    }
    Result->DirMapEndOffset = current_index_offset;

    if (InstanceCounter != Result->NumOfEntry) {
        fprintf(stderr, "Warning: Instance count (%u) does not match Entry count (%u).\n", InstanceCounter, Result->NumOfEntry);
    }

    return 0;
}

// ----------------------------------------------------------------------
// 资源清理函数
// ----------------------------------------------------------------------
void CleanupPakData(PakIndexData *data) {
    if (data->OriginalIndexData) free(data->OriginalIndexData);
    if (data->FileEntries) {
        for (uint32_t i = 0; i < data->NumOfEntry; i++) {
            if (data->FileEntries[i].blocks) free(data->FileEntries[i].blocks);
        }
        free(data->FileEntries);
    }
    if (data->AllFileInstances) {
        for (uint32_t i = 0; i < data->NumOfInstances; i++) {
            if (data->AllFileInstances[i].DirPath) free(data->AllFileInstances[i].DirPath);
        }
        free(data->AllFileInstances);
    }
    memset(data, 0, sizeof(PakIndexData));
}

// ----------------------------------------------------------------------
// 核心逻辑：数据块复制和替换
// ----------------------------------------------------------------------

// 查找最后一个文件实例的实际偏移
uint64_t find_last_file_offset(PakIndexData *data, uint64_t *out_size) {
    if (data->NumOfEntry == 0) return 0;
    uint64_t max_offset = 0;
    uint64_t max_offset_end = 0;

    for (uint32_t i = 0; i < data->NumOfEntry; i++) {
        Entry *e = &data->FileEntries[i];
        uint64_t end_offset = e->FileOffset + e->CompressedLength;
        
        // 忽略 FileOffset 为 0 的条目
        if (e->FileOffset > 0 && end_offset > max_offset_end) {
            max_offset_end = end_offset;
            max_offset = e->FileOffset;
        }
    }

    // 假设索引中 FileOffset 最大的那个就是数据体中的最后一个文件
    // ⚠️ 警告: 实际的 PAK 文件可能不是按 FileOffset 顺序排列的。
    // 但是，为了简单起见，我们假设最大的 FileOffset+CompressedLength 对应了最后一个数据块的末尾。
    *out_size = max_offset_end;
    return max_offset;
}

// ----------------------------------------------------------------------
// 调试辅助函数：验证内存中的新索引数据
// 注意: 此函数需要访问全局的 unicode_to_utf8 函数定义
// ----------------------------------------------------------------------
void VerifyNewIndexData(const uint8_t *NewIndexData, uint64_t NewIndexDataSize, 
                        uint32_t NumOfEntry, uint64_t offset_add) {
    printf("\n\n--- 🌟 调试验证：解析内存中的新索引数据 🌟 ---\n");
    
    // 临时缓冲区用于存储和打印目录名和文件名
    char temp_buffer[1024] = {0};

    // 索引头（MountPoint + NumOfEntry）是正确的，直接跳过
    uint64_t verify_offset = 0;
    
    // 1. 跳过 MountPoint (从 NewIndexData 头部开始读取)
    uint32_t MountPointLength;

    memcpy(&MountPointLength, NewIndexData + verify_offset, 4);
    verify_offset += 4;
    
    // 打印 MountPoint Length
    printf("MountPoint Length: %u\n", MountPointLength);
    verify_offset += MountPointLength; // 跳过 MountPoint
    
    // 2. 打印 NumOfEntry
    uint32_t read_NumOfEntry = 0;
    memcpy(&read_NumOfEntry, NewIndexData + verify_offset, 4);
    verify_offset += 4; // 跳过 NumOfEntry
    
    if (read_NumOfEntry != NumOfEntry) {
        printf("🚨 致命错误：验证时发现 Entry 数量不匹配！ (预期: %u, 读取: %u)\n", NumOfEntry, read_NumOfEntry);
    } else {
        printf("Entry 数量: %u (匹配)\n", read_NumOfEntry);
    }

    // 3. 遍历并验证 Entry 列表的字节流 (保持不变，用于定位 DirMap 起始位置)
    for (uint32_t i = 0; i < read_NumOfEntry; i++) {
        if(i>=429)
        {
            int debug_break=1;
        }
        // Entry 结构体字段（只关注 FileHash 和 FileOffset）
        uint8_t FileHash[20];
        uint64_t FileOffset;
        uint64_t FileSize;
        uint32_t CompressionMethod;

        // 验证 FileHash (20 字节)
        if (verify_offset + 20 > NewIndexDataSize) break;
        memcpy(FileHash, NewIndexData + verify_offset, 20);
        verify_offset += 20;

        // 验证 FileOffset (8 字节)
        if (verify_offset + 8 > NewIndexDataSize) break;
        memcpy(&FileOffset, NewIndexData + verify_offset, 8);
        verify_offset += 8;
        
        // 验证 FileSize (8 字节)
        if (verify_offset + 8 > NewIndexDataSize) break;
        memcpy(&FileSize, NewIndexData + verify_offset, 8);
        verify_offset += 8;

        // 验证 CompressionMethod (4 字节)
        if (verify_offset + 4 > NewIndexDataSize) break;
        memcpy(&CompressionMethod, NewIndexData + verify_offset, 4);
        verify_offset += 4;
        
        // --- 精简打印，仅在关键位置打印，避免日志过长 ---
        if (i < 5 || i >= read_NumOfEntry - 5) {
            printf("--- Entry #%u ---\n", i);
            printf("  FileHash: %02x%02x...%02x\n", FileHash[0], FileHash[1], FileHash[19]);
            printf("  FileOffset: 0x%llx\n", (unsigned long long)FileOffset);
            // >>> 新增：打印 FileSize
            printf("  FileSize: %llu bytes (0x%llx)\n", 
                   (unsigned long long)FileSize, 
                   (unsigned long long)FileSize);
        }
        
        // 跳过剩余的固定字段 (CompressedLength: 8B, Dummy: 21B)
        if (verify_offset + 8 + 21 > NewIndexDataSize) break;
        verify_offset += 8 + 21; 

        // 处理 NumOfBlocks 和 Blocks Data
        if (CompressionMethod != 0) {
            uint32_t NumOfBlocks = 0;
            if (verify_offset + 4 > NewIndexDataSize) break;
            memcpy(&NumOfBlocks, NewIndexData + verify_offset, 4);
            verify_offset += 4;
            
            // 跳过所有 CompressionBlock (NumOfBlocks * 16 字节)
            uint64_t blocks_size = (uint64_t)NumOfBlocks * 16;
            if (verify_offset + blocks_size > NewIndexDataSize) break;
            verify_offset += blocks_size;
        }

        // 跳过 CompressedBlockSize (4B) 和 Encrypted (1B)
        if (verify_offset + 4 + 1 > NewIndexDataSize) break;
        verify_offset += 4 + 1;
    }
    
    printf("--- Entry 列表验证完成。当前偏移 (DirMapStart): 0x%llx ---\n", (unsigned long long)verify_offset);

    // 4. 验证 Directory Map
    printf("\n--- 🌐 验证 Directory Map 结构 (打印目录和文件名) ---\n");
    
    // 4.1. 读取全局头部 (ENTRIES, DIR_COUNT)
    uint64_t ENTRIES = 0;
    uint64_t DIR_COUNT = 0;
    
    if (verify_offset + 8 > NewIndexDataSize) { goto dir_map_error; }
    memcpy(&ENTRIES, NewIndexData + verify_offset, 8);
    verify_offset += 8;

    if (verify_offset + 8 > NewIndexDataSize) { goto dir_map_error; }
    memcpy(&DIR_COUNT, NewIndexData + verify_offset, 8);
    verify_offset += 8;
    
    printf("  全局 ENTRIES: %llu, DIR_COUNT: %llu\n", (unsigned long long)ENTRIES, (unsigned long long)DIR_COUNT);

    uint64_t total_files_verified = 0;
    
    // 4.2. 遍历并验证目录和文件条目
    for (uint64_t d = 0; d < DIR_COUNT; d++) {
        uint32_t DIR_LEN = 0;
        uint64_t DIR_FILES = 0;

        // 读取 DIR_LEN (4B)
        if (verify_offset + 4 > NewIndexDataSize)
        {
            printf("🚨 错误：DirMap 中 DIR_LEN 读取异常。\n");
            goto dir_map_error;
        }
        memcpy(&DIR_LEN, NewIndexData + verify_offset, 4);
        verify_offset += 4;

        // 读取 DIR_NAME (DIR_LEN B) 并打印
        if (verify_offset + DIR_LEN > NewIndexDataSize)
        {
            printf("🚨 错误：DirMap 中 DIR_NAME 读取异常。\n");
            goto dir_map_error;
        }
        if (DIR_LEN > 0)
        {
            // 将目录名复制到临时缓冲区并确保以 null 终止
            size_t copy_len = (DIR_LEN < sizeof(temp_buffer) - 1) ? DIR_LEN : sizeof(temp_buffer) - 1;
            memcpy(temp_buffer, NewIndexData + verify_offset, copy_len);
            temp_buffer[copy_len] = '\0';
        }
        else
        {
            temp_buffer[0] = '\0';
        }
        verify_offset += DIR_LEN; 
        const char *DIR_NAME = temp_buffer;

        // 读取 DIR_FILES (8B)
        if (verify_offset + 8 > NewIndexDataSize)
        {
            printf("🚨 错误：DirMap 中 DIR_FILES 读取异常。\n");
            goto dir_map_error;
        }
        memcpy(&DIR_FILES, NewIndexData + verify_offset, 8);
        verify_offset += 8;
        
        printf("  - 目录 #%llu: LEN=%u, FILES=%llu, NAME='%s'\n", 
               (unsigned long long)d, DIR_LEN, (unsigned long long)DIR_FILES, DIR_NAME);


        // 遍历当前目录下的所有文件 (File Path Entry Body 部分)
        for (uint64_t f = 0; f < DIR_FILES; f++) {
            uint32_t FilenameSize = 0;
            uint32_t ENTRY_Index = 0;
            
            // 读取 FilenameSize (4B)
            if (verify_offset + 4 > NewIndexDataSize) 
            { 
                printf("🚨 错误：DirMap 中 FilenameSize 读取异常。\n");
                goto dir_map_error; 
            }
            memcpy(&FilenameSize, NewIndexData + verify_offset, 4);
            verify_offset += 4;
            
            // 计算 FilenameRaw 的长度
            int raw_name_len_int = (int32_t)FilenameSize;
            int raw_name_len = (raw_name_len_int > 0) ? raw_name_len_int : -raw_name_len_int * 2;
            
            if (raw_name_len < 0) {
                 printf("🚨 错误：DirMap 中 raw_name_len 计算异常。\n");
                 goto dir_map_error;
            }

            // 读取 FilenameRaw (variable B)
            if (verify_offset + raw_name_len > NewIndexDataSize)   
            { 
                printf("🚨 错误：DirMap 中 FilenameRaw 读取异常。\n");
                goto dir_map_error; 
            }
            
            char filename_utf8[1024] = {0};
            
            if (FilenameSize > 0) {
                // ASCII 或 UTF-8
                size_t copy_len = (raw_name_len < sizeof(filename_utf8) - 1) ? raw_name_len : sizeof(filename_utf8) - 1;
                memcpy(filename_utf8, NewIndexData + verify_offset, copy_len);
                filename_utf8[copy_len] = '\0';
            } else if (FilenameSize < 0) {
                // UTF-16LE, 假设 unicode_to_utf8 可用
                if (unicode_to_utf8(NewIndexData + verify_offset, raw_name_len, filename_utf8, sizeof(filename_utf8)) == -1) {
                    snprintf(filename_utf8, sizeof(filename_utf8), "<UTF-16LE Conversion Failed>");
                }
            }
            verify_offset += raw_name_len;

            // 读取 ENTRY_Index (4B)
            if (verify_offset + 4 > NewIndexDataSize)
            {
                printf("🚨 错误：DirMap 中 ENTRY_Index 读取异常。\n");
                goto dir_map_error;
            }

            memcpy(&ENTRY_Index, NewIndexData + verify_offset, 4);
            verify_offset += 4;
            
            total_files_verified++;
            
            // 打印文件条目信息 (只打印前几个和后几个)
            if (total_files_verified < 5 || total_files_verified > (total_files_verified - 5)) { // 粗略打印前几个和后几个
                 printf("    -> 文件 #%llu: RawSize=%d, ENTRY=%u, NAME='%s'\n", 
                        (unsigned long long)f, FilenameSize, ENTRY_Index, filename_utf8);
            }
        }
    }
    
    printf("  DirMap 结构验证成功。总共验证文件条目: %llu\n", (unsigned long long)total_files_verified);
    printf("--- 调试验证完成 ---\n");
    return;

dir_map_error:
    printf("🚨 致命错误：DirMap 结构验证失败，在偏移 0x%llx 处数据大小不足或结构异常。\n", (unsigned long long)verify_offset);
    printf("--- 调试验证完成 ---\n");
}
int main() {
    // 定义源 PAK 文件和包含新数据的 PAK 文件路径
    const char *SRC_PAK_PATH = "../paks/game_patch_1.33.12.14383原厂（复件）.pak";
    const char *MY_PAK_PATH = "../paks/map_lobby_1.33.12.14210原厂（复件）.pak";
    const char *NEW_PAK_PATH = "../paks/game_patch_1.33.12.14383万能范围原厂.pak"; // 🌟 新增：生成的新文件路径

    // 定义要替换的旧文件实例的索引（我们假设要替换最后两个 Entry Index）
    int old_entry_indices[2] = {-1, -1};
    
    // 定义要提取的新文件实例名（在 my_pak 中寻找）
    const char *key_str_my[] = { "CH_Base_SK_PhysicsAsset.uasset", "CH_Base_SK_PhysicsAsset.uexp" };

    // 检查文件是否存在
    if (access(SRC_PAK_PATH, F_OK) == -1 || access(MY_PAK_PATH, F_OK) == -1) {
        fprintf(stderr, "Error: Source PAK or My PAK file not found.\n");
        return 1;
    }

    // 结构体初始化
    PakIndexData my_data = {0};
    PakIndexData src_data = {0};
    NewInstance my_instances[2] = {0};
    int MyPakFile = -1;
    int SrcPakFile = -1;
    int NewPakFile = -1; // 🌟 新增：新文件描述符
    int result = 0;
    uint8_t *NewIndexData = NULL;
    uint64_t found_my_count = 0;

    // ------------------------------------------------------------------
    // 1. 读取 src_pak，解析索引并确定替换范围
    // ------------------------------------------------------------------
    printf("--- Phase 1: Reading SRC_PAK and determining replacement range ---\n");
    SrcPakFile = open(SRC_PAK_PATH, O_RDONLY);
    if (SrcPakFile == -1) { fprintf(stderr, "Failed to open src.pak\n"); return 1; }

    if (ParsePakIndex(SrcPakFile, &src_data) != 0) {
        fprintf(stderr, "Failed to parse src.pak index.\n");
        result = 1; goto cleanup;
    }

    // 确定要替换的最后两个文件实例的 Entry Index
    if (src_data.NumOfEntry >= 2) {
        old_entry_indices[0] = src_data.NumOfEntry - 2;
        old_entry_indices[1] = src_data.NumOfEntry - 1;
        printf("Targeting last two entries: #%d and #%d for replacement.\n", old_entry_indices[0], old_entry_indices[1]);
    } else {
        fprintf(stderr, "Error: SRC_PAK has fewer than 2 file entries.\n");
        result = 1; goto cleanup;
    }

    // 找到最后一个文件数据块的末尾偏移 (即旧数据体的大小)
    uint64_t old_data_body_end = 0;
    uint64_t last_file_offset = find_last_file_offset(&src_data, &old_data_body_end);
    
    // 我们只需要知道最后一个文件数据块的起始位置
    Entry *old_entry_0 = &src_data.FileEntries[old_entry_indices[0]];
    Entry *old_entry_1 = &src_data.FileEntries[old_entry_indices[1]];
    uint64_t old_entry_0_FileOffset = old_entry_0->FileOffset;
    printf("Old data body start offset for replacement: 0x%llx\n", old_entry_0_FileOffset);


    // ------------------------------------------------------------------
    // 2. 读取 my_pak，提取新实例数据
    // ------------------------------------------------------------------
    printf("--- Phase 2: Reading MY_PAK and extracting new data ---\n");
    MyPakFile = open(MY_PAK_PATH, O_RDONLY);
    if (MyPakFile == -1) { fprintf(stderr, "Failed to open my.pak\n"); result = 1; goto cleanup; }

    if (ParsePakIndex(MyPakFile, &my_data) != 0) {
        fprintf(stderr, "Failed to parse my.pak index.\n");
        result = 1; goto cleanup;
    }

    // 遍历 my_pak 提取目标新实例
    for (uint32_t i = 0; i < my_data.NumOfInstances && found_my_count < 2; i++) {
        FileInstance *inst = &my_data.AllFileInstances[i];
        for (int k = 0; k < 2; k++) {
            if (strcmp(inst->Filename, key_str_my[k]) == 0) {
                NewInstance *ni = &my_instances[found_my_count];
                
                // 拷贝 Entry 元数据（除了 FileOffset，因为它是旧的）
                memcpy(&ni->entry, inst->entry_ptr, sizeof(Entry));
                
                // 拷贝 CompressionBlock 数组
                uint32_t blocks_size = ni->entry.NumOfBlocks * sizeof(CompressionBlock);
                ni->blocks = (CompressionBlock*)malloc(blocks_size);
                if (ni->blocks) {
                    memcpy(ni->blocks, inst->entry_ptr->blocks, blocks_size);
                } else {
                    fprintf(stderr, "Failed to allocate memory for blocks.\n");
                    result = 1; goto cleanup;
                }

                // 读取新实例的实际压缩文件数据
                if (get_data_CompressedLength(MyPakFile, &ni->entry, &ni->Data, &ni->DataSize) != 0) {
                    fprintf(stderr, "Failed to read data for %s\n", key_str_my[k]);
                    result = 1; goto cleanup;
                }
                // START: DirMap信息填充并序列化 (调用新增的函数)
                if (computeDirMap(&my_data, inst, ni) != 0) {
                     fprintf(stderr, "Failed to compute DirMap for %s\n", key_str_my[k]);
                     result = 1; goto cleanup;
                }
                // END: DirMap信息填充
                found_my_count++;
                break;
            }
        }
    }

    if (found_my_count != 2) {
        fprintf(stderr, "Error: Failed to find 2 new instances in MY_PAK.\n");
        result = 1; goto cleanup;
    }
    close(MyPakFile); MyPakFile = -1;

    // ------------------------------------------------------------------
    // 3. 生成 new_pak 文件
    // ------------------------------------------------------------------
    printf("--- Phase 3: Creating NEW_PAK and updating content ---\n");
    NewPakFile = open(NEW_PAK_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (NewPakFile == -1) { fprintf(stderr, "Failed to create new.pak\n"); result = 1; goto cleanup; }

    uint64_t current_new_offset = 0; // 追踪 new_pak 的当前写入偏移
    uint64_t buffer_size = CHUNK_SIZE;
    uint8_t *buffer = (uint8_t*)malloc(buffer_size);
    if (!buffer) { fprintf(stderr, "Buffer allocation failed.\n"); result = 1; goto cleanup; }
    
    // 3.1 写入 src_pak 中旧数据块起始偏移之前的内容 (Data Body 前半部分)
    printf("3.1 Writing data body before offset 0x%llx...\n", old_entry_0_FileOffset);
    uint64_t bytes_to_copy = old_entry_0_FileOffset;
    uint64_t bytes_copied = 0;
    
    // 从 src_pak 的 0 偏移开始复制
    while (bytes_copied < bytes_to_copy) {
        size_t to_read = (size_t)(bytes_to_copy - bytes_copied < buffer_size ? bytes_to_copy - bytes_copied : buffer_size);
        ssize_t bytesRead = pread(SrcPakFile, buffer, to_read, bytes_copied);
        if (bytesRead <= 0) {
            fprintf(stderr, "Failed to read data body from src.pak.\n");
            result = 1; goto cleanup_buffer;
        }
        if (write(NewPakFile, buffer, bytesRead) != bytesRead) {
            fprintf(stderr, "Failed to write data body to new.pak.\n");
            result = 1; goto cleanup_buffer;
        }
        bytes_copied += bytesRead;
        current_new_offset += bytesRead;
    }

    // 3.2 写入两个新实例的 databody ，包含 head + data
    printf("3.2 Writing new instance data blocks...\n");
    uint64_t offset_add = 0; // 记录新的数据体大小与旧数据体大小的增量偏移

    // 写入第一个新实例（先写入size=94的head，再写入size=CompressionLength的data）
    
    NewInstance *ni0 = &my_instances[0];

    // 压缩块的 start/end 偏移不是相对于 FileOffset 的，而是相对于整个文件开头的偏移，因此需要先调整
    if (ni0->entry.CompressionMethod != 0 && ni0->entry.NumOfBlocks > 0)
    {
        for (uint32_t b = 0; b < ni0->entry.NumOfBlocks; b++)
        {
            ni0->entry.blocks[b].start =  ni0->entry.blocks[b].start- ni0->entry.FileOffset + current_new_offset ;
            ni0->entry.blocks[b].end = ni0->entry.blocks[b].end - ni0->entry.FileOffset + current_new_offset ;
        }
    }
    ni0->entry.FileOffset = current_new_offset;
    // 根据新的 entry ，构造新的  ni0->Head （size = 94）字段
    SerializeHeadData(&ni0->entry, &ni0->Head, &ni0->HeadSize); 
    // 写入size=94的  ni0->Head
    if (write(NewPakFile, &ni0->Head, ni0->HeadSize) != ni0->HeadSize)
    {
        fprintf(stderr, "Failed to write new data block 1 head.\n");
        result = 1;
        goto cleanup_buffer;
    }
    // 再写入size=CompressionLength的data
        if (write(NewPakFile, ni0->Data, ni0->DataSize) != ni0->DataSize)
    {
        fprintf(stderr, "Failed to write new data block 1.\n");
        result = 1;
        goto cleanup_buffer;
    }
    // 更新当前偏移
    current_new_offset += ni0->HeadSize + ni0->DataSize;

    // 写入第二个新实例
    NewInstance *ni1 = &my_instances[1];
    // 压缩块的 start/end 偏移不是相对于 FileOffset 的，而是相对于整个文件开头的偏移，因此需要调整
    if (ni1->entry.CompressionMethod != 0 && ni1->entry.NumOfBlocks > 0)
    {
        for (uint32_t b = 0; b < ni1->entry.NumOfBlocks; b++)
        {
            ni1->entry.blocks[b].start =  ni1->entry.blocks[b].start- ni1->entry.FileOffset + current_new_offset ;
            ni1->entry.blocks[b].end = ni1->entry.blocks[b].end - ni1->entry.FileOffset + current_new_offset ;
        }
    }
    ni1->entry.FileOffset = current_new_offset; // 更新 Entry 结构体中的 FileOffset
    // 根据新的 entry ，构造新的  ni1->Head （size = 94）字段
    SerializeHeadData(&ni1->entry, &ni1->Head, &ni1->HeadSize);
    // 写入size=94的  ni1->Head
    if (write(NewPakFile, &ni1->Head, ni1->HeadSize) != ni1->HeadSize)
    {
        fprintf(stderr, "Failed to write new data block 1 head.\n");
        result = 1;
        goto cleanup_buffer;
    }
    // 再写入size=CompressionLength的data
        if (write(NewPakFile, ni1->Data, ni1->DataSize) != ni1->DataSize)
    {
        fprintf(stderr, "Failed to write new data block 1.\n");
        result = 1;
        goto cleanup_buffer;
    }
    // 更新当前偏移
    current_new_offset += ni1->HeadSize + ni1->DataSize;

    // 计算增量偏移
    offset_add = ni0->entry.CompressedLength -  old_entry_0->CompressedLength +
                 ni1->entry.CompressedLength -  old_entry_1->CompressedLength;
    printf("Data body size change: %llu bytes (Offset Add).\n", offset_add);

    // 3.3 写入 SRC_PAK 中最后两个实例之后的剩余数据 (如果存在)
    // printf("3.3 Writing remaining data body...\n");
    // 复制旧索引之前的数据体剩余部分，从旧数据块末尾开始
    // bytes_to_copy = src_data.info.offset;
    // bytes_copied = old_data_body_end;
    
    // while (bytes_copied < bytes_to_copy) {
    //     size_t to_read = (size_t)(bytes_to_copy - bytes_copied < buffer_size ? bytes_to_copy - bytes_copied : buffer_size);
    //     ssize_t bytesRead = pread(SrcPakFile, buffer, to_read, bytes_copied);
    //     if (bytesRead <= 0) break; // 如果读取失败或结束，就退出
        
    //     if (write(NewPakFile, buffer, bytesRead) != bytesRead) {
    //         fprintf(stderr, "Failed to write remaining data body to new.pak.\n");
    //         result = 1; goto cleanup_buffer;
    //     }
    //     bytes_copied += bytesRead;
    //     current_new_offset += bytesRead;
    // }
    // free(buffer);
    // buffer = NULL;

    // ------------------------------------------------------------------
    // 4. 重建并写入索引数据
    // ------------------------------------------------------------------
    printf("--- Phase 4: Reconstructing and writing new Index Data ---\n");
    uint64_t new_index_offset = current_new_offset; // 新索引的起始偏移
    uint64_t NewIndexDataSize = 0; // 新索引区的字节大小（包含挂载点+索引列表+路径列表）
    // 分配内存用于存储新的索引数据 (预留空间)
    NewIndexData = (uint8_t*)malloc(src_data.OriginalIndexSize + 1024);
    if (!NewIndexData) { result = 1; goto cleanup; }

    // 4.1. MountPointLength + MountPoint + NumOfEntry (索引数据的前半部分) 保持原版不变
    uint64_t pre_entry_size = 4+29+4;
    write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData, pre_entry_size);

    // 4.2. 写入更新后的 Entry 列表
    for (uint32_t i = 0; i < src_data.NumOfEntry; i++)
    {
        Entry *e = &src_data.FileEntries[i];

        // 如果是最后两个被替换的实例，用新实例的元数据覆盖
        if (i == old_entry_indices[0] - 1)
        {
            int k = 0;
        }
        if (i == old_entry_indices[0])
        {
            e = &ni0->entry;
        }
        else if (i == old_entry_indices[1])
        {
            e = &ni1->entry;
        }

        // 🌟 核心更新：调整 FileOffset
        // 只有被替换的实例 FileOffset 是绝对值（已在 3.2 更新）。
        // 其他实例的 FileOffset 需要加上增量偏移 offset_add。
        uint64_t adjusted_offset = e->FileOffset;
        if (i < old_entry_indices[0]) {
            // 在被替换块之前，FileOffset 不变
            adjusted_offset = e->FileOffset;
        } else if (i > old_entry_indices[1]) {
            // 在被替换块之后，FileOffset 需要加上增量
            adjusted_offset = e->FileOffset + offset_add;
            // TODO 压缩块的 start/end 偏移不是相对于 FileOffset 的，而是相对于整个文件开头的偏移，因此需要调整
        } else {
            // 被替换的块，使用新块的绝对 FileOffset (已在 ni0/ni1->entry 中更新)
            adjusted_offset = e->FileOffset;
            // 压缩块的 start/end 偏移不是相对于 FileOffset 的，而是相对于整个文件开头的偏移，因此需要调整，已在 3.2 处理
        }
        
        // 写入 Entry 结构体
        // 【DEBUG TEST】: 替换 FileHash 字段为调试测试用的特殊字符 (20x 0xDE)
        // uint8_t debug_hash[20];
        // memset(debug_hash, 0xDE, 20); // 用 0xDE 填充 20 字节
        // write_data(NewIndexData, &NewIndexDataSize, debug_hash, 20); // 写入调试哈希
        write_data(NewIndexData, &NewIndexDataSize, e->FileHash, 20);
        write_data(NewIndexData, &NewIndexDataSize, &adjusted_offset, 8); // 写入调整后的偏移
        write_data(NewIndexData, &NewIndexDataSize, &e->FileSize, 8);
        write_data(NewIndexData, &NewIndexDataSize, &e->CompressionMethod, 4);
        write_data(NewIndexData, &NewIndexDataSize, &e->CompressedLength, 8);
        write_data(NewIndexData, &NewIndexDataSize, e->Dummy, 21);

        // NumOfBlocks + Blocks Data
        if (e->CompressionMethod != 0)
        {
            write_data(NewIndexData, &NewIndexDataSize, &e->NumOfBlocks, 4);
            for (uint32_t i = 0; i < e->NumOfBlocks; i++)
            {
                if(i>1)
                {
                    int debug = i;
                }
                write_data(NewIndexData, &NewIndexDataSize, &e->blocks[i].start, 8);
                write_data(NewIndexData, &NewIndexDataSize, &e->blocks[i].end, 8);
            }
        }
        write_data(NewIndexData, &NewIndexDataSize, &e->CompressedBlockSize, 4);
        write_data(NewIndexData, &NewIndexDataSize, &e->Encrypted, 1);
    }

    // 4.3. 更新并写入 Directory Map

    // 先写入 原始 的 Directory Map 数据
    FileInstance src_0 =  src_data.AllFileInstances[src_data.NumOfEntry-2];
    FileInstance src_1 =  src_data.AllFileInstances[src_data.NumOfEntry-1];
    uint64_t src_front_size = src_0.DirStartOffset - src_data.DirMapStartOffset; // 上文长度
    uint64_t src_0_head_size = src_0.PathIndexStart - src_0.DirStartOffset; // 本文的头长度
    uint64_t src_0_body_size = src_0.PathIndexEnd - src_0.PathIndexStart;// 本文0的身长度
    uint64_t src_1_body_size = src_1.PathIndexEnd - src_1.PathIndexStart;// 本文1的身长度
    uint64_t src_back_size = src_data.DirMapEndOffset -src_1.PathIndexEnd ; // 下文长度

    write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData + src_data.DirMapStartOffset, src_front_size);
    // 然后写入 新实例 的文件夹 Directory Map 
    ni0->dir_map.dir_files=2; // 手动更新文件数量 DEBUG
    ni0->dir_map.entry_index = src_0.EntryIndex; // 更新实体索引
    ni1->dir_map.entry_index = src_1.EntryIndex; //  更新实体索引
    SerializeDirMap(&ni0->dir_map);
    SerializeDirMap(&ni1->dir_map);
    write_data(NewIndexData, &NewIndexDataSize, ni0->dir_map.dir_bin_head, ni0->dir_map.dir_bin_head_size);
    // 写入 两个新实例 计算生成的 文件 Directory Map 
    write_data(NewIndexData, &NewIndexDataSize, ni0->dir_map.dir_bin_body, ni0->dir_map.dir_bin_body_size);
    write_data(NewIndexData, &NewIndexDataSize, ni1->dir_map.dir_bin_body, ni1->dir_map.dir_bin_body_size);
    // 最后写入剩下的原始内容（最后一个空路径）
    write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData + src_1.PathIndexEnd , src_back_size);
    // 在写入文件之前，先打印验证我们在内存中构造的新索引数据
    VerifyNewIndexData(NewIndexData, NewIndexDataSize, src_data.NumOfEntry, offset_add);

    // 4.4. 将新索引数据写入 NEW_PAK 文件
    if (write(NewPakFile, NewIndexData, NewIndexDataSize) != NewIndexDataSize) {
        fprintf(stderr, "Failed to write new index data.\n");
        result = 1; goto cleanup;
    }
    current_new_offset += NewIndexDataSize;

    // ------------------------------------------------------------------
    // 5. 更新并写入 PakInfo (文件尾部)
    // ------------------------------------------------------------------
    printf("--- Phase 5: Writing updated PakInfo ---\n");
    // 新的 PakInfo 偏移：新索引起始位置相对于文件开头的偏移
    src_data.info.offset = new_index_offset ^ OFFSET_KEY;    // 🌟 更新核心字段：异或后的索引偏移
    // src_data.info.size = NewIndexDataSize;               // 更新索引大小(存疑TODO)
    
    // 将更新后的 PakInfo 写入文件末尾
    if (write(NewPakFile, &src_data.info, 45) != 45) {
        fprintf(stderr, "Failed to write updated PakInfo.\n");
        result = 1;
        goto cleanup;
    }
    current_new_offset += 45;
    printf("Successfully created and updated %s (New Size: %llu bytes, Size Delta: %+lld bytes).\n", NEW_PAK_PATH, (unsigned long long)current_new_offset, (long long)offset_add);

cleanup_buffer:
    if (buffer) free(buffer);
cleanup:
    // 清理新实例数据
    for (int i = 0; i < 2; i++) {
        if (my_instances[i].Data) free(my_instances[i].Data);
        // blocks 数组已经移交给 src_data.FileEntries，但 src_data 会在 CleanupPakData 中释放
    }
    // 清理 PAK 索引解析时分配的内存
    CleanupPakData(&my_data);
    CleanupPakData(&src_data);
    // 清理新的索引缓存
    if (NewIndexData) free(NewIndexData);
    // 关闭文件描述符
    if (MyPakFile != -1) close(MyPakFile);
    if (SrcPakFile != -1) close(SrcPakFile);
    if (NewPakFile != -1) close(NewPakFile);

    return result;
}
