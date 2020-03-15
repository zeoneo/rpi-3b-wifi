#include <string.h>
#include <fs/fat.h>
#include <device/emmc.h>
#include <plibc/stdio.h>

struct __attribute__((packed, aligned(2))) partition_info
{
    uint8_t status;        // 0x80 - active partition
    uint8_t headStart;     // starting head
    uint16_t cylSectStart; // starting cylinder and sector
    uint8_t type;          // partition type (01h = 12bit FAT, 04h = 16bit FAT, 05h = Ex MSDOS, 06h = 16bit FAT (>32Mb), 0Bh = 32bit FAT (<2048GB))
    uint8_t headEnd;       // ending head of the partition
    uint16_t cylSectEnd;   // ending cylinder and sector
    uint32_t firstSector;  // total sectors between MBR & the first sector of the partition
    uint32_t sectorsTotal; // size of this partition in sectors
} __packed;

struct __attribute__((packed, aligned(4))) mbr
{
    uint8_t nothing[446];                   // Filler the gap in the structure
    struct partition_info partitionData[4]; // partition records (16x4)
    uint16_t signature;                     // 0xaa55
};

// uint8_t file_to_search[] = "/SpaceCraft/Runner/color_0.jpg";
uint8_t file_to_search[] = "/COPYING.linux";

// uint8_t file_to_search[] = "/overlays/allo-piano-dac-plus-pcm512x-audio.dtbo";

struct __attribute__((packed, aligned(4))) sd_partition
{
    uint32_t rootCluster;         // Active partition rootCluster
    uint32_t sectorPerCluster;    // Active partition sectors per cluster
    uint32_t bytesPerSector;      // Active partition bytes per sector
    uint32_t firstDataSector;     // Active partition first data sector
    uint32_t dataSectors;         // Active partition data sectors
    uint32_t unusedSectors;       // Active partition unused sectors
    uint32_t reservedSectorCount; // Active partition reserved sectors,
    uint32_t fatSize;
};

typedef enum
{
    ATTR_FILE_EMPTY = 0x00,
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN = 0x02,
    ATTR_SYSTEM = 0x04,
    ATTR_FILE_LABEL = 0x8,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE = 0x20,
    ATTR_DEVICE = 0x40,
    ATTR_NORMAL = 0x80,
    ATTR_FILE_DELETED = 0xE5,
    ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_FILE_LABEL,
    ATTR_LONG_NAME_MASK = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_FILE_LABEL | ATTR_DIRECTORY | ATTR_ARCHIVE
} FILE_ATTRIBUTE;

static fat_type selected_fat_type = FAT_NULL;
static struct sd_partition current_sd_partition = {0};

struct __attribute__((packed, aligned(1))) dir_Structure
{
    sfn_name_t name;         // 8.3 filename
    uint8_t attrib;          // file attributes
    uint8_t NTreserved;      // always 0
    uint8_t timeTenth;       // tenths of seconds, set to 0 here
    uint16_t writeTime;      // time file was last written
    uint16_t writeDate;      // date file was last written
    uint16_t lastAccessDate; // date file was last accessed
    uint16_t firstClusterHI; // higher word of the first cluster number
    uint16_t createTime;     // time file was created
    uint16_t createDate;     // date file was created
    uint16_t firstClusterLO; // lower word of the first cluster number
    uint32_t fileSize;       // size of file in bytes
};

void getFatEntrySectorAndOffset(uint32_t cluster_number, uint32_t *fat_sector_num, uint32_t *fat_entry_offset);
uint32_t getFirstSectorOfCluster(uint32_t cluster_number);
void print_entries_in_sector(uint32_t sector_number, uint8_t *buffer);
void print_directory_cluster_contents(uint32_t directory_first_sector);
void print_directory_contents(uint32_t directory_first_cluster);
bool CopyUnAlignedString(char *Dest, const char *Src, uint32_t size);
void print_fat_sector_content(uint32_t cluster_number);
static uint8_t ReadLFNEntry(struct dir_lfn_entry *LFdir, char LFNtext[14]);
uint32_t search_directory_contents(uint32_t directory_first_cluster, uint8_t *next_dir_name, uint32_t next_index);
uint32_t search_directory_cluster_contents(uint32_t directory_first_sector, uint8_t *next_dir_name, uint32_t next_index);
uint32_t search_entries_in_sector(uint32_t sector_number, uint8_t *buffer, uint8_t *next_dir_name, uint32_t next_index);

