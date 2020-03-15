#ifndef _FAT_H
#define _FAT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include <fs/fat16.h>
#include <fs/fat32.h>

	typedef enum
	{
		FAT_NULL = 0,
		FAT12 = 1,
		FAT16 = 2,
		FAT32 = 3
	} fat_type;

	struct __attribute__((packed, aligned(4))) fat_bpb
	{
		uint8_t bootjmp[3];
		uint8_t oem_name[8];
		uint16_t bytes_per_sector;
		uint8_t sectors_per_cluster;
		uint16_t reserved_sector_count;
		uint8_t fat_table_count;
		uint16_t root_entry_count;
		// uint16_t    root_entry_count; Alignment issues
		uint16_t total_sectors_16;
		uint8_t media_type;
		uint16_t fat16_table_size;
		uint16_t sectors_per_track;
		uint16_t head_side_count;
		uint32_t hidden_sector_count;
		uint32_t fat32_total_sectors;

		//this will be cast to it's specific type once the driver actually knows what type of FAT this is.
		union {
			struct fat_ext_bpb_16 ex_fat1612; // Fat 16/12 extended bpb structure
			struct fat_ext_bpb_32 ex_fat32;   // Fat 32 extended bpb structure
		} fat_type_data;
	};

	typedef char lfn_name_t[256]; // long file name
	typedef char sfn_name_t[11];  // short file name

	struct __attribute__((packed, aligned(1))) dir_sfn_entry
	{
		sfn_name_t short_file_name;
		uint8_t file_attrib;
		uint8_t reserved; // always 0
		uint8_t time_in_tenth_of_sec;
		uint16_t last_write_time;
		uint16_t last_write_date;
		uint16_t last_access_date;
		uint16_t first_cluster_high; // This along with lower bytes form first cluster where data is stored.
		uint16_t file_creation_time;
		uint16_t file_creation_date;
		uint16_t first_cluster_low;
		uint32_t file_size_bytes;
	};

	struct __attribute__((packed, aligned(1))) dir_lfn_entry
	{
		uint8_t LDIR_SeqNum;		  // Long directory order
		uint8_t LDIR_Name1[10];		  // Characters 1-5 of long name (UTF 16) but misaligned again
		uint8_t LDIR_Attr;			  // Long directory attribute .. always 0x0f
		uint8_t LDIR_Type;			  // Long directory type ... always 0x00
		uint8_t LDIR_ChkSum;		  // Long directory checksum
		uint16_t LDIR_Name2[6];		  // Characters 6-11 of long name (UTF 16)
		uint16_t LDIR_firstClusterLO; // First cluster lo
		uint16_t LDIR_Name3[2];		  // Characters 12-13 of long name (UTF 16)
	};

typedef bool (*read_handler_t)(uint32_t, uint32_t, uint8_t *);

	bool initialize_fat(read_handler_t read_handler);
	void print_root_directory_info();
	uint32_t get_file_size(uint8_t *absolute_file_name);
	void read_file(uint8_t *absolute_file_name);
#ifdef __cplusplus
}
#endif

#endif