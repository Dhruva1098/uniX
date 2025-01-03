#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> 

// No bool types in C
typedef uint8_t bool;
#define true 1
#define false 0

// Boot sector structure
typedef struct {
    uint8_t BootJumpInstruction[3];
    uint8_t OEMIdentifier[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t FatCount;
    uint16_t DirectoryEntryCount;
    uint16_t TotalSectors;
    uint16_t MediaDescriptorType;
    uint16_t SectorsPerFat;
    uint16_t SectorsPerTrack;
    uint16_t HeadCount;
    uint32_t HiddenSectors;
    uint32_t TotalSectorsBig;

    //extended boot record
    uint8_t DriveNumber;
    uint8_t Reserved;
    uint8_t BootSignature;
    uint32_t VolumeID;
    uint8_t VolumeLabel[11];
    uint8_t SystemId[8];
} __attribute__((packed)) BootSector;  // packed attribute is used to prevent the compiler from padding the structure

typedef struct
{
    uint8_t Name[11];
    uint8_t Attributes;
    uint8_t Reserved;
    uint8_t CreationTimeTenths;
    uint16_t CreationTime;
    uint16_t CreationDate;
    uint16_t LastAccessDate;
    uint16_t FirstClusterHigh;
    uint16_t LastWriteTime;
    uint16_t LastWriteDate;
    uint16_t FirstClusterLow;
    uint32_t FileSize;
} __attribute__((packed)) DirectoryEntry;

BootSector g_BootSector;
uint8_t* g_Fat = NULL;
DirectoryEntry* g_RootDirectory = NULL;

bool readBoolSector(FILE* disk) {
    return fread(&g_BootSector, sizeof(BootSector), 1, disk);
}

bool readSectors(FILE* disk, uint32_t lba, uint32_t count, void* bufferOut) {
    bool ok = true;
    printf("Seeking to LBA: %u\n", lba);
    if (fseek(disk, lba * g_BootSector.BytesPerSector, SEEK_SET) != 0) {
        printf("fseek failed\n");
        ok = false;
    }
    printf("Reading %u sectors\n", count);
    size_t sectorsRead = fread(bufferOut, g_BootSector.BytesPerSector, count, disk);
    if (sectorsRead != count) {
        printf("fread failed, read %zu sectors instead of %u\n", sectorsRead, count);
        printf("Error: %s\n", strerror(ferror(disk)));
        ok = false;
    }
    return ok;
}

bool readFat(FILE* disk) {
    printf("BootSector.BytesPerSector: %u\n", g_BootSector.BytesPerSector);
    printf("BootSector.SectorsPerFat: %u\n", g_BootSector.SectorsPerFat);
    printf("BootSector.ReservedSectors: %u\n", g_BootSector.ReservedSectors);

    g_Fat = (uint8_t*)malloc(g_BootSector.SectorsPerFat * g_BootSector.BytesPerSector);
    if(!g_Fat) {
        printf("Failed to allocate memory for FAT\n");
        return false;
    }
    bool result = readSectors(disk, g_BootSector.ReservedSectors, g_BootSector.SectorsPerFat, g_Fat);
    if (!result) {
        printf("Failed to read FAT sectors\n");
    }
    return result;
}

bool readRootDirectory(FILE* disk) {
    uint32_t lba = g_BootSector.ReservedSectors + (g_BootSector.FatCount * g_BootSector.SectorsPerFat);
    uint32_t size = sizeof(DirectoryEntry) * g_BootSector.DirectoryEntryCount;
    uint32_t count = size / g_BootSector.BytesPerSector;
    if(size % g_BootSector.BytesPerSector ) {
        count++;
    }
    g_RootDirectory = (DirectoryEntry*)malloc(count * g_BootSector.BytesPerSector); //  Not size because read sectors take sectors
    return readSectors(disk, lba, count, g_RootDirectory);
}

DirectoryEntry* findFile(const char* filename) {
    for (int i = 0; i < g_BootSector.DirectoryEntryCount; i++) {
        if (memcmp(g_RootDirectory[i].Name, filename, 11) == 0) {
            return &g_RootDirectory[i];
        }
    }
    return NULL;
}

int main(int argc, char** argv){
    
    if(argc < 3) {
        printf("Invalid syntax. Syntax: %s <disk image> <file name> \n", argv[0]);
        return 1;
    }

    FILE* disk = fopen(argv[1], "rb");
    if(!disk){
        fprintf(stderr, "Error: Unable to open disk image %s\n", argv[1]);
        return -1;
    }

    if(!readBoolSector(disk)){
        fprintf(stderr, "Error: Unable to read boot sector\n");
        fclose(disk);
        return -2;
    }

    if(!readFat(disk)){
        fprintf(stderr, "Error: Unable to read FAT\n");
        free(g_Fat);
        fclose(disk);
        return -3;
    }

    if(!readRootDirectory(disk)){
        fprintf(stderr, "Error: Unable to read root directory\n");
        free(g_Fat);
        fclose(disk);
        return -4;
    }

    DirectoryEntry* entry = findFile(argv[2]);
    if(!entry){
        fprintf(stderr, "Error: File not found\n");
        free(g_Fat);
        free(g_RootDirectory);
        fclose(disk);
        return -5;
    }

    free(g_Fat);
    return 0;
}