static read_handler_t block_read_handler;

bool initialize_fat(read_handler_t read_handler)
{
    if(read_handler == (void *) 0) {
        return false;
    }

    // Save function pointer to read handler 
    block_read_handler = read_handler;

    uint8_t num_of_blocks = 1;

    uint8_t bpb_buffer[512] __attribute__((aligned(4)));
    if (!block_read_handler(0, num_of_blocks, (uint8_t *)&bpb_buffer[0]))
    {
        printf("FAT: UNABLE to read first block in sd card. \n");
        return false;
    }

    struct fat_bpb *bpb = (struct fat_bpb *)bpb_buffer;
    if (bpb->bootjmp[0] != 0xE9 && bpb->bootjmp[0] != 0xEB)
    {
        struct mbr *mbr_info = (struct mbr *)&bpb_buffer[0];
        if (mbr_info->signature != 0xaa55)
        {
            printf("BPP is not boot sector or MBR. Corrupted data ???. \n");
            return false;
        }
        else
        {
            printf("Got MBR.. \n");
            struct partition_info *pd = &mbr_info->partitionData[0];
            current_sd_partition.unusedSectors = pd->firstSector;
            // Read first unused sector i.e. 512 Bytes
            if (!block_read_handler(pd->firstSector, 1, (uint8_t *)&bpb_buffer[0]))
            {
                printf("Could not mbr first sector. \n");
                return false;
            }
            // No need to map bpb_buffer to fat_bpb struct again, but this is easy to understand
            // struct fat_bpb *mbr_bpb = (struct fat_bpb *)bpb_buffer;
            if (bpb->bootjmp[0] != 0xE9 && bpb->bootjmp[0] != 0xEB)
            {
                printf("MBR doesn't have valid FAT_BPB sector. \n");
                return false;
            }
            else
            {
                printf("Got valid FAT_BPB for given MBR. \n");
            }
        }
    }
    else
    {
        printf("Got BOOT SECTOR.. \n");
    }

    current_sd_partition.bytesPerSector = bpb->bytes_per_sector;      // Bytes per sector on partition
    current_sd_partition.sectorPerCluster = bpb->sectors_per_cluster; // Hold the sector per cluster count
    current_sd_partition.reservedSectorCount = bpb->reserved_sector_count;

    if ((bpb->fat16_table_size == 0) && (bpb->root_entry_count == 0))
    {
        selected_fat_type = FAT32;
        printf("Got FAT 32 FS. \n");

        current_sd_partition.rootCluster = bpb->fat_type_data.ex_fat32.root_cluster; // Hold partition root cluster
        current_sd_partition.firstDataSector = bpb->reserved_sector_count + bpb->hidden_sector_count + (bpb->fat_type_data.ex_fat32.fat_table_size_32 * bpb->fat_table_count);
        current_sd_partition.dataSectors = bpb->fat32_total_sectors - bpb->reserved_sector_count - (bpb->fat_type_data.ex_fat32.fat_table_size_32 * bpb->fat_table_count);
        current_sd_partition.fatSize = bpb->fat_type_data.ex_fat32.fat_table_size_32;
        printf("ex_fat32 Volume Label: %s \n", bpb->fat_type_data.ex_fat32.volume_label); // Basic detail print if requested
    }
    else
    {
        selected_fat_type = FAT16;
        printf("Got FAT 16 FS. \n");

        current_sd_partition.fatSize = bpb->fat16_table_size;
        current_sd_partition.rootCluster = 2; // Hold partition root cluster, FAT16 always start at 2
        current_sd_partition.firstDataSector = current_sd_partition.unusedSectors + (bpb->fat_table_count * bpb->fat16_table_size) + 1;
        // data sectors x sectorsize = capacity ... I have check this on PC and gives right calc
        current_sd_partition.dataSectors = bpb->fat32_total_sectors - (bpb->fat_table_count * bpb->fat16_table_size) - 33; // -1 see above +1 and 32 fixed sectors
        if (bpb->fat_type_data.ex_fat1612.boot_signature == 0x29)
        {
            printf("FAT12/16 Volume Label: %s\n", bpb->fat_type_data.ex_fat1612.volume_label); // Basic detail print if requested
        }
    }

    uint32_t partition_totalClusters = current_sd_partition.dataSectors / current_sd_partition.sectorPerCluster;
    printf("First Sector: %lu, Data Sectors: %lu, TotalClusters: %lu, RootCluster: %lu\n",
           (unsigned long)current_sd_partition.firstDataSector, (unsigned long)current_sd_partition.dataSectors,
           (unsigned long)partition_totalClusters, (unsigned long)current_sd_partition.rootCluster);

    printf("Reserved sectors :%lu Fat size : %d bytesPerSector: %d \n", current_sd_partition.reservedSectorCount, current_sd_partition.fatSize, current_sd_partition.bytesPerSector);
    return true;
}
void readFat(uint32_t clusterNumber);
void print_root_directory_info()
{
    uint32_t root_directory_cluster_number = current_sd_partition.rootCluster;
    print_directory_contents(root_directory_cluster_number);
    // printf("Files first Cluster: 0x%x \n", get_file_size(&file_to_search[0]));
    // read_file(&file_to_search[0]);
    // print_fat_sector_content(0xbd);
    // readFat(0x10a5);
}

