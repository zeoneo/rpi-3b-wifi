#include <device/gpio.h>
#include <device/emmc-sdio.h>
#include <device/emmc.h>
#include <device/dma.h>
#include <plibc/stdio.h>
#include <kernel/rpi-base.h>
#include <kernel/rpi-interrupts.h>
#include <kernel/systimer.h>

#define DEBUG_INFO 1
#define NULL ((void *)0)
#if DEBUG_INFO == 1
#define LOG_DEBUG(...) printf(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

/*--------------------------------------------------------------------------}
{      EMMC BLKSIZECNT register - BCM2835.PDF Manual Section 5 page 68      }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regBLKSIZECNT
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned BLKSIZE : 10; // @0-9		Block size in bytes
			unsigned reserved : 6;			// @10-15	Write as zero read as don't care
			volatile unsigned BLKCNT : 16;  // @16-31	Number of blocks to be transferred
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{      EMMC CMDTM register - BCM2835.PDF Manual Section 5 pages 69-70       }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regCMDTM
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			unsigned reserved : 1;				// @0		Write as zero read as don't care
			volatile unsigned TM_BLKCNT_EN : 1; // @1		Enable the block counter for multiple block transfers
			volatile enum { TM_NO_COMMAND = 0,  //			no command
							TM_CMD12 = 1,		//			command CMD12
							TM_CMD23 = 2,		//			command CMD23
							TM_RESERVED = 3,
			} TM_AUTO_CMD_EN : 2;					// @2-3		Select the command to be send after completion of a data transfer
			volatile unsigned TM_DAT_DIR : 1;		// @4		Direction of data transfer (0 = host to card , 1 = card to host )
			volatile unsigned TM_MULTI_BLOCK : 1;   // @5		Type of data transfer (0 = single block, 1 = muli block)
			unsigned reserved1 : 10;				// @6-15	Write as zero read as don't care
			volatile enum { CMD_NO_RESP = 0,		//			no response
							CMD_136BIT_RESP = 1,	//			136 bits response
							CMD_48BIT_RESP = 2,		//			48 bits response
							CMD_BUSY48BIT_RESP = 3, //			48 bits response using busy
			} CMD_RSPNS_TYPE : 2;					// @16-17
			unsigned reserved2 : 1;					// @18		Write as zero read as don't care
			volatile unsigned CMD_CRCCHK_EN : 1;	// @19		Check the responses CRC (0=disabled, 1= enabled)
			volatile unsigned CMD_IXCHK_EN : 1;		// @20		Check that response has same index as command (0=disabled, 1= enabled)
			volatile unsigned CMD_ISDATA : 1;		// @21		Command involves data transfer (0=disabled, 1= enabled)
			volatile enum { CMD_TYPE_NORMAL = 0,	//			normal command
							CMD_TYPE_SUSPEND = 1,   //			suspend command
							CMD_TYPE_RESUME = 2,	//			resume command
							CMD_TYPE_ABORT = 3,		//			abort command
			} CMD_TYPE : 2;							// @22-23
			volatile unsigned CMD_INDEX : 6;		// @24-29
			unsigned reserved3 : 2;					// @30-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{        EMMC STATUS register - BCM2835.PDF Manual Section 5 page 72        }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regSTATUS
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned CMD_INHIBIT : 1;	// @0		Command line still used by previous command
			volatile unsigned DAT_INHIBIT : 1;	// @1		Data lines still used by previous data transfer
			volatile unsigned DAT_ACTIVE : 1;	 // @2		At least one data line is active
			unsigned reserved : 5;				  // @3-7		Write as zero read as don't care
			volatile unsigned WRITE_TRANSFER : 1; // @8		New data can be written to EMMC
			volatile unsigned READ_TRANSFER : 1;  // @9		New data can be read from EMMC
			unsigned reserved1 : 10;			  // @10-19	Write as zero read as don't care
			volatile unsigned DAT_LEVEL0 : 4;	 // @20-23	Value of data lines DAT3 to DAT0
			volatile unsigned CMD_LEVEL : 1;	  // @24		Value of command line CMD
			volatile unsigned DAT_LEVEL1 : 4;	 // @25-28	Value of data lines DAT7 to DAT4
			unsigned reserved2 : 3;				  // @29-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{      EMMC CONTROL0 register - BCM2835.PDF Manual Section 5 page 73/74     }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regCONTROL0
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			unsigned reserved : 1;			   // @0		Write as zero read as don't care
			volatile unsigned HCTL_DWIDTH : 1; // @1		Use 4 data lines (true = enable)
			volatile unsigned HCTL_HS_EN : 1;  // @2		Select high speed mode (true = enable)
			unsigned reserved1 : 2;			   // @3-4		Write as zero read as don't care
			volatile unsigned HCTL_8BIT : 1;   // @5		Use 8 data lines (true = enable)
			unsigned reserved2 : 10;		   // @6-15	Write as zero read as don't care
			volatile unsigned GAP_STOP : 1;	// @16		Stop the current transaction at the next block gap
			volatile unsigned GAP_RESTART : 1; // @17		Restart a transaction last stopped using the GAP_STOP
			volatile unsigned READWAIT_EN : 1; // @18		Use DAT2 read-wait protocol for cards supporting this
			volatile unsigned GAP_IEN : 1;	 // @19		Enable SDIO interrupt at block gap
			volatile unsigned SPI_MODE : 1;	// @20		SPI mode enable
			volatile unsigned BOOT_EN : 1;	 // @21		Boot mode access
			volatile unsigned ALT_BOOT_EN : 1; // @22		Enable alternate boot mode access
			unsigned reserved3 : 9;			   // @23-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{      EMMC CONTROL1 register - BCM2835.PDF Manual Section 5 page 74/75     }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regCONTROL1
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned CLK_INTLEN : 1;		// @0		Clock enable for internal EMMC clocks for power saving
			volatile const unsigned CLK_STABLE : 1; // @1		SD clock stable  0=No 1=yes   **read only
			volatile unsigned CLK_EN : 1;			// @2		SD clock enable  0=disable 1=enable
			unsigned reserved : 2;					// @3-4		Write as zero read as don't care
			volatile unsigned CLK_GENSEL : 1;		// @5		Mode of clock generation (0=Divided, 1=Programmable)
			volatile unsigned CLK_FREQ_MS2 : 2;		// @6-7		SD clock base divider MSBs (Version3+ only)
			volatile unsigned CLK_FREQ8 : 8;		// @8-15	SD clock base divider LSBs
			volatile unsigned DATA_TOUNIT : 4;		// @16-19	Data timeout unit exponent
			unsigned reserved1 : 4;					// @20-23	Write as zero read as don't care
			volatile unsigned SRST_HC : 1;			// @24		Reset the complete host circuit
			volatile unsigned SRST_CMD : 1;			// @25		Reset the command handling circuit
			volatile unsigned SRST_DATA : 1;		// @26		Reset the data handling circuit
			unsigned reserved2 : 5;					// @27-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{    EMMC CONTROL2 register - BCM2835.PDF Manual Section 5 pages 81-84      }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regCONTROL2
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile const unsigned ACNOX_ERR : 1;  // @0		Auto command not executed due to an error **read only
			volatile const unsigned ACTO_ERR : 1;   // @1		Timeout occurred during auto command execution **read only
			volatile const unsigned ACCRC_ERR : 1;  // @2		Command CRC error occurred during auto command execution **read only
			volatile const unsigned ACEND_ERR : 1;  // @3		End bit is not 1 during auto command execution **read only
			volatile const unsigned ACBAD_ERR : 1;  // @4		Command index error occurred during auto command execution **read only
			unsigned reserved : 2;					// @5-6		Write as zero read as don't care
			volatile const unsigned NOTC12_ERR : 1; // @7		Error occurred during auto command CMD12 execution **read only
			unsigned reserved1 : 8;					// @8-15	Write as zero read as don't care
			volatile enum { SDR12 = 0,
							SDR25 = 1,
							SDR50 = 2,
							SDR104 = 3,
							DDR50 = 4,
			} UHSMODE : 3;				  // @16-18	Select the speed mode of the SD card (SDR12, SDR25 etc)
			unsigned reserved2 : 3;		  // @19-21	Write as zero read as don't care
			volatile unsigned TUNEON : 1; // @22		Start tuning the SD clock
			volatile unsigned TUNED : 1;  // @23		Tuned clock is used for sampling data
			unsigned reserved3 : 8;		  // @24-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{     EMMC INTERRUPT register - BCM2835.PDF Manual Section 5 pages 75-77    }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regINTERRUPT
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned CMD_DONE : 1;  // @0		Command has finished
			volatile unsigned DATA_DONE : 1; // @1		Data transfer has finished
			volatile unsigned BLOCK_GAP : 1; // @2		Data transfer has stopped at block gap
			unsigned reserved : 1;			 // @3		Write as zero read as don't care
			volatile unsigned WRITE_RDY : 1; // @4		Data can be written to DATA register
			volatile unsigned READ_RDY : 1;  // @5		DATA register contains data to be read
			unsigned reserved1 : 2;			 // @6-7		Write as zero read as don't care
			volatile unsigned CARD : 1;		 // @8		Card made interrupt request
			unsigned reserved2 : 3;			 // @9-11	Write as zero read as don't care
			volatile unsigned RETUNE : 1;	// @12		Clock retune request was made
			volatile unsigned BOOTACK : 1;   // @13		Boot acknowledge has been received
			volatile unsigned ENDBOOT : 1;   // @14		Boot operation has terminated
			volatile unsigned ERR : 1;		 // @15		An error has occured
			volatile unsigned CTO_ERR : 1;   // @16		Timeout on command line
			volatile unsigned CCRC_ERR : 1;  // @17		Command CRC error
			volatile unsigned CEND_ERR : 1;  // @18		End bit on command line not 1
			volatile unsigned CBAD_ERR : 1;  // @19		Incorrect command index in response
			volatile unsigned DTO_ERR : 1;   // @20		Timeout on data line
			volatile unsigned DCRC_ERR : 1;  // @21		Data CRC error
			volatile unsigned DEND_ERR : 1;  // @22		End bit on data line not 1
			unsigned reserved3 : 1;			 // @23		Write as zero read as don't care
			volatile unsigned ACMD_ERR : 1;  // @24		Auto command error
			unsigned reserved4 : 7;			 // @25-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{     EMMC IRPT_MASK register - BCM2835.PDF Manual Section 5 pages 77-79    }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regIRPT_MASK
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned CMD_DONE : 1;  // @0		Command has finished
			volatile unsigned DATA_DONE : 1; // @1		Data transfer has finished
			volatile unsigned BLOCK_GAP : 1; // @2		Data transfer has stopped at block gap
			unsigned reserved : 1;			 // @3		Write as zero read as don't care
			volatile unsigned WRITE_RDY : 1; // @4		Data can be written to DATA register
			volatile unsigned READ_RDY : 1;  // @5		DATA register contains data to be read
			unsigned reserved1 : 2;			 // @6-7		Write as zero read as don't care
			volatile unsigned CARD : 1;		 // @8		Card made interrupt request
			unsigned reserved2 : 3;			 // @9-11	Write as zero read as don't care
			volatile unsigned RETUNE : 1;	// @12		Clock retune request was made
			volatile unsigned BOOTACK : 1;   // @13		Boot acknowledge has been received
			volatile unsigned ENDBOOT : 1;   // @14		Boot operation has terminated
			volatile unsigned ERR : 1;		 // @15		An error has occured
			volatile unsigned CTO_ERR : 1;   // @16		Timeout on command line
			volatile unsigned CCRC_ERR : 1;  // @17		Command CRC error
			volatile unsigned CEND_ERR : 1;  // @18		End bit on command line not 1
			volatile unsigned CBAD_ERR : 1;  // @19		Incorrect command index in response
			volatile unsigned DTO_ERR : 1;   // @20		Timeout on data line
			volatile unsigned DCRC_ERR : 1;  // @21		Data CRC error
			volatile unsigned DEND_ERR : 1;  // @22		End bit on data line not 1
			unsigned reserved3 : 1;			 // @23		Write as zero read as don't care
			volatile unsigned ACMD_ERR : 1;  // @24		Auto command error
			unsigned reserved4 : 7;			 // @25-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{      EMMC IRPT_EN register - BCM2835.PDF Manual Section 5 pages 79-71     }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regIRPT_EN
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned CMD_DONE : 1;  // @0		Command has finished
			volatile unsigned DATA_DONE : 1; // @1		Data transfer has finished
			volatile unsigned BLOCK_GAP : 1; // @2		Data transfer has stopped at block gap
			unsigned reserved : 1;			 // @3		Write as zero read as don't care
			volatile unsigned WRITE_RDY : 1; // @4		Data can be written to DATA register
			volatile unsigned READ_RDY : 1;  // @5		DATA register contains data to be read
			unsigned reserved1 : 2;			 // @6-7		Write as zero read as don't care
			volatile unsigned CARD : 1;		 // @8		Card made interrupt request
			unsigned reserved2 : 3;			 // @9-11	Write as zero read as don't care
			volatile unsigned RETUNE : 1;	// @12		Clock retune request was made
			volatile unsigned BOOTACK : 1;   // @13		Boot acknowledge has been received
			volatile unsigned ENDBOOT : 1;   // @14		Boot operation has terminated
			volatile unsigned ERR : 1;		 // @15		An error has occured
			volatile unsigned CTO_ERR : 1;   // @16		Timeout on command line
			volatile unsigned CCRC_ERR : 1;  // @17		Command CRC error
			volatile unsigned CEND_ERR : 1;  // @18		End bit on command line not 1
			volatile unsigned CBAD_ERR : 1;  // @19		Incorrect command index in response
			volatile unsigned DTO_ERR : 1;   // @20		Timeout on data line
			volatile unsigned DCRC_ERR : 1;  // @21		Data CRC error
			volatile unsigned DEND_ERR : 1;  // @22		End bit on data line not 1
			unsigned reserved3 : 1;			 // @23		Write as zero read as don't care
			volatile unsigned ACMD_ERR : 1;  // @24		Auto command error
			unsigned reserved4 : 7;			 // @25-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{       EMMC TUNE_STEP  register - BCM2835.PDF Manual Section 5 page 86     }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regTUNE_STEP
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile enum { TUNE_DELAY_200ps = 0,
							TUNE_DELAY_400ps = 1,
							TUNE_DELAY_400psA = 2, // I dont understand the duplicate value???
							TUNE_DELAY_600ps = 3,
							TUNE_DELAY_700ps = 4,
							TUNE_DELAY_900ps = 5,
							TUNE_DELAY_900psA = 6, // I dont understand the duplicate value??
							TUNE_DELAY_1100ps = 7,
			} DELAY : 3;			// @0-2		Select the speed mode of the SD card (SDR12, SDR25 etc)
			unsigned reserved : 29; // @3-31	Write as zero read as don't care
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{    EMMC SLOTISR_VER register - BCM2835.PDF Manual Section 5 pages 87-88   }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regSLOTISR_VER
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned SLOT_STATUS : 8; // @0-7		Logical OR of interrupt and wakeup signal for each slot
			unsigned reserved : 8;			   // @8-15	Write as zero read as don't care
			volatile unsigned SDVERSION : 8;   // @16-23	Host Controller specification version
			volatile unsigned VENDOR : 8;	  // @24-31	Vendor Version Number
		};
		volatile uint32_t Raw32; // @0-31	Union to access all 32 bits as a uint32_t
	};
};

/***************************************************************************}
{         PRIVATE POINTERS TO ALL THE BCM2835 EMMC HOST REGISTERS           }
****************************************************************************/
#define EMMC_ARG2 ((volatile __attribute__((aligned(4))) uint32_t *)(uintptr_t)(PERIPHERAL_BASE + 0x300000))
#define EMMC_BLKSIZECNT ((volatile struct __attribute__((aligned(4))) regBLKSIZECNT *)(uintptr_t)(PERIPHERAL_BASE + 0x300004))
#define EMMC_ARG1 ((volatile __attribute__((aligned(4))) uint32_t *)(uintptr_t)(PERIPHERAL_BASE + 0x300008))
#define EMMC_CMDTM ((volatile struct __attribute__((aligned(4))) regCMDTM *)(uintptr_t)(PERIPHERAL_BASE + 0x30000c))
#define EMMC_RESP0 ((volatile __attribute__((aligned(4))) uint32_t *)(uintptr_t)(PERIPHERAL_BASE + 0x300010))
#define EMMC_RESP1 ((volatile __attribute__((aligned(4))) uint32_t *)(uintptr_t)(PERIPHERAL_BASE + 0x300014))
#define EMMC_RESP2 ((volatile __attribute__((aligned(4))) uint32_t *)(uintptr_t)(PERIPHERAL_BASE + 0x300018))
#define EMMC_RESP3 ((volatile __attribute__((aligned(4))) uint32_t *)(uintptr_t)(PERIPHERAL_BASE + 0x30001C))
#define EMMC_DATA ((volatile __attribute__((aligned(4))) uint32_t *)(uintptr_t)(PERIPHERAL_BASE + 0x300020))
#define EMMC_STATUS ((volatile struct __attribute__((aligned(4))) regSTATUS *)(uintptr_t)(PERIPHERAL_BASE + 0x300024))
#define EMMC_CONTROL0 ((volatile struct __attribute__((aligned(4))) regCONTROL0 *)(uintptr_t)(PERIPHERAL_BASE + 0x300028))
#define EMMC_CONTROL1 ((volatile struct __attribute__((aligned(4))) regCONTROL1 *)(uintptr_t)(PERIPHERAL_BASE + 0x30002C))
#define EMMC_INTERRUPT ((volatile struct __attribute__((aligned(4))) regINTERRUPT *)(uintptr_t)(PERIPHERAL_BASE + 0x300030))
#define EMMC_IRPT_MASK ((volatile struct __attribute__((aligned(4))) regIRPT_MASK *)(uintptr_t)(PERIPHERAL_BASE + 0x300034))
#define EMMC_IRPT_EN ((volatile struct __attribute__((aligned(4))) regIRPT_EN *)(uintptr_t)(PERIPHERAL_BASE + 0x300038))
#define EMMC_CONTROL2 ((volatile struct __attribute__((aligned(4))) regCONTROL2 *)(uintptr_t)(PERIPHERAL_BASE + 0x30003C))
#define EMMC_TUNE_STEP ((volatile struct __attribute__((aligned(4))) regTUNE_STEP *)(uintptr_t)(PERIPHERAL_BASE + 0x300088))
#define EMMC_SLOTISR_VER ((volatile struct __attribute__((aligned(4))) regSLOTISR_VER *)(uintptr_t)(PERIPHERAL_BASE + 0x3000fC))

