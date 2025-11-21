/*
 * @Author       : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @Date         : 2025-11-04 11:55:58
 * @LastEditors  : baizs_work_pc_ubuntu_kioxia zhongshan.bai@vitalchem.com
 * @LastEditTime : 2025-11-21 14:10:29
 * @FilePath     : /game_for_peace_unpacker/src/game_for_peace_unpack.c
 * @Description  : 找到补丁pak实例列表中的最后一个实例，将其替换为我的新实例（压缩数据、索引、路径）
 * 
 * Copyright (c) 2025 by vitalchem, All Rights Reserved. 
 */
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

// 用于文件索引信息反混淆（解密）的密钥
#define OFFSET_KEY 0xD74AF37FAA6B020D // 用于异或解密文件索引偏移量
#define SIZE_KEY 0x8924B0E3298B7069   // 用于异或解密文件索引大小 (本代码未使用)

// 读写和解压缩的缓冲区大小
#define CHUNK_SIZE 65536

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
    uint64_t start; // 压缩块在pak文件中的起始偏移
    uint64_t end;   // 压缩块在pak文件中的结束偏移
} __attribute__((packed)) CompressionBlock;

// 单个文件条目的元数据结构体
typedef struct {
    uint8_t FileHash[20];        // 文件内容的哈希
    uint64_t FileOffset;         // 文件数据在pak文件中的偏移
    uint64_t FileSize;           // 文件原始大小
    uint32_t CompressionMethod;  // 压缩方法 (0: 无压缩, 1: ZLIB)
    uint64_t CompressedLength;   // 文件压缩后的大小
    uint8_t Dummy[21];           // 未知用途的填充数据
    uint32_t NumOfBlocks;        // 压缩块数量
    CompressionBlock *blocks;    // 压缩块列表
    uint32_t CompressedBlockSize; // 压缩块大小
    uint8_t Encrypted;           // 文件内容是否加密
} __attribute__((packed)) Entry;


// 使用异或进行数据解密
void DecryptData(uint8_t *data, uint32_t size) {
    for (uint32_t index = 0; index < size; index++) {
        data[index] ^= 0x79u;
    }
}

// 根据完整路径创建文件并创建所需的目录
int create_file(const char *fullPath) {
    // 查找最后一个斜杠，以分离路径和文件名
    const char *lastSlash = strrchr(fullPath, '/');
    if (lastSlash != NULL) {
        size_t length = (size_t)(lastSlash - fullPath);
        char path[length + 1];
        strncpy(path, fullPath, length);
        path[length] = '\0';
        // 使用 strtok 分割路径，逐级创建目录
        char *token = strtok(path, "/");
        char currentPath[1024] = "";
        while (token != NULL) {
            strcat(currentPath, token);
            strcat(currentPath, "/");
            mkdir(currentPath, 0777); // 创建目录
            token = strtok(NULL, "/");
        }
    }
    // 创建并打开文件
    return open(fullPath, O_WRONLY | O_CREAT | O_TRUNC, 0644); 
}

// 使用 ZLIB 库解压缩数据
unsigned int ZLIB_decompress(unsigned char *InData, unsigned int InSize, unsigned char *OutData, unsigned int OutSize) {
    z_stream strm;
    // 初始化 zlib 结构体
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.next_in = InData;
    strm.avail_in = InSize;
    strm.next_out = OutData;
    strm.avail_out = OutSize;
    
    // 初始化解压器
    if (inflateInit(&strm) != Z_OK) {
        fprintf(stderr, "Failed to initialize zlib.\n");
        return 0;
    }
    
    // 执行解压缩
    if (inflate(&strm, Z_FINISH) != Z_STREAM_END) {
        fprintf(stderr, "inflate failed: %s\n", strm.msg);
        inflateEnd(&strm);
        return 0;
    }
    
    // 结束解压器
    if (inflateEnd(&strm) != Z_OK) {
        fprintf(stderr, "inflateEnd failed: %s\n", strm.msg);
        return 0;
    }
    
    return strm.total_out; // 返回解压后的数据大小
}

// 全局变量，用于追踪在内存中读取文件索引的当前位置
uint64_t current_index_offset = 0;

