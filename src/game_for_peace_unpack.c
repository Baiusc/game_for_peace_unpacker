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

// Pak file constants
#define OFFSET_KEY 0xD74AF37FAA6B020D
#define SIZE_KEY 0x8924B0E3298B7069
#define CHUNK_SIZE 65536

// Pak file header struct, located at the end of the file
typedef struct {
    uint8_t encrypted;
    uint32_t magic;
    uint32_t version;
    uint8_t hash[20];
    uint64_t size;
    uint64_t offset;
} __attribute__((packed)) PakInfo;

// Compression block struct
typedef struct {
    uint64_t start;
    uint64_t end;
} __attribute__((packed)) CompressionBlock;

// Single file entry metadata struct
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
    char *filename; // Used for repacking to store file names
} __attribute__((packed)) Entry;

// File counters for creating file names like bms script
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

// New ZLIB compress function
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

// Extraction function, now accepts output directory as a parameter
void extract(int PakFile, Entry entry, const char* output_dir) {
    char filename[1024];
    
    if (counters.files_in_folder >= 1000) {
        counters.folder_index++;
        counters.files_in_folder = 0;
    }

    // Naming convention for folders: file_0, file_1, etc.
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
    } else { // If file is not compressed
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

// Unpack function
void unpack_pak(const char *pak_file, const char *output_dir) {
    // Debug info: confirm received file paths
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

// Repack function
void repack_pak(const char *input_pak_file, const char *output_pak_file) {
    printf("Repacking data from '%s' into new file '%s'...\n", input_pak_file, output_pak_file);

    int InPakFile = open(input_pak_file, O_RDONLY);
    if (InPakFile == -1) {
        perror("Failed to open input pak file");
        return;
    }

    // Use stat to get file size
    struct stat st;
    if (stat(input_pak_file, &st) == -1) {
        perror("Failed to get input file size");
        close(InPakFile);
        return;
    }
    
    // Read the PakInfo to find the index
    PakInfo info;
    lseek(InPakFile, -45, SEEK_END);
    if (read(InPakFile, &info, 45) != 45) {
        perror("Failed to read pak header");
        close(InPakFile);
        return;
    }
    info.offset ^= OFFSET_KEY;
    info.encrypted ^= 0x6c;
    
    // Calculate the size of the index data
    uint64_t index_size = st.st_size - info.offset - 45;
    printf("Calculated index size: %lu bytes\n", index_size);

    // Allocate buffer for index data
    uint8_t *IndexData = (uint8_t*)malloc(index_size);
    if (!IndexData) {
        perror("Memory allocation for index failed");
        close(InPakFile);
        return;
    }

    // Read the old index from the input file
    if (pread(InPakFile, IndexData, index_size, info.offset) != index_size) {
        perror("Failed to load index data from input pak");
        free(IndexData);
        close(InPakFile);
        return;
    }

    // Open the output file
    int OutPakFile = open(output_pak_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (OutPakFile == -1) {
        perror("Failed to open output pak file");
        free(IndexData);
        close(InPakFile);
        return;
    }
    
    // Copy the entire data block from the old file to the new file
    off_t data_size_to_copy = info.offset;
    printf("Data size to copy: %ld bytes (%.2f KB)\n", data_size_to_copy, data_size_to_copy / 1024.0);

    uint8_t *buffer = (uint8_t*)malloc(CHUNK_SIZE);
    if (!buffer) {
        perror("Memory allocation for buffer failed");
        free(IndexData);
        close(InPakFile);
        close(OutPakFile);
        return;
    }
    
    lseek(InPakFile, 0, SEEK_SET);
    off_t bytes_copied = 0;
    while (bytes_copied < data_size_to_copy) {
        size_t bytes_to_read = (data_size_to_copy - bytes_copied > CHUNK_SIZE) ? CHUNK_SIZE : (data_size_to_copy - bytes_copied);
        ssize_t bytes_read = read(InPakFile, buffer, bytes_to_read);
        if (bytes_read <= 0) {
            perror("Error reading from input pak");
            break;
        }
        write(OutPakFile, buffer, bytes_read);
        bytes_copied += bytes_read;
    }

    free(buffer);
    
    // Now get the new index offset
    uint64_t new_index_offset = lseek(OutPakFile, 0, SEEK_CUR);
    printf("New index offset: %lu\n", new_index_offset);

    // Write the old index to the new file
    if (info.encrypted) {
        // Need to decrypt it first if we read it encrypted
        DecryptData(IndexData, index_size);
    }
    write(OutPakFile, IndexData, index_size);
    
    // Finally, write the new PakInfo
    PakInfo new_info = info;
    new_info.offset = new_index_offset ^ OFFSET_KEY;
    // Update the size field with the calculated index size
    new_info.size = index_size;
    // The header itself isn't encrypted in this version
    write(OutPakFile, &new_info, sizeof(PakInfo));

    free(IndexData);
    close(InPakFile);
    close(OutPakFile);
    
    struct stat st_out;
    if (stat(output_pak_file, &st_out) == -1) {
        perror("Failed to get output file size");
        return;
    }
    
    printf("Repacking completed successfully. Old size: %ld bytes, New size: %ld bytes.\n", st.st_size, st_out.st_size);
}

int main(void) {
    // You can change this variable to switch modes
    // Set to true to execute repack
    // Set to false to execute unpack
    bool repack_mode = true;

    const char* pak_path = "../paks/game_patch_1.33.12.14227.pak";
    const char* dat_path = "../paks/dat_temp";

    if (repack_mode) {
        // --- Repack Mode ---
        repack_pak(pak_path, "repacked.pak");
    } else {
        // --- Unpack Mode ---
        unpack_pak(pak_path, dat_path);
    }

    return 0;
}