/*--------------------------------------------------------------------------}
{						   SD CARD OCR register							    }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regOCR
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			unsigned reserved : 15;			 // @0-14	Write as zero read as don't care
			unsigned voltage2v7to2v8 : 1;	// @15		Voltage window 2.7v to 2.8v
			unsigned voltage2v8to2v9 : 1;	// @16		Voltage window 2.8v to 2.9v
			unsigned voltage2v9to3v0 : 1;	// @17		Voltage window 2.9v to 3.0v
			unsigned voltage3v0to3v1 : 1;	// @18		Voltage window 3.0v to 3.1v
			unsigned voltage3v1to3v2 : 1;	// @19		Voltage window 3.1v to 3.2v
			unsigned voltage3v2to3v3 : 1;	// @20		Voltage window 3.2v to 3.3v
			unsigned voltage3v3to3v4 : 1;	// @21		Voltage window 3.3v to 3.4v
			unsigned voltage3v4to3v5 : 1;	// @22		Voltage window 3.4v to 3.5v
			unsigned voltage3v5to3v6 : 1;	// @23		Voltage window 3.5v to 3.6v
			unsigned reserved1 : 6;			 // @24-29	Write as zero read as don't care
			unsigned card_capacity : 1;		 // @30		Card Capacity status
			unsigned card_power_up_busy : 1; // @31		Card power up status (busy)
		};
		volatile uint32_t Raw32; // @0-31	Union to access 32 bits as a uint32_t
	};
};

/*--------------------------------------------------------------------------}
{						   SD CARD SCR register							    }
{--------------------------------------------------------------------------*/
struct __attribute__((__packed__, aligned(4))) regSCR
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile enum { SD_SPEC_1_101 = 0,		// ..enum..	Version 1.0-1.01
							SD_SPEC_11 = 1,			// ..enum..	Version 1.10
							SD_SPEC_2_3 = 2,		// ..enum..	Version 2.00 or Version 3.00 (check bit SD_SPEC3)
			} SD_SPEC : 4;							// @0-3		SD Memory Card Physical Layer Specification version
			volatile enum { SCR_VER_1 = 0,			// ..enum..	SCR version 1.0
			} SCR_STRUCT : 4;						// @4-7		SCR structure version
			volatile enum { BUS_WIDTH_1 = 1,		// ..enum..	Card supports bus width 1
							BUS_WIDTH_4 = 4,		// ..enum.. Card supports bus width 4
			} BUS_WIDTH : 4;						// @8-11	SD Bus width
			volatile enum { SD_SEC_NONE = 0,		// ..enum..	No Security
							SD_SEC_NOT_USED = 1,	// ..enum..	Security Not Used
							SD_SEC_101 = 2,			// ..enum..	SDSC Card (Security Version 1.01)
							SD_SEC_2 = 3,			// ..enum..	SDHC Card (Security Version 2.00)
							SD_SEC_3 = 4,			// ..enum..	SDXC Card (Security Version 3.xx)
			} SD_SECURITY : 3;						// @12-14	Card security in use
			volatile unsigned DATA_AFTER_ERASE : 1; // @15		Defines the data status after erase, whether it is 0 or 1
			unsigned reserved : 3;					// @16-18	Write as zero read as don't care
			volatile enum { EX_SEC_NONE = 0,		// ..enum..	No extended Security
			} EX_SECURITY : 4;						// @19-22	Extended security
			volatile unsigned SD_SPEC3 : 1;			// @23		Spec. Version 3.00 or higher
			volatile enum { CMD_SUPP_SPEED_CLASS = 1,
							CMD_SUPP_SET_BLKCNT = 2,
			} CMD_SUPPORT : 2;		// @24-25	CMD support
			unsigned reserved1 : 6; // @26-63	Write as zero read as don't care
		};
		volatile uint32_t Raw32_Lo; // @0-31	Union to access low 32 bits as a uint32_t
	};
	volatile uint32_t Raw32_Hi; // @32-63	Access upper 32 bits as a uint32_t
};

