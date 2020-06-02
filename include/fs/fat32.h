#ifndef _FAT32_H
#define _FAT32_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct __attribute__((packed, aligned(1))) fat_ext_bpb_32 {
    uint32_t fat_table_size_32;
    uint16_t extended_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fat_info;
    uint16_t backup_BS_sector;
    uint8_t reserved_0[12];
    uint8_t drive_number;
    uint8_t reserved_1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fat_type_label[8];
};

void doNothingFat32();

#ifdef __cplusplus
}
#endif

#endif