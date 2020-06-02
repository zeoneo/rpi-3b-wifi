#ifndef __SD_HOST_H_
#define __SD_HOST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#ifndef BIT
#define BIT(nr) (1 << (nr))
#endif

/**
 * Start all the declarations here.
 *
 * */

typedef enum {
    SD_OK            = 0,  // NO error
    SD_ERROR         = 1,  // General non specific SD error
    SD_TIMEOUT       = 2,  // SD Timeout error
    SD_BUSY          = 3,  // SD Card is busy
    SD_NO_RESP       = 5,  // SD Card did not respond
    SD_ERROR_RESET   = 6,  // SD Card did not reset
    SD_ERROR_CLOCK   = 7,  // SD Card clock change failed
    SD_ERROR_VOLTAGE = 8,  // SD Card does not support requested voltage
    SD_ERROR_APP_CMD = 9,  // SD Card app command failed
    SD_CARD_ABSENT   = 10, // SD Card not present
    SD_READ_ERROR    = 11,
    SD_MOUNT_FAIL    = 12,
} SDRESULT;

// SD card types
typedef enum {
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_MMC     = 1,
    SD_TYPE_1       = 2,
    SD_TYPE_2_SC    = 3,
    SD_TYPE_2_HC    = 4,
} SDCARD_TYPE;

struct __attribute__((__packed__, aligned(4))) CSD {
    union {
        struct __attribute__((__packed__, aligned(1))) {
            enum {
                CSD_VERSION_1 = 0,  // enum CSD version 1.0 - 1.1, Version 2.00/Standard Capacity
                CSD_VERSION_2 = 1,  // enum CSD cersion 2.0, Version 2.00/High Capacity and Extended Capacity
            } csd_structure : 2;    // @127-126	CSD Structure Version as on SD CSD bits
            unsigned spec_vers : 6; // @125-120 CSD version as on SD CSD bits
            uint8_t taac;           // @119-112	taac as on SD CSD bits
            uint8_t nsac;           // @111-104	nsac as on SD CSD bits
            uint8_t tran_speed;     // @103-96	trans_speed as on SD CSD bits
        };
        uint32_t Raw32_0; // @127-96	Union to access 32 bits as a uint32_t
    };
    union {
        struct __attribute__((__packed__, aligned(1))) {
            unsigned ccc : 12;               // @95-84	ccc as on SD CSD bits
            unsigned read_bl_len : 4;        // @83-80	read_bl_len on SD CSD bits
            unsigned read_bl_partial : 1;    // @79		read_bl_partial as on SD CSD bits
            unsigned write_blk_misalign : 1; // @78		write_blk_misalign as on SD CSD bits
            unsigned read_blk_misalign : 1;  // @77		read_blk_misalign as on SD CSD bits
            unsigned dsr_imp : 1;            // @76		dsr_imp as on SD CSD bits
            unsigned c_size : 12;            // @75-64   Version 1 C_Size as on SD CSD bits
        };
        uint32_t Raw32_1; // @0-31	Union to access 32 bits as a uint32_t
    };
    union {
        struct __attribute__((__packed__, aligned(1))) {
            union {
                struct __attribute__((__packed__, aligned(1))) {
                    unsigned vdd_r_curr_min : 3; // @61-59	vdd_r_curr_min as on SD CSD bits
                    unsigned vdd_r_curr_max : 3; // @58-56	vdd_r_curr_max as on SD CSD bits
                    unsigned vdd_w_curr_min : 3; // @55-53	vdd_w_curr_min as on SD CSD bits
                    unsigned vdd_w_curr_max : 3; // @52-50	vdd_w_curr_max as on SD CSD bits
                    unsigned c_size_mult : 3;    // @49-47	c_size_mult as on SD CSD bits
                    unsigned reserved0 : 7;      // reserved for CSD ver 2.0 size match
                };
                unsigned ver2_c_size : 22; // Version 2 C_Size
            };
            unsigned erase_blk_en : 1; // @46		erase_blk_en as on SD CSD bits
            unsigned sector_size : 7;  // @45-39	sector_size as on SD CSD bits
            unsigned reserved1 : 2;    // 2 Spares bit unused
        };
        uint32_t Raw32_2; // @0-31	Union to access 32 bits as a uint32_t
    };
    union {
        struct __attribute__((__packed__, aligned(1))) {
            unsigned wp_grp_size : 7;        // @38-32	wp_grp_size as on SD CSD bits
            unsigned wp_grp_enable : 1;      // @31		wp_grp_enable as on SD CSD bits
            unsigned reserved2 : 2;          // @30-29 	Write as zero read as don't care
            unsigned r2w_factor : 3;         // @28-26	r2w_factor as on SD CSD bits
            unsigned write_bl_len : 4;       // @25-22	write_bl_len as on SD CSD bits
            unsigned write_bl_partial : 1;   // @21		write_bl_partial as on SD CSD bits
            unsigned default_ecc : 5;        // @20-16	default_ecc as on SD CSD bits
            unsigned file_format_grp : 1;    // @15		file_format_grp as on SD CSD bits
            unsigned copy : 1;               // @14		copy as on SD CSD bits
            unsigned perm_write_protect : 1; // @13		perm_write_protect as on SD CSD bits
            unsigned tmp_write_protect : 1;  // @12		tmp_write_protect as on SD CSD bits
            enum {
                FAT_PARTITION_TABLE    = 0, // enum SD card is FAT with partition table
                FAT_NO_PARTITION_TABLE = 1, // enum SD card is FAT with no partition table
                FS_UNIVERSAL           = 2, // enum	SD card file system is universal
                FS_OTHER               = 3, // enum	SD card file system is other
            } file_format : 2;              // @11-10	File format as on SD CSD bits
            unsigned ecc : 2;               // @9-8		ecc as on SD CSD bits
            unsigned reserved3 : 1;         // 1 spare bit unused
        };
        uint32_t Raw32_3; // @0-31	Union to access 32 bits as a uint32_t
    };
};