/*--------------------------------------------------------------------------}
{						  PI SD CARD CID register						    }
{--------------------------------------------------------------------------*/
/* The CID is Big Endian and secondly the Pi butchers it by not having CRC */
/*  So the CID appears shifted 8 bits right with first 8 bits reading zero */
struct __attribute__((__packed__, aligned(4))) regCID
{
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile uint8_t OID_Lo;
			volatile uint8_t OID_Hi; // @0-15	Identifies the card OEM. The OID is assigned by the SD-3C, LLC
			volatile uint8_t MID;	// @16-23	Manufacturer ID, assigned by the SD-3C, LLC
			unsigned reserved : 8;   // @24-31	PI butcher with CRC removed these bits end up empty
		};
		volatile uint32_t Raw32_0; // @0-31	Union to access 32 bits as a uint32_t
	};
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile char ProdName4 : 8; // @0-7		Product name character four
			volatile char ProdName3 : 8; // @8-15	Product name character three
			volatile char ProdName2 : 8; // @16-23	Product name character two
			volatile char ProdName1 : 8; // @24-31	Product name character one
		};
		volatile uint32_t Raw32_1; // @0-31	Union to access 32 bits as a uint32_t
	};
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned SerialNumHi : 16; // @0-15	Serial number upper 16 bits
			volatile unsigned ProdRevLo : 4;	// @16-19	Product revision low value in BCD
			volatile unsigned ProdRevHi : 4;	// @20-23	Product revision high value in BCD
			volatile char ProdName5 : 8;		// @24-31	Product name character five
		};
		volatile uint32_t Raw32_2; // @0-31	Union to access 32 bits as a uint32_t
	};
	union {
		struct __attribute__((__packed__, aligned(1)))
		{
			volatile unsigned ManufactureMonth : 4; // @0-3		Manufacturing date month (1=Jan, 2=Feb, 3=Mar etc)
			volatile unsigned ManufactureYear : 8;  // @4-11	Manufacturing dateyear (offset from 2000 .. 1=2001,2=2002,3=2003 etc)
			unsigned reserved1 : 4;					// @12-15 	Write as zero read as don't care
			volatile unsigned SerialNumLo : 16;		// @16-23	Serial number lower 16 bits
		};
		volatile uint32_t Raw32_3; // @0-31	Union to access 32 bits as a uint32_t
	};
};