uint32_t find_file_in_directory(uint32_t directory_cluster, uint8_t *absolute_file_name, uint32_t next_index);
uint32_t get_file_size(uint8_t *absolute_file_name)
{
    if (absolute_file_name == 0 || absolute_file_name[0] != '/')
    {
        return 0;
    }

    return find_file_in_directory(current_sd_partition.rootCluster, absolute_file_name, 1);
}

uint8_t get_next_dir_name(uint8_t *absolute_file_name, uint8_t *dest)
{
    uint8_t count = 0;
    uint8_t *src_ptr = &absolute_file_name[0];
    // printf("SRC FILE name : %s \n", absolute_file_name);
    while (*src_ptr != '\0' && *src_ptr != '/')
    {
        dest[count++] = *src_ptr;
        src_ptr++;
    }
    return count;
}

uint32_t find_file_in_directory(uint32_t directory_cluster, uint8_t *absolute_file_name, uint32_t next_index)
{
    uint8_t next_dir_name[256] = {'\0'};
    uint8_t next_length = get_next_dir_name(&absolute_file_name[next_index], &next_dir_name[0]);
    if (next_length == 0)
    {
        printf("Can't find directory/file %s \n", absolute_file_name);
    }
    return search_directory_contents(directory_cluster, &absolute_file_name[0], next_index);
}

uint32_t search_directory_contents(uint32_t directory_first_cluster, uint8_t *next_dir_name, uint32_t next_index)
{
    printf("--------------*****************----------------------\n");
    uint8_t fat_sector_buffer[512] __attribute__((aligned(4)));
    uint32_t cluster_number = directory_first_cluster;

    // Now it's time to check if we have multiple clusters for this directory using fat table.
    uint32_t fat_sector_num, fat_entry_offset;
    uint32_t old_fat_sector_num = 0;
    while (cluster_number != 0x0fffffff && cluster_number != 0x0FFFFFF7 && cluster_number != 0)
    {
        getFatEntrySectorAndOffset(cluster_number, &fat_sector_num, &fat_entry_offset);

        if (old_fat_sector_num != fat_sector_num)
        {
            // printf("Reading fat for cluster:%d fat_sector_number:%d ", cluster_number, fat_sector_num);
            if (!block_read_handler(fat_sector_num, 1, (uint8_t *)&fat_sector_buffer[0]))
            {
                printf("FAT: Could not read FAT sector :%d. \n", fat_sector_num);
                return 0;
            }
        }

        uint32_t first_cluster = search_directory_cluster_contents(cluster_number, &next_dir_name[0], next_index);

        if (first_cluster != 0)
        {
            return first_cluster;
        }

        uint32_t *fat_entry_ptr = (uint32_t *)&fat_sector_buffer[fat_entry_offset];
        cluster_number = (*fat_entry_ptr) & 0x0fffffff;
        old_fat_sector_num = fat_sector_num;
    }
    printf("--------------####################----------------------\n");
    return 0;
}