/* SD/MMC version bits; 8 flags, 8 major, 8 minor, 8 change */
#define SD_VERSION_SD   (1U << 31)
#define MMC_VERSION_MMC (1U << 30)

#define MAKE_SDMMC_VERSION(a, b, c) ((((u32)(a)) << 16) | ((u32)(b) << 8) | (u32)(c))
#define MAKE_SD_VERSION(a, b, c)    (SD_VERSION_SD | MAKE_SDMMC_VERSION(a, b, c))
#define MAKE_MMC_VERSION(a, b, c)   (MMC_VERSION_MMC | MAKE_SDMMC_VERSION(a, b, c))

#define EXTRACT_SDMMC_MAJOR_VERSION(x)  (((u32)(x) >> 16) & 0xff)
#define EXTRACT_SDMMC_MINOR_VERSION(x)  (((u32)(x) >> 8) & 0xff)
#define EXTRACT_SDMMC_CHANGE_VERSION(x) ((u32)(x) &0xff)

#define SD_VERSION_3    MAKE_SD_VERSION(3, 0, 0)
#define SD_VERSION_2    MAKE_SD_VERSION(2, 0, 0)
#define SD_VERSION_1_0  MAKE_SD_VERSION(1, 0, 0)
#define SD_VERSION_1_10 MAKE_SD_VERSION(1, 10, 0)

#define MMC_VERSION_UNKNOWN MAKE_MMC_VERSION(0, 0, 0)
#define MMC_VERSION_1_2     MAKE_MMC_VERSION(1, 2, 0)
#define MMC_VERSION_1_4     MAKE_MMC_VERSION(1, 4, 0)
#define MMC_VERSION_2_2     MAKE_MMC_VERSION(2, 2, 0)
#define MMC_VERSION_3       MAKE_MMC_VERSION(3, 0, 0)
#define MMC_VERSION_4       MAKE_MMC_VERSION(4, 0, 0)
#define MMC_VERSION_4_1     MAKE_MMC_VERSION(4, 1, 0)
#define MMC_VERSION_4_2     MAKE_MMC_VERSION(4, 2, 0)
#define MMC_VERSION_4_3     MAKE_MMC_VERSION(4, 3, 0)
#define MMC_VERSION_4_4     MAKE_MMC_VERSION(4, 4, 0)
#define MMC_VERSION_4_41    MAKE_MMC_VERSION(4, 4, 1)
#define MMC_VERSION_4_5     MAKE_MMC_VERSION(4, 5, 0)
#define MMC_VERSION_5_0     MAKE_MMC_VERSION(5, 0, 0)
#define MMC_VERSION_5_1     MAKE_MMC_VERSION(5, 1, 0)