#include <assert.h> // Need for compile time static_assert

/* Check the main register section group sizes */
static_assert(sizeof(struct regBLKSIZECNT) == 0x04, "EMMC register BLKSIZECNT should be 0x04 bytes in size");
static_assert(sizeof(struct regCMDTM) == 0x04, "EMMC register CMDTM should be 0x04 bytes in size");
static_assert(sizeof(struct regSTATUS) == 0x04, "EMMC register STATUS should be 0x04 bytes in size");
static_assert(sizeof(struct regCONTROL0) == 0x04, "EMMC register CONTROL0 should be 0x04 bytes in size");
static_assert(sizeof(struct regCONTROL1) == 0x04, "EMMC register CONTROL1 should be 0x04 bytes in size");
static_assert(sizeof(struct regCONTROL2) == 0x04, "EMMC register CONTROL2 should be 0x04 bytes in size");
static_assert(sizeof(struct regINTERRUPT) == 0x04, "EMMC register INTERRUPT should be 0x04 bytes in size");
static_assert(sizeof(struct regIRPT_MASK) == 0x04, "EMMC register IRPT_MASK should be 0x04 bytes in size");
static_assert(sizeof(struct regIRPT_EN) == 0x04, "EMMC register IRPT_EN should be 0x04 bytes in size");
static_assert(sizeof(struct regTUNE_STEP) == 0x04, "EMMC register TUNE_STEP should be 0x04 bytes in size");
static_assert(sizeof(struct regSLOTISR_VER) == 0x04, "EMMC register SLOTISR_VER should be 0x04 bytes in size");

static_assert(sizeof(struct regOCR) == 0x04, "EMMC register OCR should be 0x04 bytes in size");
static_assert(sizeof(struct regSCR) == 0x08, "EMMC register SCR should be 0x08 bytes in size");
static_assert(sizeof(struct regCID) == 0x10, "EMMC register CID should be 0x10 bytes in size");

