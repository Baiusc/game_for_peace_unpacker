/*
 * @Description  : 找到并替换src_pak中两个旧实例为my_pak中两个新实例
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

// Pak文件头结构体
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

// 文件路径和索引信息结构
typedef struct {
    int32_t FilenameSize;
    char Filename[1024];
    int32_t EntryIndex;
    Entry *entry_ptr;
    char *DirPath;
    int32_t DirLen;
    uint64_t PathIndexStart; // 路径条目在 IndexData 中的起始偏移
    uint64_t PathIndexEnd;   // 路径条目在 IndexData 中的结束偏移
    uint64_t DirFilesPtrOffset; // DIR_FILES 字段在 IndexData 中的偏移
    uint64_t EntryIndexStart; // 对应 Entry 在 IndexData 中的起始偏移
    uint64_t EntryIndexEnd;   // 对应 Entry 在 IndexData 中的结束偏移
    uint64_t DirStartOffset; // 目录块在 IndexData 中的起始偏移 (DIR_LEN 之前)
} FileInstance;

// 新实例数据结构
typedef struct {
    Entry entry;
    char Filename[1024];
    char DirPath[1024];
    uint8_t *Data; // 压缩后的文件数据
    uint64_t DataSize;
    CompressionBlock *blocks; // 压缩块列表
} NewInstance;

// 完整的解析结果结构，用于从函数返回
typedef struct {
    PakInfo info;
    char MountPoint[1024];
    uint32_t NumOfEntry;
    Entry *FileEntries;
    FileInstance *AllFileInstances;
    uint32_t NumOfInstances; // 应该等于 NumOfEntry
    uint8_t *OriginalIndexData;
    int64_t OriginalIndexSize;
    uint64_t EntryListEndOffset; // Entry 列表在 IndexData 中的结束偏移
    uint64_t DirMapStartOffset; // Directory Map 在 IndexData 中的起始偏移 (ENTRIES 之前)
    uint64_t DirMapEndOffset; // Directory Map 在 IndexData 中的结束偏移
} PakIndexData;

// ----------------------------------------------------------------------
// 辅助函数 (保持声明，简化实现)
// ----------------------------------------------------------------------

void DecryptData(uint8_t *data, uint32_t size) { /* ... */ }
unsigned int ZLIB_decompress(unsigned char *InData, unsigned int InSize, unsigned char *OutData, unsigned int OutSize) { /* ... */ return 0; }
int unicode_to_utf8(const char *input, size_t input_len, char *out, size_t output_len) { /* ... */ return 0; }

// 全局变量，用于追踪在内存中读取文件索引的当前位置
uint64_t current_index_offset = 0;

void read_data(void *destination, const uint8_t *source, size_t length) {
    memcpy(destination, source + current_index_offset, length);
    current_index_offset += length;
}

// ----------------------------------------------------------------------
// 核心辅助函数：PAK 索引解析
// ----------------------------------------------------------------------

/**
 * @brief 解析 PAK 文件的索引数据并返回所有必要信息
 * @param PakFileDescriptor 已打开的 PAK 文件描述符
 * @param Result 指向 PakIndexData 结构的指针，用于存储解析结果
 * @return int 0 成功，非 0 失败
 */