#define MMC_CAP(mode)      (1 << mode)
#define MMC_MODE_HS        (MMC_CAP(MMC_HS) | MMC_CAP(SD_HS))
#define MMC_MODE_HS_52MHz  MMC_CAP(MMC_HS_52)
#define MMC_MODE_DDR_52MHz MMC_CAP(MMC_DDR_52)
#define MMC_MODE_HS200     MMC_CAP(MMC_HS_200)
#define MMC_MODE_HS400     MMC_CAP(MMC_HS_400)
#define MMC_MODE_HS400_ES  MMC_CAP(MMC_HS_400_ES)

#define MMC_CAP_NONREMOVABLE   BIT(14)
#define MMC_CAP_NEEDS_POLL     BIT(15)
#define MMC_CAP_CD_ACTIVE_HIGH BIT(16)

#define MMC_MODE_8BIT BIT(30)
#define MMC_MODE_4BIT BIT(29)
#define MMC_MODE_1BIT BIT(28)
#define MMC_MODE_SPI  BIT(27)

#define SD_DATA_4BIT 0x00040000

#define IS_SD(x)  ((x)->version & SD_VERSION_SD)
#define IS_MMC(x) ((x)->version & MMC_VERSION_MMC)

#define MMC_DATA_READ  1
#define MMC_DATA_WRITE 2

#define MMC_CMD_GO_IDLE_STATE           0
#define MMC_CMD_SEND_OP_COND            1
#define MMC_CMD_ALL_SEND_CID            2
#define MMC_CMD_SET_RELATIVE_ADDR       3
#define MMC_CMD_SET_DSR                 4
#define MMC_CMD_SWITCH                  6
#define MMC_CMD_SELECT_CARD             7
#define MMC_CMD_SEND_EXT_CSD            8
#define MMC_CMD_SEND_CSD                9
#define MMC_CMD_SEND_CID                10
#define MMC_CMD_STOP_TRANSMISSION       12
#define MMC_CMD_SEND_STATUS             13
#define MMC_CMD_SET_BLOCKLEN            16
#define MMC_CMD_READ_SINGLE_BLOCK       17
#define MMC_CMD_READ_MULTIPLE_BLOCK     18
#define MMC_CMD_SEND_TUNING_BLOCK       19
#define MMC_CMD_SEND_TUNING_BLOCK_HS200 21
#define MMC_CMD_SET_BLOCK_COUNT         23
#define MMC_CMD_WRITE_SINGLE_BLOCK      24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK    25
#define MMC_CMD_ERASE_GROUP_START       35
#define MMC_CMD_ERASE_GROUP_END         36
#define MMC_CMD_ERASE                   38
#define MMC_CMD_APP_CMD                 55
#define MMC_CMD_SPI_READ_OCR            58
#define MMC_CMD_SPI_CRC_ON_OFF          59
#define MMC_CMD_RES_MAN                 62

#define MMC_CMD62_ARG1 0xefac62ec
#define MMC_CMD62_ARG2 0xcbaea7

#define SD_CMD_SEND_RELATIVE_ADDR 3
#define SD_CMD_SWITCH_FUNC        6
#define SD_CMD_SEND_IF_COND       8
#define SD_CMD_SWITCH_UHS18V      11

#define SD_CMD_APP_SET_BUS_WIDTH  6
#define SD_CMD_APP_SD_STATUS      13
#define SD_CMD_ERASE_WR_BLK_START 32
#define SD_CMD_ERASE_WR_BLK_END   33
#define SD_CMD_APP_SEND_OP_COND   41
#define SD_CMD_APP_SEND_SCR       51