/*--------------------------------------------------------------------------}
{			  INTERRUPT REGISTER TURN TO MASK BIT DEFINITIONS			    }
{--------------------------------------------------------------------------*/
#define INT_AUTO_ERROR 0x01000000   // ACMD_ERR bit in register
#define INT_DATA_END_ERR 0x00400000 // DEND_ERR bit in register
#define INT_DATA_CRC_ERR 0x00200000 // DCRC_ERR bit in register
#define INT_DATA_TIMEOUT 0x00100000 // DTO_ERR bit in register
#define INT_INDEX_ERROR 0x00080000  // CBAD_ERR bit in register
#define INT_END_ERROR 0x00040000	// CEND_ERR bit in register
#define INT_CRC_ERROR 0x00020000	// CCRC_ERR bit in register
#define INT_CMD_TIMEOUT 0x00010000  // CTO_ERR bit in register
#define INT_ERR 0x00008000			// ERR bit in register
#define INT_ENDBOOT 0x00004000		// ENDBOOT bit in register
#define INT_BOOTACK 0x00002000		// BOOTACK bit in register
#define INT_RETUNE 0x00001000		// RETUNE bit in register
#define INT_CARD 0x00000100			// CARD bit in register
#define INT_READ_RDY 0x00000020		// READ_RDY bit in register
#define INT_WRITE_RDY 0x00000010	// WRITE_RDY bit in register
#define INT_BLOCK_GAP 0x00000004	// BLOCK_GAP bit in register
#define INT_DATA_DONE 0x00000002	// DATA_DONE bit in register
#define INT_CMD_DONE 0x00000001		// CMD_DONE bit in register
#define INT_ERROR_MASK (INT_CRC_ERROR | INT_END_ERROR | INT_INDEX_ERROR |        \
						INT_DATA_TIMEOUT | INT_DATA_CRC_ERR | INT_DATA_END_ERR | \
						INT_ERR | INT_AUTO_ERROR)
#define INT_ALL_MASK (INT_CMD_DONE | INT_DATA_DONE | INT_READ_RDY | INT_WRITE_RDY | INT_ERROR_MASK)

/*--------------------------------------------------------------------------}
{						  SD CARD FREQUENCIES							    }
{--------------------------------------------------------------------------*/
#define FREQ_SETUP 400000	// 400 Khz
#define FREQ_NORMAL 25000000 // 25 Mhz
#define FREQ_HIGH 50000000 // 50 Mhz

/*--------------------------------------------------------------------------}
{						  CMD 41 BIT SELECTIONS							    }
{--------------------------------------------------------------------------*/
#define ACMD41_HCS 0x40000000
#define ACMD41_SDXC_POWER 0x10000000
#define ACMD41_S18R 0x04000000
#define ACMD41_VOLTAGE 0x00ff8000
/* PI DOES NOT SUPPORT VOLTAGE SWITCH */
#define ACMD41_ARG_HC (ACMD41_HCS | ACMD41_SDXC_POWER | ACMD41_VOLTAGE) //(ACMD41_HCS|ACMD41_SDXC_POWER|ACMD41_VOLTAGE|ACMD41_S18R)
#define ACMD41_ARG_SC (ACMD41_VOLTAGE)									//(ACMD41_VOLTAGE|ACMD41_S18R)

/*--------------------------------------------------------------------------}
{						   SD CARD COMMAND RECORD						    }
{--------------------------------------------------------------------------*/
typedef struct EMMCCommand
{
	const char cmd_name[16];
	struct regCMDTM code;
	struct __attribute__((__packed__))
	{
		unsigned use_rca : 1;   // @0		Command uses rca
		unsigned reserved : 15; // @1-15	Write as zero read as don't care
		uint16_t delay;			// @16-31	Delay to apply after command
	};
} EMMCCommand;



/*--------------------------------------------------------------------------}
{							  SD CARD COMMAND TABLE						    }
{--------------------------------------------------------------------------*/
static EMMCCommand sdCommandTable[IX_IO_RW_DIRECT_EXTENDED + 1] = {
	[IX_GO_IDLE_STATE] = {"GO_IDLE_STATE", .code.CMD_INDEX = 0, .code.CMD_RSPNS_TYPE = CMD_NO_RESP, .use_rca = 0, .delay = 0},
	[IX_IO_SEND_OP_COND] = {"IO_SEND_OP_COND", .code.CMD_INDEX = 5, .code.CMD_RSPNS_TYPE = CMD_48BIT_RESP, .use_rca = 0, .delay = 0},
	[IX_SEND_RELATIVE_ADDR] = {"SEND_REL_ADDR", .code.CMD_INDEX = 3, .code.CMD_RSPNS_TYPE = CMD_48BIT_RESP, .use_rca = 0, .delay = 0},
	[IX_SELECT_CARD] = {"SELECT_CARD", .code.CMD_INDEX = 7, .code.CMD_RSPNS_TYPE = CMD_48BIT_RESP, .use_rca = 0, .delay = 0},
	[IX_IO_RW_DIRECT] = {"IO_RW_DIRECT", .code.CMD_INDEX = 52, .code.CMD_IXCHK_EN = 1, .code.CMD_CRCCHK_EN = 1, .code.CMD_RSPNS_TYPE = CMD_48BIT_RESP, .use_rca = 0, .delay = 0},
	[IX_IO_RW_DIRECT_EXTENDED] = {"IO_RW_DIRECT", .code.CMD_INDEX = 53, .code.CMD_IXCHK_EN = 1, .code.CMD_ISDATA = 1, .code.CMD_CRCCHK_EN = 1, .code.CMD_RSPNS_TYPE = CMD_48BIT_RESP, .use_rca = 0, .delay = 0},
};

// static const char *SD_TYPE_NAME[] = {"Unknown", "MMC", "Type 1", "Type 2 SC", "Type 2 HC"};

/*--------------------------------------------------------------------------}
{						  SD CARD DESCRIPTION RECORD					    }
{--------------------------------------------------------------------------*/
typedef struct SDDescriptor
{
	struct regCID cid;	 // Card cid
	struct CSD csd;		   // Card csd
	struct regSCR scr;	 // Card scr
	uint64_t CardCapacity; // Card capacity expanded .. calculated from card details
	SDCARD_TYPE type;	  // Card type
	uint32_t rca;		   // Card rca
	struct regOCR ocr;	 // Card ocr
	uint32_t status;	   // Card last status

	EMMCCommand *lastCmd;

	struct
	{
		uint32_t rootCluster;		  // Active partition rootCluster
		uint32_t sectorPerCluster;	// Active partition sectors per cluster
		uint32_t bytesPerSector;	  // Active partition bytes per sector
		uint32_t firstDataSector;	 // Active partition first data sector
		uint32_t dataSectors;		  // Active partition data sectors
		uint32_t unusedSectors;		  // Active partition unused sectors
		uint32_t reservedSectorCount; // Active partition reserved sectors
	} partition;

	uint8_t partitionLabe1[11]; // Partition label
} SDDescriptor;

/*--------------------------------------------------------------------------}
{					    CURRENT SD CARD DATA STORAGE					    }
{--------------------------------------------------------------------------*/
static SDDescriptor sdCard = {0};

static int sdSendCommandP(EMMCCommand *cmd, uint32_t arg, uint32_t * resp);
static SDRESULT sdSetClock(uint32_t freq);