uint32_t search_directory_cluster_contents(uint32_t directory_first_sector, uint8_t *next_dir_name, uint32_t next_index)
{
    uint32_t first_root_dir_sector = getFirstSectorOfCluster(directory_first_sector);
    uint32_t last_clust_root_dir_sector = first_root_dir_sector + current_sd_partition.sectorPerCluster;
    uint32_t sector_number = first_root_dir_sector;
    uint8_t buffer[512] __attribute__((aligned(4)));

    while (sector_number < last_clust_root_dir_sector)
    {
        uint32_t first_cluster = search_entries_in_sector(sector_number, &buffer[0], &next_dir_name[0], next_index) ;
        if (first_cluster != 0)
        {
            return first_cluster;
        }
        sector_number++;
    }
    return 0;
}

bool is_same_file(uint8_t *src, uint8_t *dest)
{
    while (*src != '\0' && *dest != '\0')
    {
        if (*src != *dest)
        {
            return false;
        }
        src++;
        dest++;
    }
    if (*src != '\0' || *dest != '\0')
    {
        return false;
    }
    return true;
}

uint32_t search_entries_in_sector(uint32_t sector_number, uint8_t *buffer, uint8_t *next_dir_name, uint32_t next_index)
{
    uint32_t limit = 512;
    uint32_t index = 0;

    if (!block_read_handler(sector_number, 1, (uint8_t *)&buffer[0]))
    {
        printf("FAT: ROOT DIR sector :%d. \n", sector_number);
        return 0;
    }

    uint8_t next_file_name[256] = {'\0'};
    uint8_t next_length = get_next_dir_name(&next_dir_name[next_index], &next_file_name[0]);
    if (next_length == 0)
    {
        printf("Length is zero. \n");
        return 0;
    }
    while (index < limit)
    {
        struct dir_sfn_entry *dir_entry = (struct dir_sfn_entry *)&buffer[index];
        struct dir_lfn_entry *lfn_entry = (struct dir_lfn_entry *)&buffer[index];

        if (lfn_entry->LDIR_Attr == ATTR_LONG_NAME)
        {
            uint8_t LFN_count = 0;

            uint8_t long_file_name[256] __attribute__((aligned(4))) = {'\0'};
            while (index < limit)
            {
                dir_entry = (struct dir_sfn_entry *)&buffer[index];
                lfn_entry = (struct dir_lfn_entry *)&buffer[index];

                uint8_t LFN_blockcount;
                char LFNtext[14] = {0};
                LFN_blockcount = ReadLFNEntry((struct dir_lfn_entry *)&buffer[index], LFNtext);
                uint8_t index1 = ((~0x40) & lfn_entry->LDIR_SeqNum);
                index1 = ((index1 - 1) * 13);
                CopyUnAlignedString((char *)&long_file_name[index1], (char *)&LFNtext[0], LFN_blockcount);
                LFN_count += LFN_blockcount;

                if (lfn_entry->LDIR_SeqNum == 0x1 || lfn_entry->LDIR_SeqNum == 0x41)
                {

                    if (is_same_file(&long_file_name[0], &next_file_name[0]))
                    {
                        printf("filename : %s \n", long_file_name);
                        printf("File Match Found.  \n");

                        index += sizeof(struct dir_sfn_entry);
                        if (index >= limit)
                        {
                            break;
                        }
                        dir_entry = (struct dir_sfn_entry *)&buffer[index];
                        uint32_t first_cluster = ((uint32_t)(dir_entry->first_cluster_high << 16 | dir_entry->first_cluster_low)) & 0x0fffffff;
                        if (dir_entry->file_attrib == ATTR_DIRECTORY)
                        {
                            return search_directory_contents(first_cluster, &next_dir_name[0], next_index + next_length + 1);
                        }
                        return first_cluster;
                    }
                    index += sizeof(struct dir_sfn_entry);
                    // printf("INDEX : %d \n", index);
                    if (index >= limit)
                    {
                        // printf("INDEX REACHED 512: %d \n", index);
                        break;
                    }
                    dir_entry = (struct dir_sfn_entry *)&buffer[index];
                    // printf("file attrib : %x  ", dir_entry->file_attrib);
                    uint32_t first_cluster = ((uint32_t)(dir_entry->first_cluster_high << 16 | dir_entry->first_cluster_low)) & 0x0fffffff;
                    // printf("file first cluster : %x \n", first_cluster);
                    if (dir_entry->file_attrib == ATTR_DIRECTORY)
                    {
                        first_cluster = search_directory_contents(first_cluster, &next_dir_name[0], next_index);
                        if (first_cluster != 0)
                        {
                            return first_cluster;
                        }
                    }
                    break;
                }
                index += sizeof(struct dir_sfn_entry);
            }
        }
        else if (dir_entry->short_file_name[0] == 0)
        {
            break; // No more valid entries;
        }
        index += sizeof(struct dir_sfn_entry);
    }
    return 0;
}