static inline bool mmc_is_tuning_cmd(uint32_t cmdidx) {
    if ((cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) || (cmdidx == MMC_CMD_SEND_TUNING_BLOCK))
        return true;
    return false;
}

/* SCR definitions in different words */
#define SD_HIGHSPEED_BUSY      0x00020000
#define SD_HIGHSPEED_SUPPORTED 0x00020000

#define UHS_SDR12_BUS_SPEED  0
#define HIGH_SPEED_BUS_SPEED 1
#define UHS_SDR25_BUS_SPEED  1
#define UHS_SDR50_BUS_SPEED  2
#define UHS_SDR104_BUS_SPEED 3
#define UHS_DDR50_BUS_SPEED  4

#define SD_MODE_UHS_SDR12  BIT(UHS_SDR12_BUS_SPEED)
#define SD_MODE_UHS_SDR25  BIT(UHS_SDR25_BUS_SPEED)
#define SD_MODE_UHS_SDR50  BIT(UHS_SDR50_BUS_SPEED)
#define SD_MODE_UHS_SDR104 BIT(UHS_SDR104_BUS_SPEED)
#define SD_MODE_UHS_DDR50  BIT(UHS_DDR50_BUS_SPEED)

#define OCR_BUSY         0x80000000
#define OCR_HCS          0x40000000
#define OCR_S18R         0x1000000
#define OCR_VOLTAGE_MASK 0x007FFF80
#define OCR_ACCESS_MODE  0x60000000

#define MMC_ERASE_ARG        0x00000000
#define MMC_SECURE_ERASE_ARG 0x80000000
#define MMC_TRIM_ARG         0x00000001
#define MMC_DISCARD_ARG      0x00000003
#define MMC_SECURE_TRIM1_ARG 0x80000001
#define MMC_SECURE_TRIM2_ARG 0x80008000

#define MMC_STATUS_MASK         (~0x0206BF7F)
#define MMC_STATUS_SWITCH_ERROR (1 << 7)
#define MMC_STATUS_RDY_FOR_DATA (1 << 8)
#define MMC_STATUS_CURR_STATE   (0xf << 9)
#define MMC_STATUS_ERROR        (1 << 19)

#define MMC_STATE_PRG (7 << 9)

#define MMC_VDD_165_195 0x00000080 /* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21   0x00000100 /* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22   0x00000200 /* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23   0x00000400 /* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24   0x00000800 /* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25   0x00001000 /* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26   0x00002000 /* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27   0x00004000 /* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28   0x00008000 /* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29   0x00010000 /* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30   0x00020000 /* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31   0x00040000 /* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32   0x00080000 /* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33   0x00100000 /* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34   0x00200000 /* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35   0x00400000 /* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36   0x00800000 /* VDD voltage 3.5 ~ 3.6 */

#define MMC_SWITCH_MODE_CMD_SET 0x00 /* Change the command set */
#define MMC_SWITCH_MODE_SET_BITS                                                                                       \
    0x01 /* Set bits in EXT_CSD byte                                                                                   \
            addressed by index which are                                                                               \
            1 in value field */
#define MMC_SWITCH_MODE_CLEAR_BITS                                                                                     \
    0x02                                /* Clear bits in EXT_CSD byte                                                  \
                                           addressed by index, which are                                               \
                                           1 in value field */
#define MMC_SWITCH_MODE_WRITE_BYTE 0x03 /* Set target byte to value */

#define SD_SWITCH_CHECK  0
#define SD_SWITCH_SWITCH 1

/*
 * EXT_CSD fields
 */