static uint_fast8_t fls_uint32_t(uint32_t x)
{
	uint_fast8_t r = 32; // Start at 32
	if (!x)
		return 0; // If x is zero answer must be zero
	if (!(x & 0xffff0000u))
	{			  // If none of the upper word bits are set
		x <<= 16; // We can roll it up 16 bits
		r -= 16;  // Reduce r by 16
	}
	if (!(x & 0xff000000u))
	{			 // If none of uppermost byte bits are set
		x <<= 8; // We can roll it up by 8 bits
		r -= 8;  // Reduce r by 8
	}
	if (!(x & 0xf0000000u))
	{			 // If none of the uppermost 4 bits are set
		x <<= 4; // We can roll it up by 4 bits
		r -= 4;  // Reduce r by 4
	}
	if (!(x & 0xc0000000u))
	{			 // If none of the uppermost 2 bits are set
		x <<= 2; // We can roll it up by 2 bits
		r -= 2;  // Reduce r by 2
	}
	if (!(x & 0x80000000u))
	{			 // If the uppermost bit is not set
		x <<= 1; // We can roll it up by 1 bit
		r -= 1;  // Reduce r by 1
	}
	return r; // Return the number of the uppermost set bit
}

static uint32_t sdGetClockDivider(uint32_t freq)
{
	uint32_t divisor = (41666667 + freq - 1) / freq; // Pi SD frequency is always 41.66667Mhz on baremetal
	if (divisor > 0x3FF)
		divisor = 0x3FF; // Constrain divisor to max 0x3FF
	if (EMMC_SLOTISR_VER->SDVERSION < 2)
	{													 // Any version less than HOST SPECIFICATION 3 (Aka numeric 2)
		uint_fast8_t shiftcount = fls_uint32_t(divisor); // Only 8 bits and set pwr2 div on Hosts specs 1 & 2
		if (shiftcount > 0)
			shiftcount--; // Note the offset of shift by 1 (look at the spec)
		if (shiftcount > 7)
			shiftcount = 7;					   // It's only 8 bits maximum on HOST_SPEC_V2
		divisor = ((uint32_t)1 << shiftcount); // Version 1,2 take power 2
	}
	else if (divisor < 3)
		divisor = 4; // Set minimum divisor limit
	LOG_DEBUG("Divisor = %d, Freq Set = %d\n", (int)divisor, (int)(41666667 / divisor));
	return divisor; // Return divisor that would be required
}

static SDRESULT sdWaitForInterrupt(uint32_t mask)
{
	uint64_t td = 0;						// Zero time difference
	uint64_t start_time = 0;				// Zero start time
	uint32_t tMask = mask | INT_ERROR_MASK; // Add fatal error masks to mask provided
	while (!(EMMC_INTERRUPT->Raw32 & tMask) && (td < 1000000))
	{
		if (!start_time)
			start_time = timer_getTickCount64(); // If start time not set the set start time
		else
			td = tick_difference(start_time, timer_getTickCount64()); // Time difference between start time and now
	}
	uint32_t ival = EMMC_INTERRUPT->Raw32; // Fetch all the interrupt flags
	if (td >= 1000000 ||				   // No reponse timeout occurred
		(ival & INT_CMD_TIMEOUT) ||		   // Command timeout occurred
		(ival & INT_DATA_TIMEOUT))		   // Data timeout occurred
	{
		printf("EMMC: Wait for interrupt %08x EMMC_STATUS: %08x int_val:%08x resp0: %08x\n", (unsigned int)mask, (unsigned int)EMMC_STATUS->Raw32, 
		  (unsigned int)ival, (unsigned int)*EMMC_RESP0); // Log any error if requested

		// Clear the interrupt register completely.
		EMMC_INTERRUPT->Raw32 = ival; // Clear any interrupt that occured

		return SD_TIMEOUT; // Return SD_TIMEOUT
	}
	else if (ival & INT_ERROR_MASK)
	{
		printf("EMMC: Error waiting for interrupt: %08x %08x %08x\n",
			   (unsigned int)EMMC_STATUS->Raw32, (unsigned int)ival,
			   (unsigned int)*EMMC_RESP0); // Log any error if requested

		// Clear the interrupt register completely.
		EMMC_INTERRUPT->Raw32 = ival; // Clear any interrupt that occured

		return SD_ERROR; // Return SD_ERROR
	}

	// Clear the interrupt we were waiting for, leaving any other (non-error) interrupts.
	EMMC_INTERRUPT->Raw32 = mask; // Clear any interrupt we are waiting on

	return SD_OK; // Return SD_OK
}

static SDRESULT sdWaitForCommand(void)
{
	uint64_t td = 0;									// Zero time difference
	uint64_t start_time = 0;							// Zero start time
	while ((EMMC_STATUS->CMD_INHIBIT) &&				// Command inhibit signal
		   !(EMMC_INTERRUPT->Raw32 & INT_ERROR_MASK) && // No error occurred
		   (td < 1000000))								// Timeout not reached
	{
		if (!start_time)
			start_time = timer_getTickCount64(); // Get start time
		else
			td = tick_difference(start_time, timer_getTickCount64()); // Time difference between start and now
	}
	if ((td >= 1000000) || (EMMC_INTERRUPT->Raw32 & INT_ERROR_MASK)) // Error occurred or it timed out
	{
		printf("EMMC: Wait for command aborted: %08x %08x %08x\n",
			   (unsigned int)EMMC_STATUS->Raw32, (unsigned int)EMMC_INTERRUPT->Raw32,
			   (unsigned int)*EMMC_RESP0); // Log any error if requested
		return SD_BUSY;					   // return SD_BUSY
	}

	return SD_OK; // return SD_OK
}

