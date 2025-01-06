#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef uint8_t bool;
#define true 1
#define false 0

typedef struct 
{
    uint8_t boot_jump_instruction[3];
    uint8_t oem_identifier[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t dir_entry_count;
    uint16_t total_sectors;
    uint8_t media_descriptor_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t large_sector_count;

    // extended boot record
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t signature;
    uint32_t volume_id;          // serial number, value doesn't matter
    uint8_t volume_label[11];    // 11 bytes, padded with spaces
    uint8_t system_id[8];

    // ... we don't care about code ...

} __attribute__((packed)) BootSector;

typedef struct 
{
    uint8_t name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t created_time_tenths;
    uint16_t created_time;
    uint16_t created_date;
    uint16_t accessed_date;
    uint16_t first_cluster_high;
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) DirectoryEntry;


BootSector g_boot_sector;
uint8_t* g_fat = NULL;
DirectoryEntry* g_root_directory = NULL;
uint32_t g_root_directory_end;


bool ReadBootSector(FILE* disk)
{
    return fread(&g_boot_sector, sizeof(g_boot_sector), 1, disk) > 0;
}

bool ReadSectors(FILE* disk, uint32_t lba, uint32_t count, void* buffer_out)
{
    bool ok = true;
    ok = ok && (fseek(disk, lba * g_boot_sector.bytes_per_sector, SEEK_SET) == 0);
    ok = ok && (fread(buffer_out, g_boot_sector.bytes_per_sector, count, disk) == count);
    return ok;
}

bool ReadFat(FILE* disk)
{
    g_fat = (uint8_t*) malloc(g_boot_sector.sectors_per_fat * g_boot_sector.bytes_per_sector);
    if (!g_fat) {
        fprintf(stderr, "Failed to allocate memory for FAT\n");
        return false;
    }
    return ReadSectors(disk, g_boot_sector.reserved_sectors, g_boot_sector.sectors_per_fat, g_fat);
}

bool ReadRootDirectory(FILE* disk)
{
    uint32_t lba = g_boot_sector.reserved_sectors + g_boot_sector.sectors_per_fat * g_boot_sector.fat_count;
    uint32_t size = sizeof(DirectoryEntry) * g_boot_sector.dir_entry_count;
    uint32_t sectors = (size / g_boot_sector.bytes_per_sector);
    if (size % g_boot_sector.bytes_per_sector > 0)
        sectors++;

    g_root_directory_end = lba + sectors;
    g_root_directory = (DirectoryEntry*) malloc(sectors * g_boot_sector.bytes_per_sector);
    if (!g_root_directory) {
        fprintf(stderr, "Failed to allocate memory for root directory\n");
        return false;
    }
    return ReadSectors(disk, lba, sectors, g_root_directory);
}

DirectoryEntry* FindFile(const char* name)
{
    for (uint32_t i = 0; i < g_boot_sector.dir_entry_count; i++)
    {
        if (memcmp(name, g_root_directory[i].name, 11) == 0)
            return &g_root_directory[i];
    }

    return NULL;
}

bool ReadFile(DirectoryEntry* file_entry, FILE* disk, uint8_t* output_buffer)
{
    bool ok = true;
    uint16_t current_cluster = file_entry->first_cluster_low;

    do {
        uint32_t lba = g_root_directory_end + (current_cluster - 2) * g_boot_sector.sectors_per_cluster;
        ok = ok && ReadSectors(disk, lba, g_boot_sector.sectors_per_cluster, output_buffer);
        output_buffer += g_boot_sector.sectors_per_cluster * g_boot_sector.bytes_per_sector;

        uint32_t fat_index = current_cluster * 3 / 2;
        if (current_cluster % 2 == 0)
            current_cluster = (*(uint16_t*)(g_fat + fat_index)) & 0x0FFF;
        else
            current_cluster = (*(uint16_t*)(g_fat + fat_index)) >> 4;

    } while (ok && current_cluster < 0x0FF8);

    return ok;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "Syntax: %s <disk image> <file name>\n", argv[0]);
        return -1;
    }

    FILE* disk = fopen(argv[1], "rb");
    if (!disk) {
        fprintf(stderr, "Cannot open disk image %s!\n", argv[1]);
        return -1;
    }

    if (!ReadBootSector(disk)) {
        fprintf(stderr, "Could not read boot sector!\n");
        fclose(disk);
        return -2;
    }

    if (!ReadFat(disk)) {
        fprintf(stderr, "Could not read FAT!\n");
        free(g_fat);
        fclose(disk);
        return -3;
    }

    if (!ReadRootDirectory(disk)) {
        fprintf(stderr, "Could not read root directory!\n");
        free(g_fat);
        fclose(disk);
        return -4;
    }

    DirectoryEntry* file_entry = FindFile(argv[2]);
    if (!file_entry) {
        fprintf(stderr, "Could not find file %s!\n", argv[2]);
        free(g_fat);
        free(g_root_directory);
        fclose(disk);
        return -5;
    }

    uint8_t* buffer = (uint8_t*) malloc(file_entry->size + g_boot_sector.bytes_per_sector);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for file buffer\n");
        free(g_fat);
        free(g_root_directory);
        fclose(disk);
        return -6;
    }

    if (!ReadFile(file_entry, disk, buffer)) {
        fprintf(stderr, "Could not read file %s!\n", argv[2]);
        free(g_fat);
        free(g_root_directory);
        free(buffer);
        fclose(disk);
        return -7;
    }

    for (size_t i = 0; i < file_entry->size; i++)
    {
        if (isprint(buffer[i])) fputc(buffer[i], stdout);
        else printf("<%02x>", buffer[i]);
    }
    printf("\n");

    free(buffer);
    free(g_fat);
    free(g_root_directory);
    fclose(disk);
    return 0;
}