#define EXT_CSD_ENH_START_ADDR       136 /* R/W */
#define EXT_CSD_ENH_SIZE_MULT        140 /* R/W */
#define EXT_CSD_GP_SIZE_MULT         143 /* R/W */
#define EXT_CSD_PARTITION_SETTING    155 /* R/W */
#define EXT_CSD_PARTITIONS_ATTRIBUTE 156 /* R/W */
#define EXT_CSD_MAX_ENH_SIZE_MULT    157 /* R */
#define EXT_CSD_PARTITIONING_SUPPORT 160 /* RO */
#define EXT_CSD_RST_N_FUNCTION       162 /* R/W */
#define EXT_CSD_BKOPS_EN             163 /* R/W & R/W/E */
#define EXT_CSD_WR_REL_PARAM         166 /* R */
#define EXT_CSD_WR_REL_SET           167 /* R/W */
#define EXT_CSD_RPMB_MULT            168 /* RO */
#define EXT_CSD_ERASE_GROUP_DEF      175 /* R/W */
#define EXT_CSD_BOOT_BUS_WIDTH       177
#define EXT_CSD_PART_CONF            179 /* R/W */
#define EXT_CSD_BUS_WIDTH            183 /* R/W */
#define EXT_CSD_STROBE_SUPPORT       184 /* R/W */
#define EXT_CSD_HS_TIMING            185 /* R/W */
#define EXT_CSD_REV                  192 /* RO */
#define EXT_CSD_CARD_TYPE            196 /* RO */
#define EXT_CSD_PART_SWITCH_TIME     199 /* RO */
#define EXT_CSD_SEC_CNT              212 /* RO, 4 bytes */
#define EXT_CSD_HC_WP_GRP_SIZE       221 /* RO */
#define EXT_CSD_HC_ERASE_GRP_SIZE    224 /* RO */
#define EXT_CSD_BOOT_MULT            226 /* RO */
#define EXT_CSD_GENERIC_CMD6_TIME    248 /* RO */
#define EXT_CSD_BKOPS_SUPPORT        502 /* RO */

/*
 * EXT_CSD field definitions
 */

#define EXT_CSD_CMD_SET_NORMAL   (1 << 0)
#define EXT_CSD_CMD_SET_SECURE   (1 << 1)
#define EXT_CSD_CMD_SET_CPSECURE (1 << 2)

#define EXT_CSD_CARD_TYPE_26       (1 << 0) /* Card can run at 26MHz */
#define EXT_CSD_CARD_TYPE_52       (1 << 1) /* Card can run at 52MHz */
#define EXT_CSD_CARD_TYPE_DDR_1_8V (1 << 2)
#define EXT_CSD_CARD_TYPE_DDR_1_2V (1 << 3)
#define EXT_CSD_CARD_TYPE_DDR_52   (EXT_CSD_CARD_TYPE_DDR_1_8V | EXT_CSD_CARD_TYPE_DDR_1_2V)

#define EXT_CSD_CARD_TYPE_HS200_1_8V BIT(4) /* Card can run at 200MHz */
                                            /* SDR mode @1.8V I/O */
#define EXT_CSD_CARD_TYPE_HS200_1_2V BIT(5) /* Card can run at 200MHz */
                                            /* SDR mode @1.2V I/O */
#define EXT_CSD_CARD_TYPE_HS200      (EXT_CSD_CARD_TYPE_HS200_1_8V | EXT_CSD_CARD_TYPE_HS200_1_2V)
#define EXT_CSD_CARD_TYPE_HS400_1_8V BIT(6)
#define EXT_CSD_CARD_TYPE_HS400_1_2V BIT(7)
#define EXT_CSD_CARD_TYPE_HS400      (EXT_CSD_CARD_TYPE_HS400_1_8V | EXT_CSD_CARD_TYPE_HS400_1_2V)

#define EXT_CSD_BUS_WIDTH_1      0      /* Card is in 1 bit mode */
#define EXT_CSD_BUS_WIDTH_4      1      /* Card is in 4 bit mode */
#define EXT_CSD_BUS_WIDTH_8      2      /* Card is in 8 bit mode */
#define EXT_CSD_DDR_BUS_WIDTH_4  5      /* Card is in 4 bit DDR mode */
#define EXT_CSD_DDR_BUS_WIDTH_8  6      /* Card is in 8 bit DDR mode */
#define EXT_CSD_DDR_FLAG         BIT(2) /* Flag for DDR mode */
#define EXT_CSD_BUS_WIDTH_STROBE BIT(7) /* Enhanced strobe mode */

#define EXT_CSD_TIMING_LEGACY 0 /* no high speed */
#define EXT_CSD_TIMING_HS     1 /* HS */
#define EXT_CSD_TIMING_HS200  2 /* HS200 */
#define EXT_CSD_TIMING_HS400  3 /* HS400 */
#define EXT_CSD_DRV_STR_SHIFT 4 /* Driver Strength shift */