#define R1_ERRORS_MASK 0xfff9c004
static int sdSendCommandP(EMMCCommand *cmd, uint32_t arg, uint32_t *resp)
{
	SDRESULT res;

	/* Check for command in progress */
	if (sdWaitForCommand() != SD_OK)
		return SD_BUSY; // Check command wait

	/* Clear interrupt flags.  This is done by setting the ones that are currently set */
	EMMC_INTERRUPT->Raw32 = EMMC_INTERRUPT->Raw32; // Clear interrupts

	if(cmd->code.CMD_ISDATA) {
		cmd->code.Raw32 |= 1<<8; // This enables DMA
	}

	/*
	 * CMD6 may be Setbuswidth or Switchfunc depending on Appcmd prefix = 6
	 */
	if(cmd->code.CMD_INDEX == 6 && sdCard.lastCmd->code.CMD_INDEX == 55) {
		cmd->code.CMD_ISDATA = 1;
		cmd->code.TM_DAT_DIR = 1;
	}
	
	// Check if command is IORWextended
	if(cmd->code.CMD_INDEX == 53) {
		if(arg & (1<<31)) {
			cmd->code.TM_DAT_DIR = 0; // Host to card. This means write
		} else {
			cmd->code.TM_DAT_DIR = 1; // Card to host. This means read
		}
	}

	// Set block count flags
	// & 

	if((EMMC_BLKSIZECNT->Raw32  & 0xFFFF0000) != 0x10000) {
		cmd->code.TM_BLKCNT_EN = 1;
		cmd->code.TM_MULTI_BLOCK = 1;
	}
			// c |= Multiblock | Blkcnten;


	/* Set the argument and the command code, Some commands require a delay before reading the response */
	*EMMC_ARG1 = arg;		 // Set argument to SD card
	*EMMC_CMDTM = cmd->code ; // Send command to SD card
	if (cmd->delay) {
		MicroDelay(cmd->delay); // Wait for required delay
	}
		

	/* Wait until command complete interrupt */
	if ((res = sdWaitForInterrupt(INT_CMD_DONE)))
		return res; // In non zero return result

	/* Get response from RESP0 */
	uint32_t resp0 = *EMMC_RESP0; // Fetch SD card response 0 to command

	/* Handle response types for command */
	switch (cmd->code.CMD_RSPNS_TYPE)
	{
	// no response
	case CMD_NO_RESP:
		resp[0] = 0;
		break;

	case CMD_BUSY48BIT_RESP:
		resp[0] = *EMMC_RESP0;
		sdCard.status = resp0;
		break;
	// RESP0 contains card status, no other data from the RESP* registers.
	// Return value non-zero if any error flag in the status value.
	case CMD_48BIT_RESP:
		resp[0] = *EMMC_RESP0;
		break;
	// RESP0..3 contains 128 bit CID or CSD shifted down by 8 bits as no CRC
	// Note: highest bits are in RESP3.
	case CMD_136BIT_RESP:
		resp[0] = *EMMC_RESP0 <<8;
		resp[1] = *EMMC_RESP0 >>24 | *EMMC_RESP1 <<8;
		resp[2] = *EMMC_RESP1 >>24 | *EMMC_RESP2 <<8;
		resp[3] = *EMMC_RESP2 >>24 | *EMMC_RESP3 <<8;
		break;
	}

	if(cmd->code.CMD_RSPNS_TYPE == CMD_BUSY48BIT_RESP) {
		EMMC_IRPT_EN->Raw32 = (EMMC_IRPT_EN->Raw32 | 1<<1 | 1<<15); // DataDone | Err
		MicroDelay(3000);
		if(EMMC_INTERRUPT->DATA_DONE == 0) {
			printf("Error: no Datadone after CMD %d\n", cmd->code.Raw32);
		}
		
		if(EMMC_INTERRUPT->ERR) {
			printf("emmcio: CMD %d error interrupt %ux\n",cmd->code.Raw32,EMMC_INTERRUPT->Raw32);
		}
		EMMC_INTERRUPT->Raw32 = EMMC_INTERRUPT->Raw32;//reset interuppts;
	}

	// Handle command specific cases

	if(cmd->code.CMD_INDEX == 7){ // MMC_SELECT
		MicroDelay(10);
		sdSetClock(FREQ_NORMAL); //25 Mhz
		MicroDelay(10);
		// emmc.fastclock = 1;
	} else if(cmd->code.CMD_INDEX == 6) { // Setbuswidth
		if(sdCard.lastCmd->code.CMD_INDEX == 55){ //  last command == App command
			/*
			 * If card bus width changes, change host bus width
			 */
			switch(arg){
			case 0:
				EMMC_CONTROL0->Raw32 = (EMMC_CONTROL0->Raw32 & (~ (1<<1)));
				break;
			case 2:
				EMMC_CONTROL0->Raw32 = (EMMC_CONTROL0->Raw32 |  1<<1 );
				break;
			}
		}else{
			/*
			 * If card switched into high speed mode, increase clock speed
			 */
			if((arg & 0x8000000F) == 0x80000001){
				MicroDelay(10);
				sdSetClock(FREQ_HIGH);
				MicroDelay(10);
			}
		}
	} else if(cmd->code.CMD_INDEX == 52 && ((arg & ~0xFF) == ( 1<<31 | 0<<28 | 7<<9)) ){ // IORWdirect = 52
		switch(arg & 0x3){
			case 0:
				EMMC_CONTROL0->Raw32 = (EMMC_CONTROL0->Raw32 & (~ (1<<1)));
				break;
			case 2:
				EMMC_CONTROL0->Raw32 = (EMMC_CONTROL0->Raw32 |  1<<1 );
				break;
		}
	}

	sdCard.lastCmd = cmd;

	return SD_OK;
}


static SDRESULT sdSetClock(uint32_t freq)
{
	uint64_t td = 0;		 // Zero time difference
	uint64_t start_time = 0; // Zero start time
	LOG_DEBUG("EMMC: setting clock speed to %d.\n", (unsigned int)freq);
	while ((EMMC_STATUS->CMD_INHIBIT || EMMC_STATUS->DAT_INHIBIT) // Data inhibit signal
		   && (td < 100000))									  // Timeout not reached
	{
		if (!start_time)
			start_time = timer_getTickCount64(); // If start time not set the set start time
		else
			td = tick_difference(start_time, timer_getTickCount64()); // Time difference between start time and now
	}
	if (td >= 100000)
	{ // Timeout waiting for inghibit flags
		printf("EMMC: Set clock: timeout waiting for inhibit flags. Status %08x.\n",
			   (unsigned int)EMMC_STATUS->Raw32);
		return SD_ERROR_CLOCK; // Return clock error
	}

	/* Switch clock off */
	EMMC_CONTROL1->CLK_EN = 0; // Disable clock
	MicroDelay(10);			   // We must now wait 10 microseconds

	/* Request the divisor for new clock setting */
	uint_fast32_t cdiv = sdGetClockDivider(freq); // Fetch divisor for new frequency
	uint_fast32_t divlo = (cdiv & 0xff) << 8;	 // Create divisor low bits value
	uint_fast32_t divhi = ((cdiv & 0x300) >> 2);  // Create divisor high bits value

	/* Set new clock frequency by setting new divisor */
	EMMC_CONTROL1->Raw32 = (EMMC_CONTROL1->Raw32 & 0xffff001f) | divlo | divhi;
	MicroDelay(10); // We must now wait 10 microseconds

	/* Enable the clock. */
	EMMC_CONTROL1->CLK_EN = 1; // Enable the clock

	/* Wait for clock to be stablize */
	td = 0;								// Zero time difference
	start_time = 0;						// Zero start time
	while (!(EMMC_CONTROL1->CLK_STABLE) // Clock not stable yet
		   && (td < 100000))			// Timeout not reached
	{
		if (!start_time)
			start_time = timer_getTickCount64(); // If start time not set the set start time
		else
			td = tick_difference(start_time, timer_getTickCount64()); // Time difference between start time and now
	}
	if (td >= 100000)
	{ // Timeout waiting for stability flag
		printf("EMMC: ERROR: failed to get stable clock.\n");
		return SD_ERROR_CLOCK; // Return clock error
	}
	return SD_OK; // Clock frequency set worked
}