// 从内存中的文件索引数据源中读取指定长度的数据
void read_data(void *destination, const uint8_t *source, size_t length) {
    memcpy(destination, source + current_index_offset, length);
    current_index_offset += length;
}

// 声明一个独立的函数来处理文件提取
void extract(int PakFile, Entry entry, char *filename);

// 将 UTF-16LE 编码的字符串转换为 UTF-8 编码
int unicode_to_utf8(const char *input, size_t input_len, char *out, size_t output_len) {
    char output[output_len];
    size_t i = 0, j = 0;
    
    while (i < input_len && j < output_len) {
        unsigned int unicode_char;
        
        // 读取一个 UTF-16LE 字符（小端序）
        unicode_char = (unsigned char)input[i++];
        unicode_char |= (unsigned char)input[i++] << 8;
        
        // 根据字符值转换为 UTF-8
        if (unicode_char <= 0x7F) { // 1 字节
            output[j++] = (char)unicode_char;
        } else if (unicode_char <= 0x7FF) { // 2 字节
            if (j + 1 >= output_len) return -1;
            output[j++] = 0xC0 | (unicode_char >> 6);
            output[j++] = 0x80 | (unicode_char & 0x3F);
        } else { // 3 字节
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

int main(int argc, const char *argv[]) {
    clock_t t0 = clock();
    
    PakInfo info;
    
    // 检查命令行参数
    if (argc != 2) {
        fprintf(stderr, "Usage %s <pak_file>\n", argv[0]);
        return 1;
    }

    // 检查文件是否存在
    if (access(argv[1], F_OK) == -1) {
        fprintf(stderr, "Input %s file does not exist.\n", argv[1]);
        return 1;
    }
    
    // 打开pak文件
    int PakFile = open(argv[1], O_RDONLY);
    
    if (PakFile == -1) {
        printf("Unable to open %s file\n", argv[1]);
        return 1;
    }
    
    // 获取文件总大小
    off_t total = lseek(PakFile, 0, SEEK_END); // 将文件指针移动到文件末尾，返回值是文件总大小
    lseek(PakFile, 0, SEEK_SET); // 将文件指针重新移回文件开头，确保后续读取从文件起始位置开始
    
    // 从文件末尾倒数45字节处读取文件头，如果定位失败（返回-1）
    if (lseek(PakFile, -45, SEEK_END) == -1) {
        printf("failed to seek file position\n");
        return 1;
    }
    
    if (read(PakFile, &info, 45) != 45) { // 从当前位置读取45字节到 info 结构体中
        printf("Failed to read pak header at -45\n");
        return 1;
    }
    
    // 文件头反混淆
    info.offset ^= OFFSET_KEY;
    info.encrypted ^= 0x6C;
    
    // 通过lseek计算索引数据的实际大小
    int64_t size = lseek(PakFile, -info.offset, SEEK_END);  
    size -= 45; // 索引数据大小 = (文件总大小) - (索引数据起始位置) - (文件头大小45)

    // 检查索引数据大小是否合理（小于50MB）
    if (size > 52428800) {
        fprintf(stderr, "Index data size is not compatible.\n");
        close(PakFile);
        return 1;
    }
    
    // 分配内存来存储整个文件索引数据
    uint8_t *IndexData = (uint8_t*)malloc(size);
    if (!IndexData) {
        printf("Memory allocation failed.\n");
        close(PakFile);
        return 1;
    }

    // 将索引数据读入内存 从 Pak 文件的 info.offset 位置开始，读取 size 字节的数据到 IndexData 内存区域
    if (pread(PakFile, IndexData, size, info.offset) != size) {
        fprintf(stderr, "Failed to load index data\n");
        return 1;
    }
    
    // 如果索引加密，则进行解密
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
    
    // 从索引数据中读取挂载点
    read_data(&MountPointLength, IndexData, 4);
    read_data(MountPoint, IndexData, MountPointLength);
    // 修正挂载点路径
    for (int x = 0; x < MountPointLength - 9; x++) {
        MountPoint[x] = MountPoint[x + 9];
    }

    // 打印从 .pak 文件索引中读取到的原始挂载点路径 Debug用
    printf("Debug: Original Mount Point Length = %u\n", MountPointLength);
    printf("Debug: Mount Point Path = %s\n", MountPoint);

    read_data(&NumOfEntry, IndexData, 4);

    // 分配内存来存储所有文件条目的元数据
    Entry *entry = (Entry*)malloc(NumOfEntry * sizeof(Entry));
    if (!entry) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(IndexData);
        close(PakFile);
        return 1;
    }
    
    // 遍历并读取所有文件条目的元数据  （解包onread关羽赵云宇宙等包时，从这里开始会遇到加密问题）
    for (uint32_t Files = 0; Files < NumOfEntry; Files++) {
        if(Files == 264) {
            printf("Debug: Reached file entry index 264\n");
        }
        read_data(entry[Files].FileHash, IndexData, 20);
        read_data(&entry[Files].FileOffset, IndexData, 8);
        read_data(&entry[Files].FileSize, IndexData, 8);
        read_data(&entry[Files].CompressionMethod, IndexData, 4);
        read_data(&entry[Files].CompressedLength, IndexData, 8);
        read_data(entry[Files].Dummy, IndexData, 21);

        // 以下是新增的打印语句
        printf("--- 文件条目 %u ---\n", Files);
        // 打印哈希值（以十六进制显示）
        printf("FileHash: ");
        for (int i = 0; i < 20; i++)
        {
            printf("%02x", entry[Files].FileHash[i]);
        }
        printf("\n");
        // 打印文件偏移量、原始大小、压缩方法和压缩后大小
        printf("FileOffset: 0x%llx (%llu)\n", entry[Files].FileOffset, entry[Files].FileOffset); // 同时打印十六进制和十进制
        printf("FileSize: %llu\n", entry[Files].FileSize);
        printf("CompressionMethod: %u\n", entry[Files].CompressionMethod);
        printf("CompressedLength: %llu\n", entry[Files].CompressedLength);

        if (entry[Files].CompressionMethod != 0 && entry[Files].CompressionMethod != 1) {
            // 数据可能被加密，需要解密
            printf("检测到可能加密的数据 (CompressionMethod = %u)\n", entry[Files].CompressionMethod);
            entry[Files].CompressionMethod^= 0x8924B0E3298B7069;
            printf("CompressionMethod: %u\n", entry[Files].CompressionMethod);
        }

        // 如果有压缩，则读取压缩块信息
        if (entry[Files].CompressionMethod != 0)
        {
            read_data(&entry[Files].NumOfBlocks, IndexData, 4);
            entry[Files].blocks = (CompressionBlock*)malloc(entry[Files].NumOfBlocks * sizeof(CompressionBlock));
            
            if (!entry[Files].blocks) {
                fprintf(stderr, "Memory allocation for blocks failed\n");
                return 0;
            }
            
            for (uint32_t i = 0; i < entry[Files].NumOfBlocks; i++) {
                read_data(&entry[Files].blocks[i].start, IndexData, 8);
                read_data(&entry[Files].blocks[i].end, IndexData, 8);
            }
        }
        else
        {
            entry[Files].NumOfBlocks = 0;
        }

        read_data(&entry[Files].CompressedBlockSize, IndexData, 4);
        read_data(&entry[Files].Encrypted, IndexData, 1);
        printf("CompressedBlockSize: %llu\n", entry[Files].CompressedBlockSize);
        printf("Encrypted: %u\n", entry[Files].Encrypted);
        printf("------------------------\n");
    }
    
    // 读取目录和文件映射表
    uint64_t ENTRIES = 0; // 未知用途
    uint64_t DIR_COUNT = 0; // 目录数量
    
    read_data(&ENTRIES, IndexData, 8);
    read_data(&DIR_COUNT, IndexData, 8);
    
    int32_t DIR_LEN = 0;
    char DIR_NAME[1024];
    uint64_t DIR_FILES = 0;
    int32_t ENTRY; // 索引，指向之前读取的Entry数组
    
    char path[1024];
    
    // 遍历所有目录
    for (int files = 0; files < DIR_COUNT; files++) {
        read_data(&DIR_LEN, IndexData, 4);
        read_data(DIR_NAME, IndexData, DIR_LEN);
        read_data(&DIR_FILES, IndexData, 8);
        
        // 遍历当前目录下的所有文件
        for (int x = 0; x < DIR_FILES; x++) {
            read_data(&FilenameSize, IndexData, 4);
            
            if (FilenameSize > 0) {
                read_data(Filename, IndexData, FilenameSize); // 如果文件名是 ASCII 编码 
            } else {
                // 如果文件名是 unicode 编码
                read_data(Filename, IndexData, -FilenameSize * 2);
                if (unicode_to_utf8(Filename, -FilenameSize * 2, Filename, sizeof(Filename)) == -1) {
                    printf("failed to convert UTF-16LE filename into UTF-8!\n");
                    exit(1);
                }
            }
            
            read_data(&ENTRY, IndexData, 4);
            
            // 构建完整的文件路径 
            memset(path, 0, 1024);
            snprintf(path, 1024, "%s%s%s", MountPoint, DIR_NAME, Filename);
            
            // 若目标特征符合，则保存目标为本地txt文件
            if ( strcmp(Filename, "BP_UGC_ShotGun_S12K.uasset") == 0 || strcmp(Filename, "BP_UGC_ShotGun_S12K.uexp") == 0 || strcmp(Filename, "BP_PlayerRifleBullet.uasset") == 0)
            {
                // 构建输出文件名：[Filename].txt
                char outputFilename[1024];
                snprintf(outputFilename, 1024, "%s_info.txt", Filename);
                FILE *logFile = fopen(outputFilename, "w");
                if (logFile == NULL) {
                    fprintf(stderr, "Error opening log file: %s\n", outputFilename);
                } else {
                    fprintf(logFile, "找到目标: %s\n", path);

                    // 打印 DIR_LEN DIR_NAME DIR_FILES FilenameSize Filename ENTRY 等原始索引键值对
                    fprintf(logFile, "\n--- 原始索引键值对 ---\n");
                    fprintf(logFile, "ENTRY Index: %d\n", ENTRY); // ENTRY 是索引
                    fprintf(logFile, "DIR_LEN: %d\n", DIR_LEN);
                    fprintf(logFile, "DIR_NAME: %s\n", DIR_NAME);
                    fprintf(logFile, "DIR_FILES (Files in Dir): %llu\n", DIR_FILES);
                    fprintf(logFile, "FilenameSize (Raw): %d\n", FilenameSize); // 负值表示UTF-16
                    fprintf(logFile, "Filename (UTF-8): %s\n", Filename);

                    // 打印 FileHash FileOffset FileSize CompressionMethod CompressedLength Dummy CompressedBlockSize Encrypted 等原始数据键值对
                    fprintf(logFile, "\n--- 原始数据键值对 ---\n");
                    
                    // 打印 FileHash (20字节数组)
                    fprintf(logFile, "FileHash: ");
                    for (int i = 0; i < 20; i++)
                    {
                        fprintf(logFile, "%02x", entry[ENTRY].FileHash[i]);
                    }
                    fprintf(logFile, "\n");
                    
                    fprintf(logFile, "FileOffset: 0x%llx (%llu)\n", entry[ENTRY].FileOffset, entry[ENTRY].FileOffset);
                    fprintf(logFile, "FileSize (Original): %llu\n", entry[ENTRY].FileSize);
                    fprintf(logFile, "CompressionMethod: %u\n", entry[ENTRY].CompressionMethod);
                    fprintf(logFile, "CompressedLength: %llu\n", entry[ENTRY].CompressedLength);
                    
                    // 打印 Dummy (21字节数组)
                    fprintf(logFile, "Dummy: ");
                    for (int i = 0; i < 21; i++)
                    {
                        fprintf(logFile, "%02x", entry[ENTRY].Dummy[i]);
                    }
                    fprintf(logFile, "\n");

                    fprintf(logFile, "NumOfBlocks: %u\n", entry[ENTRY].NumOfBlocks);
                    for (uint32_t i = 0; i < entry[ENTRY].NumOfBlocks; i++)
                    {
                        // 打印每个压缩块的起始和结束偏移
                        fprintf(logFile, "Block %u: Start = 0x%llx, End = 0x%llx\n", i, entry[ENTRY].blocks[i].start, entry[ENTRY].blocks[i].end);  
                    }
                    fprintf(logFile, "CompressedBlockSize: %u\n", entry[ENTRY].CompressedBlockSize);
                    fprintf(logFile, "Encrypted: %u\n", entry[ENTRY].Encrypted);

                    fclose(logFile);
                }

                printf("Found target file: %s, ENTRY: %d. Information saved to target_file_info.txt.\n", path, ENTRY);
            }

            // 调用提取函数，传入文件元数据和路径
            // extract(PakFile, entry[ENTRY], path);
        }
    }
    
    
    // 释放动态分配的内存
    for (uint32_t Files = 0; Files < NumOfEntry; Files++) {
        free(entry[Files].blocks);
    }
    free(entry);
    
    free(IndexData);
    close(PakFile);
    
    // 统计并打印结果
    clock_t t1 = clock();
    double time = ((double)(t1 - t0)) / CLOCKS_PER_SEC;
    const double MB = 1024.0 * 1024.0;
    printf("Processed %.2f MB, speed = %.2f MB/s, %u files found in %f seconds\n", total / MB, total / MB / time, NumOfEntry, time);
    return 0;
}

// 用于存放压缩和解压缩数据的全局缓冲区
uint8_t CompressedData[CHUNK_SIZE * 2];
uint8_t DecompressedData[CHUNK_SIZE * 2];
size_t DecompressLength = 0;

// 文件提取函数
void extract(int PakFile, Entry entry, char *filename) {
    // 创建输出文件
    int OutFile = create_file(filename);
    
    if (OutFile == -1) {
        printf("Failed to open output file: %s\n", filename);
        exit(1);
    }
    
    // 如果文件是压缩的
    if (entry.NumOfBlocks > 0) {
        for (int x = 0; x < entry.NumOfBlocks; x++) {
            // 读取一个压缩块
            if (pread(PakFile, CompressedData, entry.blocks[x].end - entry.blocks[x].start, entry.blocks[x].start) != entry.blocks[x].end - entry.blocks[x].start) {
                printf("Failed to read compressed chunk at %lx\n", entry.blocks[x].start);
                exit(1);
            }
            
            // 如果文件内容加密，则解密
            if (entry.Encrypted) {
                DecryptData(CompressedData, entry.blocks[x].end - entry.blocks[x].start);
            }
            
            // 根据压缩方法进行解压缩
            if (entry.CompressionMethod == 1) { // ZLIB
                if ((DecompressLength = ZLIB_decompress(CompressedData, entry.blocks[x].end - entry.blocks[x].start, DecompressedData, CHUNK_SIZE)) == 0) {
                    fprintf(stderr, "ZLIB Decompression failed\n");
                    exit(1);
                }
            } else {
                printf("Unknown compression method %u\n", entry.CompressionMethod);
                close(OutFile);
                exit(1);
            }
            
            // 将解压后的数据写入文件
            write(OutFile, DecompressedData, DecompressLength);
            printf("%016lx %lu\t%s\n", entry.blocks[x].start, DecompressLength, filename);
        }
    } else { // 如果文件未压缩
        // 移动文件指针到文件数据的起始位置
        lseek(PakFile, entry.FileOffset + 74, SEEK_SET);
        
        ssize_t bytesRead, bytesWritten;
        
        // 循环读取并写入文件
        while (entry.FileSize > 0) {
            size_t bytesToRead = entry.FileSize < CHUNK_SIZE ? entry.FileSize : CHUNK_SIZE;
            bytesRead = read(PakFile, DecompressedData, bytesToRead);
            
            if (bytesRead < 0) {
                printf("Error reading from file\n");
            }
            
            if (entry.Encrypted) {
                DecryptData(DecompressedData, bytesToRead);
            }
            
            bytesWritten = write(OutFile, DecompressedData, bytesRead);
            
            if (bytesWritten != bytesRead) {
                printf("Error writing to output file\n");
            }
            
            entry.FileSize -= bytesRead;
            printf("%016lx %zd\t%s\n", entry.FileOffset, bytesWritten, filename);
        }
    }
    
    close(OutFile);
    return;
}