#define EXT_CSD_BOOT_ACK_ENABLE          (1 << 6)
#define EXT_CSD_BOOT_PARTITION_ENABLE    (1 << 3)
#define EXT_CSD_PARTITION_ACCESS_ENABLE  (1 << 0)
#define EXT_CSD_PARTITION_ACCESS_DISABLE (0 << 0)

#define EXT_CSD_BOOT_ACK(x)         (x << 6)
#define EXT_CSD_BOOT_PART_NUM(x)    (x << 3)
#define EXT_CSD_PARTITION_ACCESS(x) (x << 0)

#define EXT_CSD_EXTRACT_BOOT_ACK(x)         (((x) >> 6) & 0x1)
#define EXT_CSD_EXTRACT_BOOT_PART(x)        (((x) >> 3) & 0x7)
#define EXT_CSD_EXTRACT_PARTITION_ACCESS(x) ((x) &0x7)

#define EXT_CSD_BOOT_BUS_WIDTH_MODE(x)  (x << 3)
#define EXT_CSD_BOOT_BUS_WIDTH_RESET(x) (x << 2)
#define EXT_CSD_BOOT_BUS_WIDTH_WIDTH(x) (x)

#define EXT_CSD_PARTITION_SETTING_COMPLETED (1 << 0)

#define EXT_CSD_ENH_USR   (1 << 0)         /* user data area is enhanced */
#define EXT_CSD_ENH_GP(x) (1 << ((x) + 1)) /* GP part (x+1) is enhanced */

#define EXT_CSD_HS_CTRL_REL (1 << 0) /* host controlled WR_REL_SET */

#define EXT_CSD_WR_DATA_REL_USR   (1 << 0)         /* user data area WR_REL */
#define EXT_CSD_WR_DATA_REL_GP(x) (1 << ((x) + 1)) /* GP part (x+1) WR_REL */

#define R1_ILLEGAL_COMMAND (1 << 22)
#define R1_APP_CMD         (1 << 5)

#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136     (1 << 1) /* 136 bit response */
#define MMC_RSP_CRC     (1 << 2) /* expect valid crc */
#define MMC_RSP_BUSY    (1 << 3) /* card may send busy */
#define MMC_RSP_OPCODE  (1 << 4) /* response contains opcode */

#define MMC_RSP_NONE (0)
#define MMC_RSP_R1   (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R1b  (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)
#define MMC_RSP_R2   (MMC_RSP_PRESENT | MMC_RSP_136 | MMC_RSP_CRC)
#define MMC_RSP_R3   (MMC_RSP_PRESENT)
#define MMC_RSP_R4   (MMC_RSP_PRESENT)
#define MMC_RSP_R5   (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R6   (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R7   (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)

#define MMCPART_NOAVAILABLE (0xff)
#define PART_ACCESS_MASK    (0x7)
#define PART_SUPPORT        (0x1)
#define ENHNCD_SUPPORT      (0x2)
#define PART_ENH_ATTRIB     (0x1f)

#define MMC_QUIRK_RETRY_SEND_CID     BIT(0)
#define MMC_QUIRK_RETRY_SET_BLOCKLEN BIT(1)

enum mmc_voltage {
    MMC_SIGNAL_VOLTAGE_000 = 0,
    MMC_SIGNAL_VOLTAGE_120 = 1,
    MMC_SIGNAL_VOLTAGE_180 = 2,
    MMC_SIGNAL_VOLTAGE_330 = 4,
};

#define MMC_ALL_SIGNAL_VOLTAGE (MMC_SIGNAL_VOLTAGE_120 | MMC_SIGNAL_VOLTAGE_180 | MMC_SIGNAL_VOLTAGE_330)

/* Maximum block size for MMC */
#define MMC_MAX_BLOCK_LEN 512

/* The number of MMC physical partitions.  These consist of:
 * boot partitions (2), general purpose partitions (4) in MMC v4.4.
 */