void print_file_cluster_contents(uint32_t directory_first_sector)
{
    uint32_t first_root_dir_sector = getFirstSectorOfCluster(directory_first_sector);
    uint32_t last_clust_root_dir_sector = first_root_dir_sector + current_sd_partition.sectorPerCluster;
    uint32_t sector_number = first_root_dir_sector;
    uint8_t buffer[512] __attribute__((aligned(4)));
    bool is_done = false;
    while (sector_number < last_clust_root_dir_sector && !is_done)
    {
        if (!block_read_handler(sector_number, 1, (uint8_t *)&buffer[0]))
        {
            printf("FAT: ROOT DIR sector :%d. \n", sector_number);
            return;
        }
        uint32_t index = 0;
        while(index < 512) {
            if(buffer[index] == '\0') {
                is_done = true;
                break;
            }
            printf("%c", buffer[index]);
            index++;
        }
        sector_number++;
    }
}

void read_file(uint8_t *absolute_file_name)
{
    // printf(" absolute_file_name: %s %s \n", absolute_file_name, file_buffer);

    uint8_t fat_sector_buffer[512] __attribute__((aligned(4)));
    uint32_t cluster_number = get_file_size(absolute_file_name);

    // Now it's time to check if we have multiple clusters for this directory using fat table.
    uint32_t fat_sector_num, fat_entry_offset;
    uint32_t old_fat_sector_num = 0;
    while (cluster_number != 0x0fffffff && cluster_number != 0x0FFFFFF7 && cluster_number != 0)
    {
        getFatEntrySectorAndOffset(cluster_number, &fat_sector_num, &fat_entry_offset);

        if (old_fat_sector_num != fat_sector_num)
        {
            // printf("Reading fat for cluster:%d fat_sector_number:%d ", cluster_number, fat_sector_num);
            if (!block_read_handler(fat_sector_num, 1, (uint8_t *)&fat_sector_buffer[0]))
            {
                printf("FAT: Could not read FAT sector :%d. \n", fat_sector_num);
                return;
            }
        }

        print_file_cluster_contents(cluster_number);

        uint32_t *fat_entry_ptr = (uint32_t *)&fat_sector_buffer[fat_entry_offset];
        cluster_number = (*fat_entry_ptr) & 0x0fffffff;
        old_fat_sector_num = fat_sector_num;
    }
}

void print_directory_contents(uint32_t directory_first_cluster)
{
    printf("--------------*****************----------------------\n");
    uint8_t fat_sector_buffer[512] __attribute__((aligned(4)));
    uint32_t cluster_number = directory_first_cluster;

    // Now it's time to check if we have multiple clusters for this directory using fat table.
    uint32_t fat_sector_num, fat_entry_offset;
    uint32_t old_fat_sector_num = 0;
    while (cluster_number != 0x0fffffff && cluster_number != 0x0FFFFFF7 && cluster_number != 0)
    {
        getFatEntrySectorAndOffset(cluster_number, &fat_sector_num, &fat_entry_offset);

        if (old_fat_sector_num != fat_sector_num)
        {
            // printf("Reading fat for cluster:%d fat_sector_number:%d ", cluster_number, fat_sector_num);
            if (!block_read_handler(fat_sector_num, 1, (uint8_t *)&fat_sector_buffer[0]))
            {
                printf("FAT: Could not read FAT sector :%d. \n", fat_sector_num);
                return;
            }
        }

        print_directory_cluster_contents(cluster_number);

        uint32_t *fat_entry_ptr = (uint32_t *)&fat_sector_buffer[fat_entry_offset];
        cluster_number = (*fat_entry_ptr) & 0x0fffffff;
        old_fat_sector_num = fat_sector_num;
    }
    printf("--------------####################----------------------\n");
}