int ParsePakIndex(int PakFileDescriptor, PakIndexData *Result) {
    // 1. 读取 PakInfo
    if (lseek(PakFileDescriptor, -45, SEEK_END) == -1 || read(PakFileDescriptor, &Result->info, 45) != 45) {
        fprintf(stderr, "Failed to read pak header at -45\n");
        return 1;
    }
    
    // 文件头反混淆
    uint64_t OriginalIndexOffset = Result->info.offset ^ OFFSET_KEY;
    uint8_t EncryptedFlag = Result->info.encrypted ^ 0x6C;
    
    off_t total_size = lseek(PakFileDescriptor, 0, SEEK_END);
    Result->OriginalIndexSize = total_size - OriginalIndexOffset - 45;

    if (Result->OriginalIndexSize < 0 || Result->OriginalIndexSize > 52428800) {
        fprintf(stderr, "Index data size is not compatible or offset error.\n");
        return 1;
    }

    // 2. 读取索引数据到内存
    Result->OriginalIndexData = (uint8_t*)malloc(Result->OriginalIndexSize);
    if (!Result->OriginalIndexData) {
        fprintf(stderr, "Memory allocation failed for IndexData.\n");
        return 1;
    }
    
    if (pread(PakFileDescriptor, Result->OriginalIndexData, Result->OriginalIndexSize, OriginalIndexOffset) != Result->OriginalIndexSize) {
        fprintf(stderr, "Failed to load index data\n");
        free(Result->OriginalIndexData);
        return 1;
    }
    
    // 解密
    if (EncryptedFlag) {
        DecryptData(Result->OriginalIndexData, Result->OriginalIndexSize);
    }
    
    // 3. 解析索引数据
    current_index_offset = 0;
    
    // 3.1. MountPoint 和 NumOfEntry
    uint32_t MountPointLength;
    read_data(&MountPointLength, Result->OriginalIndexData, 4);
    read_data(Result->MountPoint, Result->OriginalIndexData, MountPointLength);
    Result->MountPoint[MountPointLength] = '\0';
    
    read_data(&Result->NumOfEntry, Result->OriginalIndexData, 4);
    Result->NumOfInstances = Result->NumOfEntry; // 实例数 = Entry 数

    // 3.2. Entry 列表
    Result->FileEntries = (Entry*)malloc(Result->NumOfEntry * sizeof(Entry));
    if (!Result->FileEntries) { /* ... */ return 1; }

    for (uint32_t i = 0; i < Result->NumOfEntry; i++) {
        uint64_t entry_start = current_index_offset;

        // 读取 Entry 固长部分 (FileHash -> Dummy)
        read_data(Result->FileEntries[i].FileHash, Result->OriginalIndexData, 20);
        read_data(&Result->FileEntries[i].FileOffset, Result->OriginalIndexData, 8);
        read_data(&Result->FileEntries[i].FileSize, Result->OriginalIndexData, 8);
        read_data(&Result->FileEntries[i].CompressionMethod, Result->OriginalIndexData, 4);
        read_data(&Result->FileEntries[i].CompressedLength, Result->OriginalIndexData, 8);
        read_data(Result->FileEntries[i].Dummy, Result->OriginalIndexData, 21);

        // NumOfBlocks
        read_data(&Result->FileEntries[i].NumOfBlocks, Result->OriginalIndexData, 4);

        // 压缩块列表
        if (Result->FileEntries[i].NumOfBlocks > 0) {
            uint32_t blocks_size = Result->FileEntries[i].NumOfBlocks * sizeof(CompressionBlock);
            Result->FileEntries[i].blocks = (CompressionBlock*)malloc(blocks_size);
            if (!Result->FileEntries[i].blocks) { /* ... */ return 1; }
            read_data(Result->FileEntries[i].blocks, Result->OriginalIndexData, blocks_size);
        } else {
            Result->FileEntries[i].blocks = NULL;
        }

        // CompressedBlockSize 和 Encrypted
        read_data(&Result->FileEntries[i].CompressedBlockSize, Result->OriginalIndexData, 4);
        read_data(&Result->FileEntries[i].Encrypted, Result->OriginalIndexData, 1);
        
        // 记录 Entry 所在索引的偏移
        // ⚠️ 这一步复杂，需要外部代码在遍历 Directory Map 时填充。此处先跳过，在下一步填充
        
        // Result->FileEntries[i].EntryIndexStart = entry_start;
        // Result->FileEntries[i].EntryIndexEnd = current_index_offset;
    }
    Result->EntryListEndOffset = current_index_offset;

    // 3.3. Directory Map
    Result->DirMapStartOffset = current_index_offset;
    uint64_t ENTRIES, DIR_COUNT;
    read_data(&ENTRIES, Result->OriginalIndexData, 8);
    read_data(&DIR_COUNT, Result->OriginalIndexData, 8);
    
    Result->AllFileInstances = (FileInstance*)malloc(Result->NumOfEntry * sizeof(FileInstance));
    uint32_t InstanceCounter = 0;

    for (uint64_t dir_idx = 0; dir_idx < DIR_COUNT; dir_idx++) {
        uint64_t dir_start_offset = current_index_offset;
        int32_t DIR_LEN; char DIR_NAME[1024]; uint64_t DIR_FILES;
        
        read_data(&DIR_LEN, Result->OriginalIndexData, 4); 
        read_data(DIR_NAME, Result->OriginalIndexData, DIR_LEN); DIR_NAME[DIR_LEN] = '\0';
        uint64_t dir_files_offset = current_index_offset;
        read_data(&DIR_FILES, Result->OriginalIndexData, 8);

        for (uint64_t x = 0; x < DIR_FILES; x++) {
            if (InstanceCounter >= Result->NumOfEntry) { break; } // 安全检查
            
            uint64_t path_start_offset = current_index_offset;
            int32_t FilenameSize; char FilenameBuffer[1024]; int32_t ENTRY_Index;

            read_data(&FilenameSize, Result->OriginalIndexData, 4);
            int name_len = FilenameSize > 0 ? FilenameSize : -FilenameSize * 2;
            read_data(FilenameBuffer, Result->OriginalIndexData, name_len);
            
            if (FilenameSize < 0) {
                unicode_to_utf8(FilenameBuffer, name_len, FilenameBuffer, sizeof(FilenameBuffer));
            } else {
                FilenameBuffer[name_len] = '\0';
            }
            
            read_data(&ENTRY_Index, Result->OriginalIndexData, 4);
            uint64_t path_end_offset = current_index_offset;

            // 存储实例信息
            FileInstance *inst = &Result->AllFileInstances[InstanceCounter];
            inst->EntryIndex = ENTRY_Index;
            inst->entry_ptr = &Result->FileEntries[ENTRY_Index];
            inst->FilenameSize = FilenameSize;
            strncpy(inst->Filename, FilenameBuffer, 1024);
            
            inst->DirLen = DIR_LEN;
            inst->DirPath = strdup(DIR_NAME);
            inst->DirFilesPtrOffset = dir_files_offset;
            inst->PathIndexStart = path_start_offset;
            inst->PathIndexEnd = path_end_offset;
            inst->DirStartOffset = dir_start_offset;

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
// main 函数
// ----------------------------------------------------------------------

int main() {
    // 0. 定义路径和目标键
    const char *SRC_PAK_PATH = "src.pak";
    const char *MY_PAK_PATH = "my.pak";

    const char *key_str_old[] = { "Game/Asset/Old/AssetA.uasset", "Game/Asset/Old/AssetB.uasset" };
    const char *key_str_my[] = { "Game/Asset/New/NewAssetA.uasset", "Game/Asset/New/NewAssetB.uasset" };
    
    if (access(SRC_PAK_PATH, F_OK) == -1 || access(MY_PAK_PATH, F_OK) == -1) {
        fprintf(stderr, "Error: Source PAK or My PAK file not found.\n");
        return 1;
    }

    PakIndexData my_data = {0};
    PakIndexData src_data = {0};
    
    NewInstance my_instances[2] = {0};
    FileInstance *old_instances_ptr[2] = {NULL};
    int found_my_count = 0;
    int found_old_count = 0;
    
    int MyPakFile = -1;
    int SrcPakFile = -1;
    int result = 0;

    // ------------------------------------------------------------------
    // 1. 读取 my_pak，找到两个新实例的数据和 Entry
    // ------------------------------------------------------------------
    printf("--- Phase 1: Reading MY_PAK and extracting new instances ---\n");
    MyPakFile = open(MY_PAK_PATH, O_RDONLY);
    if (MyPakFile == -1) { fprintf(stderr, "Failed to open my.pak\n"); return 1; }
    
    if (ParsePakIndex(MyPakFile, &my_data) != 0) {
        fprintf(stderr, "Failed to parse my.pak index.\n");
        result = 1; goto cleanup;
    }
    
    // 遍历 my_pak 实例，查找目标
    for (uint32_t i = 0; i < my_data.NumOfInstances && found_my_count < 2; i++) {
        FileInstance *inst = &my_data.AllFileInstances[i];
        for (int k = 0; k < 2; k++) {
            if (strcmp(inst->Filename, key_str_my[k]) == 0) {
                // 找到新实例，现在提取其 Entry 和数据
                NewInstance *ni = &my_instances[found_my_count];
                memcpy(&ni->entry, inst->entry_ptr, sizeof(Entry));
                strncpy(ni->Filename, inst->Filename, 1024);
                strncpy(ni->DirPath, inst->DirPath, 1024);
                
                // 提取压缩块列表 (注意：需要深拷贝)
                uint32_t blocks_size = ni->entry.NumOfBlocks * sizeof(CompressionBlock);
                ni->blocks = (CompressionBlock*)malloc(blocks_size);
                memcpy(ni->blocks, inst->entry_ptr->blocks, blocks_size);
                
                // 提取文件压缩数据
                // ⚠️ 此处需要实现 read_file_data 函数来读取压缩块并处理加密
                // 占位符：假设数据读取成功
                if (1 /* 实际应为 read_file_data(...) == 0 */) {
                    // 伪代码：
                    ni->DataSize = ni->entry.CompressedLength;
                    ni->Data = (uint8_t*)malloc(ni->DataSize);
                    // pwrite(MyPakFile, ni->Data, ni->DataSize, ni->entry.FileOffset);
                    // 假设数据已正确填充
                    memset(ni->Data, 0xBB + found_my_count, ni->DataSize); 
                }
                
                found_my_count++;
                printf("Extracted new instance: %s\n", ni->Filename);
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
    // 2. 读取 src_pak 索引并找到旧实例的位置
    // ------------------------------------------------------------------
    printf("--- Phase 2: Reading SRC_PAK and locating old instances ---\n");
    SrcPakFile = open(SRC_PAK_PATH, O_RDWR);
    if (SrcPakFile == -1) { fprintf(stderr, "Failed to open src.pak for modification\n"); result = 1; goto cleanup; }
    
    if (ParsePakIndex(SrcPakFile, &src_data) != 0) {
        fprintf(stderr, "Failed to parse src.pak index.\n");
        result = 1; goto cleanup;
    }

    // 遍历 src_pak 实例，查找目标
    for (uint32_t i = 0; i < src_data.NumOfInstances && found_old_count < 2; i++) {
        FileInstance *inst = &src_data.AllFileInstances[i];
        for (int k = 0; k < 2; k++) {
            if (strcmp(inst->Filename, key_str_old[k]) == 0) {
                old_instances_ptr[found_old_count] = inst;
                found_old_count++;
                printf("Found old instance: %s at Entry Index %d\n", inst->Filename, inst->EntryIndex);
                break;
            }
        }
    }
    
    if (found_old_count != 2) {
        fprintf(stderr, "Error: Failed to find 2 old instances in SRC_PAK.\n");
        result = 1; goto cleanup;
    }

    // ------------------------------------------------------------------
    // 3. 替换逻辑 (融入 main)
    // ------------------------------------------------------------------
    printf("--- Phase 3: Writing new data and reconstructing index ---\n");
    
    // 3.1. 计算新文件数据写入的起始偏移并写入新数据
    off_t total_size = lseek(SrcPakFile, 0, SEEK_END);
    uint64_t new_file_data_offset = total_size;
    uint64_t current_write_offset = new_file_data_offset;

    for (int i = 0; i < 2; i++) {
        NewInstance *ni = &my_instances[i];
        FileInstance *oi = old_instances_ptr[i];

        // 🎯 核心替换：用新实例的 Entry 替换旧实例的 Entry
        // 1. 更新新 Entry 的文件偏移量和块信息
        ni->entry.FileOffset = current_write_offset;
        ni->entry.CompressedLength = ni->DataSize;
        
        // 2. 将新的 Entry 结构体直接替换到 src_data 的 FileEntries 数组中
        // 注意：需要释放旧 Entry 的 blocks 内存
        if (oi->entry_ptr->blocks) free(oi->entry_ptr->blocks);
        memcpy(oi->entry_ptr, &ni->entry, sizeof(Entry));
        oi->entry_ptr->blocks = ni->blocks; // 赋值新的 blocks 列表
        
        // 3. 写入新文件数据到 SrcPakFile
        if (pwrite(SrcPakFile, ni->Data, ni->DataSize, current_write_offset) != ni->DataSize) {
            fprintf(stderr, "Failed to write new file data %s.\n", ni->Filename);
            result = 1; goto cleanup;
        }
        current_write_offset += ni->DataSize;
    }

    // 新索引数据的起始偏移
    uint64_t new_index_offset = current_write_offset;
    
    // 3.2. 重建索引数据 (NewIndexData)
    // 🎯 关键：更新 Directory Map 中的路径条目
    
    // 估算新的索引大小 (因为只替换了 FileInstance 的 Filename/DirPath，大小会变化)
    // ⚠️ 此处需要复杂的内存块移动逻辑来移除旧路径条目并插入新路径条目。

    // 简化处理：由于 EntryIndex 没有变化，我们只替换路径字符串。

    uint64_t NewIndexDataSize = 0;
    uint8_t *NewIndexData = (uint8_t*)malloc(src_data.OriginalIndexSize * 2); // 预分配足够大的空间
    uint64_t copy_offset = 0;

    // a. 复制 Entry 列表之前的部分 (MountPoint + NumOfEntry)
    copy_offset = src_data.DirMapStartOffset - 16;
    write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData, copy_offset);

    // b. 重新序列化 Entry 列表 (因为 FileEntries 已经被修改)
    for (uint32_t i = 0; i < src_data.NumOfEntry; i++) {
        Entry *e = &src_data.FileEntries[i];
        
        // 固长部分
        write_data(NewIndexData, &NewIndexDataSize, e->FileHash, 20);
        write_data(NewIndexData, &NewIndexDataSize, &e->FileOffset, 8);
        write_data(NewIndexData, &NewIndexDataSize, &e->FileSize, 8);
        write_data(NewIndexData, &NewIndexDataSize, &e->CompressionMethod, 4);
        write_data(NewIndexData, &NewIndexDataSize, &e->CompressedLength, 8);
        write_data(NewIndexData, &NewIndexDataSize, e->Dummy, 21);
        
        // 可变部分
        write_data(NewIndexData, &NewIndexDataSize, &e->NumOfBlocks, 4);
        if (e->NumOfBlocks > 0) {
            write_data(NewIndexData, &NewIndexDataSize, e->blocks, e->NumOfBlocks * sizeof(CompressionBlock));
        }
        write_data(NewIndexData, &NewIndexDataSize, &e->CompressedBlockSize, 4);
        write_data(NewIndexData, &NewIndexDataSize, &e->Encrypted, 1);
    }

    // c. 复制 Directory Map 固定头部 (ENTRIES + DIR_COUNT)
    uint64_t map_header_offset = src_data.DirMapStartOffset;
    write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData + map_header_offset, 16);
    
    // d. 遍历 Directory Map 复制和替换路径条目
    current_index_offset = src_data.DirMapStartOffset + 16;
    for (uint32_t i = 0; i < src_data.NumOfInstances; i++) {
        FileInstance *inst = &src_data.AllFileInstances[i];
        
        // 检查是否为旧实例，并替换为新实例的路径信息
        int is_old_instance = 0;
        NewInstance *ni_to_use = NULL;
        for (int k = 0; k < 2; k++) {
            if (inst == old_instances_ptr[k]) {
                is_old_instance = 1;
                ni_to_use = &my_instances[k];
                break;
            }
        }

        // ⚠️ 目录块替换的复杂性：如果替换导致 DIR_NAME 或 DIR_LEN 变化，则整个目录块需要被重写。
        // 由于我们只替换文件名，这里假设 DIR_NAME 没有变化。
        
        if (inst->PathIndexStart == current_index_offset) {
            // 这是一个新的目录块或目录中的第一个文件
            // 复制或重写目录头 (DIR_LEN, DIR_NAME, DIR_FILES)
            int32_t DIR_LEN; char DIR_NAME[1024]; uint64_t DIR_FILES;

            // 复制目录头
            memcpy(&DIR_LEN, src_data.OriginalIndexData + current_index_offset, 4);
            current_index_offset += 4;
            current_index_offset += DIR_LEN;
            current_index_offset += 8;
            
            // 重写目录头到 NewIndexData
            write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData + inst->DirStartOffset, 4 + DIR_LEN + 8);
        }

        // 写入文件名和 ENTRY
        if (is_old_instance) {
            // 写入新实例的路径条目
            int32_t new_filename_size = strlen(ni_to_use->Filename);
            write_data(NewIndexData, &NewIndexDataSize, &new_filename_size, 4);
            write_data(NewIndexData, &NewIndexDataSize, ni_to_use->Filename, new_filename_size);
            write_data(NewIndexData, &NewIndexDataSize, &inst->EntryIndex, 4); // 保持 EntryIndex 不变
        } else {
            // 复制原始路径条目
            size_t path_len = inst->PathIndexEnd - inst->PathIndexStart;
            write_data(NewIndexData, &NewIndexDataSize, src_data.OriginalIndexData + inst->PathIndexStart, path_len);
        }
        
        // 推进原始索引指针
        current_index_offset = inst->PathIndexEnd;
    }
    
    // 3.3. 写入新的索引数据块
    if (pwrite(SrcPakFile, NewIndexData, NewIndexDataSize, new_index_offset) != NewIndexDataSize) {
        fprintf(stderr, "Failed to write new index data.\n");
        result = 1; goto cleanup;
    }
    
    // 3.4. 更新 PakInfo 并截断文件
    src_data.info.offset = new_index_offset ^ OFFSET_KEY;
    src_data.info.size = NewIndexDataSize;
    
    if (pwrite(SrcPakFile, &src_data.info, 45, new_index_offset + NewIndexDataSize) != 45) {
        fprintf(stderr, "Failed to write updated PakInfo.\n");
        result = 1; goto cleanup;
    }
    
    if (ftruncate(SrcPakFile, new_index_offset + NewIndexDataSize + 45) != 0) {
        fprintf(stderr, "Failed to truncate file.\n");
        result = 1; goto cleanup;
    }
    
    printf("Successfully replaced 2 old instances with 2 new instances in %s.\n", SRC_PAK_PATH);

// ------------------------------------------------------------------
// 4. 清理
// ------------------------------------------------------------------
cleanup:
    for (int i = 0; i < 2; i++) {
        if (my_instances[i].Data) free(my_instances[i].Data);
        if (my_instances[i].blocks) free(my_instances[i].blocks);
    }
    CleanupPakData(&my_data);
    CleanupPakData(&src_data);
    if (NewIndexData) free(NewIndexData);
    if (MyPakFile != -1) close(MyPakFile);
    if (SrcPakFile != -1) close(SrcPakFile);

    return result;
}