bool sdio_send_command(int cmd_idx, uint32_t arg, uint32_t *resp) {
	EMMCCommand *cmd = &sdCommandTable[cmd_idx];
	SDRESULT sdio_resp = sdSendCommandP(cmd, arg, resp);
	return sdio_resp == SD_OK;
}


static void mmc_interrupt_handler() {
	printf(" ");
}

static void mmc_interrupt_clearer() {
	if(EMMC_INTERRUPT->ERR) {
		printf("Interrupt handler error EMMC_INTERRUPT: 0x%x \n ", EMMC_INTERRUPT->Raw32);
		// sdDebugResponse();
	}

	EMMC_IRPT_EN->Raw32 = (EMMC_IRPT_EN->Raw32 & ~(EMMC_INTERRUPT->Raw32));
	// printf("returning from interrupt clearer. \n");
}

static SDRESULT sdResetCard(void)
{
	SDRESULT resp;
	uint64_t td = 0;		 // Zero time difference
	uint64_t start_time = 0; // Zero start time

	/* Send reset host controller and wait for complete */
	EMMC_CONTROL0->Raw32 = 0;   // Zero control0 register
	EMMC_CONTROL1->Raw32 = 0;   // Zero control1 register
	EMMC_CONTROL1->SRST_HC = 1; // Reset the complete host circuit
								//EMMC_CONTROL2->UHSMODE = SDR12;
	MicroDelay(10);				// Wait 10 microseconds
	LOG_DEBUG("EMMC: reset card.\n");
	while ((EMMC_CONTROL1->SRST_HC) // Host circuit reset not clear
		   && (td < 100000))		// Timeout not reached
	{
		if (!start_time)
			start_time = timer_getTickCount64(); // If start time not set the set start time
		else
			td = tick_difference(start_time, timer_getTickCount64()); // Time difference between start time and now
	}
	if (td >= 100000)
	{ // Timeout waiting for reset flag
		printf("EMMC: ERROR: failed to reset.\n");
		return SD_ERROR_RESET; // Return reset SD Card error
	}


	/* Enable internal clock and set data timeout */
	EMMC_CONTROL1->DATA_TOUNIT = 0xE; // Maximum timeout value
	EMMC_CONTROL1->CLK_INTLEN = 1;	// Enable internal clock
	MicroDelay(10);					  // Wait 10 microseconds


	/* Set clock to setup frequency */
	if ((resp = sdSetClock(FREQ_SETUP)))
		return resp; // Set low speed setup frequency (400Khz)

	printf(" Clock setup complete");

	/* Enable interrupts for command completion values */
	EMMC_IRPT_EN->Raw32 = 0xffffffff;

	EMMC_IRPT_MASK->Raw32 = 0xffffffff;
	EMMC_INTERRUPT->Raw32 = 0xffffffff;

	//MMC interrupt handler
	register_irq_handler(62, mmc_interrupt_handler, mmc_interrupt_clearer);

	MicroDelay(10);

	/* Reset our card structure entries */
	sdCard.rca = 0;				   // Zero rca
	sdCard.ocr.Raw32 = 0;		   // Zero ocr
	sdCard.lastCmd = 0;			   // Zero lastCmd
	sdCard.status = 0;			   // Zero status
	sdCard.type = SD_TYPE_UNKNOWN; // Set card type unknown

	return resp; // Return response
}

#define USED(x) if(x);else{}

void sdio_iosetup(bool write, void *buf, uint32_t bsize, uint32_t bcount)
{
	printf("\t Setting up io write: %d buf:0x%x bsize: %d bcount: %d \n", write, buf, bsize, bcount);
	USED(write);
	USED(buf);
	EMMC_BLKSIZECNT->Raw32 = bcount<<16 | bsize;
}

void sdio_do_io(bool write, uint8_t *buf, uint32_t len) {
	if(len & 3) {
		printf("SDIO_ERROR: ODD length is not allowed. Get lost. \n");
		return;
	}

	uint8_t *emmc_data_buf = (uint8_t *)(EMMC_DATA);
	if(write) {
		dma_start(4, 11, MEM_TO_DEV, buf, emmc_data_buf, len);
	} else {
		dma_start(4, 11, DEV_TO_MEM, emmc_data_buf, buf,len);
	}
	
	
	MicroDelay(3000);

	if(dma_wait(4) < 0) {
		printf("DMA ERROR while transferring data. \n");
		return;
	}

	// uint32_t *intbuff = (uint32_t *)buf;
	// for (uint_fast16_t i = 0; i < (len/4); i++)
	// {
	// 	if (write)
	// 		*EMMC_DATA = intbuff[i];
	// 	else
	// 		intbuff[i] = *EMMC_DATA;
	// }
	
	

	// for (uint_fast16_t i = 0; i < len; i++)
	// {
	// 	if (write)
	// 	{
	// 		uint32_t data = (buf[i]);
	// 		data |= (buf[i + 1] << 8);
	// 		data |= (buf[i + 2] << 16);
	// 		data |= (buf[i + 3] << 24);
	// 		*EMMC_DATA = data;
	// 	}
	// 	else
	// 	{
	// 		uint32_t data = *EMMC_DATA;
	// 		buf[i] = (data)&0xff;
	// 		buf[i + 1] = (data >> 8) & 0xff;
	// 		buf[i + 2] = (data >> 16) & 0xff;
	// 		buf[i + 3] = (data >> 24) & 0xff;
	// 	}
	// }

	// if(!write)
	// 	cachedinvse(buf, len);

	EMMC_IRPT_EN->Raw32 = (EMMC_IRPT_EN->Raw32 | 1<<1 | 1<<15 );

	MicroDelay(3000);

		
	if(EMMC_INTERRUPT->DATA_DONE == 0) {
		printf("SDIO_ERROR: Data not done: Status: 0x%x EMMC_INTERRUPT: 0x%x \n", EMMC_STATUS->Raw32, EMMC_INTERRUPT->Raw32);
		EMMC_INTERRUPT->Raw32 = EMMC_INTERRUPT->Raw32;
		return ;
	}

	if(EMMC_INTERRUPT->ERR) {
		printf("SDIO_ERROR: Status: 0x%x EMMC_INTERRUPT: 0x%x \n", EMMC_STATUS->Raw32, EMMC_INTERRUPT->Raw32);
		EMMC_INTERRUPT->Raw32 = EMMC_INTERRUPT->Raw32;
		return;
	}
	//Clear interrupts by setting ones to already set bits.
	EMMC_INTERRUPT->Raw32 = EMMC_INTERRUPT->Raw32;
}

bool initSDIO()
{
	SDRESULT resp;

	// FOllowing lines connect EMMC controller to SD CARD
    for ( uint32_t i = 34; i <= 39; i++)
    {
        select_alt_func(i, Alt3);
    }

	for (uint32_t i = 48; i <= 53; i++) {
        select_alt_func(i, Alt0);
    }

	// Reset the card.
	if ((resp = sdResetCard()))
		return resp; // Reset SD card


	printf(" Card reset complete \n");

	
	return resp == SD_OK;
}
