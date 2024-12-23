#include <stdio.h>
#include <stdint.h>

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

BootSector g_BootSector;

bool readBoolSector(FILE* disk) {
    return fread(&g_BootSector, sizeof(BootSector), 1, disk);
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

    return 0;
}