void print_directory_cluster_contents(uint32_t directory_first_sector)
{
    uint32_t first_root_dir_sector = getFirstSectorOfCluster(directory_first_sector);
    uint32_t last_clust_root_dir_sector = first_root_dir_sector + current_sd_partition.sectorPerCluster;
    uint32_t sector_number = first_root_dir_sector;
    uint8_t buffer[512] __attribute__((aligned(4)));

    while (sector_number < last_clust_root_dir_sector)
    {
        print_entries_in_sector(sector_number, &buffer[0]);
        // printf("---Completed printing sector :%d, next sector :%d \n", sector_number, sector_number + 1);
        sector_number++;
    }
    // printf("---Completed printing all the sectors in cluster \n");
}

uint8_t ascii_for_utf_16(uint16_t utf_16_code)
{
    if (utf_16_code == 0xffff || utf_16_code == 0x0000)
    {
        return '\0';
    }

    if ((utf_16_code & 0xff80) != 0)
    {
        return '~';
        // this character is not representable in ascii;
    }
    return (uint8_t)utf_16_code & 0x00ff;
}

static uint8_t ReadLFNEntry(struct dir_lfn_entry *LFdir, char LFNtext[14])
{
    uint8_t utf1, utf2;
    uint16_t utf, j;
    bool lfn_done = false;
    uint8_t LFN_blockcount = 0;
    if (LFdir)
    { // Check the directory pointer valid
        for (j = 0; ((j < 5) && (lfn_done == false)); j++)
        { // For each of the first 5 characters
            /* Alignment problems on this 1st block .. please leave alone */
            utf1 = LFdir->LDIR_Name1[j * 2];     // Load the low byte  ***unaligned access
            utf2 = LFdir->LDIR_Name1[j * 2 + 1]; // Load the high byte
            utf = (utf2 << 8) | utf1;            // Roll and join to make word
            if (utf == 0)
                lfn_done = true; // If the character is zero we are done
            else
                LFNtext[LFN_blockcount++] = (char)ascii_for_utf_16(utf); // Otherwise narrow to ascii and place in buffer
        }
        for (j = 0; ((j < 6) && (lfn_done == false)); j++)
        {                               // For the next six characters
            utf = LFdir->LDIR_Name2[j]; // Fetch the character
            if (utf == 0)
                lfn_done = true; // If the character is zero we are done
            else
                LFNtext[LFN_blockcount++] = (char)ascii_for_utf_16(utf); // Otherwise narrow to ascii and place in buffer
        }
        for (j = 0; ((j < 2) && (lfn_done == false)); j++)
        {                               // For the last two characters
            utf = LFdir->LDIR_Name3[j]; // Fetch the character
            if (utf == 0)
                lfn_done = true; // If the character is zero we are done
            else
                LFNtext[LFN_blockcount++] = (char)ascii_for_utf_16(utf); // Otherwise narrow to ascii and place in buffer
        }
    }
    LFNtext[LFN_blockcount] = '\0';
    // printf("%s seq:> %x eSeq : %x \n", LFNtext, LFdir->LDIR_SeqNum, ((~0x40) & LFdir->LDIR_SeqNum));
    return LFN_blockcount; // Return characters in buffer 0-13 is valid range
}

bool CopyUnAlignedString(char *Dest, const char *Src, uint32_t size)
{
    uint32_t index = 0;
    while (index < size)
    {
        Dest[index] = Src[index];
        index++;
    }
    return true; // One of the pointers was invalid
}