#define MMC_NUM_BOOT_PARTITION 2
#define MMC_PART_RPMB          3 /* RPMB partition number */

struct __attribute__((__packed__, aligned(4))) mmc_config {
    const uint8_t* name;
    uint32_t host_caps;
    uint32_t voltages;
    uint32_t f_min;
    uint32_t f_max;
    uint32_t b_max;
    uint8_t part_type;
};

struct __attribute__((__packed__, aligned(4))) mmc_cmd {
    uint16_t cmdidx;
    uint32_t resp_type;
    uint32_t cmdarg;
    uint32_t response[4];
};

struct __attribute__((__packed__, aligned(4))) mmc_data {
    union {
        uint8_t* dest;
        const uint8_t* src; /* src buffers don't get written to */
    };
    uint32_t flags;
    uint32_t blocks;
    uint32_t blocksize;
};

enum bus_mode {
    MMC_LEGACY,
    SD_LEGACY,
    MMC_HS,
    SD_HS,
    MMC_HS_52,
    MMC_DDR_52,
    UHS_SDR12,
    UHS_SDR25,
    UHS_SDR50,
    UHS_DDR50,
    UHS_SDR104,
    MMC_HS_200,
    MMC_HS_400,
    MMC_HS_400_ES,
    MMC_MODES_END
};

struct __attribute__((__packed__, aligned(4))) mmc {
    const struct mmc_config* cfg; /* provided configuration */
    uint32_t version;
    uint32_t has_init;
    int high_capacity;
    bool clk_disable; /* true if the clock can be turned off */
    uint32_t bus_width;
    uint32_t clock;
    enum mmc_voltage signal_voltage;
    uint32_t card_caps;
    uint32_t host_caps;
    uint32_t ocr;
    uint32_t dsr;
    uint32_t dsr_imp;
    uint32_t scr[2];
    uint32_t csd[4];
    uint32_t cid[4];
    uint16_t rca;
    uint8_t part_support;
    uint8_t part_attr;
    uint8_t wr_rel_set;
    uint8_t part_config;
    uint8_t gen_cmd6_time;
    uint8_t part_switch_time;
    uint32_t tran_speed;
    uint32_t legacy_speed; /* speed for the legacy mode provided by the card */
    uint32_t read_bl_len;
    uint64_t capacity;
    uint64_t capacity_user;
    uint64_t capacity_boot;
    uint64_t capacity_rpmb;
    uint64_t capacity_gp[4];
    char op_cond_pending;  /* 1 if we are waiting on an op_cond command */
    char init_in_progress; /* 1 if we have done mmc_start_init() */
    char preinit;          /* start init as early as possible */
    int ddr_mode;
    uint8_t* ext_csd;
    uint32_t cardtype; /* cardtype read from the MMC */
    enum mmc_voltage current_voltage;
    enum bus_mode selected_mode; /* mode currently used */
    enum bus_mode best_mode;     /* best mode is the supported mode with the
                                  * highest bandwidth. It may not always be the
                                  * operating mode due to limitations when
                                  * accessing the boot partitions
                                  */
    uint32_t quirks;
};

struct __attribute__((__packed__, aligned(4))) sdhost_state {
    uint32_t clock;
    uint32_t hcfg;
    uint32_t cdiv;
    uint32_t blocks;
    uint32_t max_clk;
    uint32_t ns_per_fifo_word;
    uint8_t use_busy;
    struct mmc_config* cfg;
    struct mmc_cmd* cmd;
    struct mmc_data* data;
    struct mmc* mmc;
};

struct sdhost_state* sdhostInit();

int bcm2835_send_cmd(struct sdhost_state* host, struct mmc_cmd* cmd, struct mmc_data* data);
int bcm2835_set_ios(struct sdhost_state* host);

// bool sd (uint32_t startBlock, uint32_t numBlocks, uint8_t* buffer);

// bool sdcard_write (uint32_t startBlock, uint32_t numBlocks, uint8_t* buffer);

// SDRESULT sdClearBlocks (uint32_t startBlock, uint32_t numBlocks);

/**
 * End of all the declarations here.
 * */

#ifdef __cplusplus
}
#endif

#endif