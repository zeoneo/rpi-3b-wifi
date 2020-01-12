#ifndef _FAT16_H
#define _FAT16_H

#ifdef __cplusplus
extern "C"
{
#endif

#include<stdint.h>

struct  __attribute__((packed, aligned(1))) fat_ext_bpb_16
{
	//extended fat12 and fat16 stuff
	uint8_t		bios_drive_num;
	uint8_t		reserved1;
	uint8_t		boot_signature;
	uint32_t	volume_id;
	uint8_t		volume_label[11];
	uint8_t		fat_type_label[8];
 
};

void doNothingFat16();

#ifdef __cplusplus
}
#endif

#endif