void print_entries_in_sector(uint32_t sector_number, uint8_t *buffer)
{
    uint32_t limit = 512;
    uint32_t index = 0;

    if (!block_read_handler(sector_number, 1, (uint8_t *)&buffer[0]))
    {
        printf("FAT: ROOT DIR sector :%d. \n", sector_number);
        return;
    }

    while (index < limit)
    {
        // printf("%c ", buffer[index++]);
        // continue;

        struct dir_sfn_entry *dir_entry = (struct dir_sfn_entry *)&buffer[index];
        struct dir_lfn_entry *lfn_entry = (struct dir_lfn_entry *)&buffer[index];

        // if(dir_entry->file_attrib == ATTR_DIRECTORY) {
        //     print_directory_contents(dir_entry->first_cluster_high << 16 | dir_entry->first_cluster_low);
        // }

        if (lfn_entry->LDIR_Attr == ATTR_LONG_NAME)
        {
            uint8_t LFN_count = 0;
            //
            uint8_t long_file_name[256] __attribute__((aligned(4))) = {'\0'};
            // We loop here until we get sets of all long file name entries
            // int8_t entry_count = 1;
            while (index < limit)
            {
                dir_entry = (struct dir_sfn_entry *)&buffer[index];
                lfn_entry = (struct dir_lfn_entry *)&buffer[index];

                uint8_t LFN_blockcount;
                char LFNtext[14] = {0};
                LFN_blockcount = ReadLFNEntry((struct dir_lfn_entry *)&buffer[index], LFNtext);
                uint8_t index1 = ((~0x40) & lfn_entry->LDIR_SeqNum);
                index1 = ((index1 - 1) * 13);
                // printf("Copy at: %d \n ", index1);
                CopyUnAlignedString((char *)&long_file_name[index1], (char *)&LFNtext[0], LFN_blockcount);
                // printf("Copy at: %s \n ", long_file_name);
                // for (int j = 0; j < LFN_blockcount; j++) {
                //     long_file_name[255 - LFN_count - LFN_blockcount + j] = LFNtext[j];
                // }
                LFN_count += LFN_blockcount;

                if (lfn_entry->LDIR_SeqNum == 0x1 || lfn_entry->LDIR_SeqNum == 0x41)
                {
                    printf("filename : %s \n", long_file_name);
                    index += sizeof(struct dir_sfn_entry);
                    // printf("INDEX : %d \n", index);
                    if (index >= limit)
                    {
                        // printf("INDEX REACHED 512: %d \n", index);
                        break;
                    }
                    dir_entry = (struct dir_sfn_entry *)&buffer[index];
                    // printf("file attrib : %x  ", dir_entry->file_attrib);
                    uint32_t first_cluster = ((uint32_t)(dir_entry->first_cluster_high << 16 | dir_entry->first_cluster_low)) & 0x0fffffff;
                    // printf("file first cluster : %x \n", first_cluster);
                    if (dir_entry->file_attrib == ATTR_DIRECTORY)
                    {
                        print_directory_contents(first_cluster);
                    }
                    break;
                }
                index += sizeof(struct dir_sfn_entry);
                // printf("INDEX : %d \n", index);
            }
            // printf("\n %s \n", long_file_name);
        }
        else if (dir_entry->short_file_name[0] == 0)
        {
            break; // No more valid entries;
        }

        index += sizeof(struct dir_sfn_entry);
    }
}

void getFatEntrySectorAndOffset(uint32_t cluster_number, uint32_t *fat_sector_num, uint32_t *fat_entry_offset)
{
    uint32_t fat_offset = cluster_number * 4; //assuming its fat32;
    if (selected_fat_type == FAT16)
    {
        fat_offset = cluster_number * 2;
    }

    *fat_sector_num = current_sd_partition.unusedSectors + current_sd_partition.reservedSectorCount + (fat_offset / current_sd_partition.bytesPerSector);
    *fat_entry_offset = (fat_offset % current_sd_partition.bytesPerSector);
}

uint32_t getFirstSectorOfCluster(uint32_t cluster_number)
{
    uint32_t firstSector = (cluster_number - 2) * current_sd_partition.sectorPerCluster;
    firstSector = firstSector + current_sd_partition.firstDataSector;
    return firstSector;
}
