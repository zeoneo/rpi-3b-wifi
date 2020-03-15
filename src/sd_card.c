#include<device/sd_card.h>
#include<device/sdhost.h>
#include<device/sdhost1.h>
#include<plibc/stdio.h>
#include <kernel/systimer.h>
#include<limits.h>



#define MMC_RSP_BITS(resp, start, len)	__bitfield((resp), (start), (len))
static inline int
__bitfield(uint32_t *src, int start, int len) {
	uint8_t *sp;
	uint32_t dst, mask;
	int shift, bs, bc;

	if (start < 0 || len < 0 || len > 32)
		return 0;

	dst = 0;
	mask = len % 32 ? UINT_MAX >> (32 - (len % 32)) : UINT_MAX;
	shift = 0;

	while (len > 0) {
		sp = (uint8_t *)src + start / 8;
		bs = start % 8;
		bc = 8 - bs;
		if (bc > len)
			bc = len;
		dst |= (*sp >> bs) << shift;
		shift += bc;
		start += bc;
		len -= bc;
	}

	dst &= mask;
	return (int)dst;
}

#define SD_CID_PNM_CPY(resp, pnm)					\
	do {								\
		(pnm)[0] = MMC_RSP_BITS((resp), 96, 8);			\
		(pnm)[1] = MMC_RSP_BITS((resp), 88, 8);			\
		(pnm)[2] = MMC_RSP_BITS((resp), 80, 8);			\
		(pnm)[3] = MMC_RSP_BITS((resp), 72, 8);			\
		(pnm)[4] = MMC_RSP_BITS((resp), 64, 8);			\
		(pnm)[5] = '\0';					\
	} while (/*CONSTCOND*/0)


#define SD_CSD_CSDVER(resp)		MMC_RSP_BITS((resp), 126, 2)
#define  SD_CSD_CSDVER_1_0		0
#define  SD_CSD_CSDVER_2_0		1
#define SD_CSD_MMCVER(resp)		MMC_RSP_BITS((resp), 122, 4)
#define SD_CSD_TAAC(resp)		MMC_RSP_BITS((resp), 112, 8)
#define SD_CSD_TAAC_EXP(resp)		MMC_RSP_BITS((resp), 115, 4)
#define SD_CSD_TAAC_MANT(resp)		MMC_RSP_BITS((resp), 112, 3)
#define  SD_CSD_TAAC_1_5_MSEC		0x26
#define SD_CSD_NSAC(resp)		MMC_RSP_BITS((resp), 104, 8)
#define SD_CSD_SPEED(resp)		MMC_RSP_BITS((resp), 96, 8)
#define SD_CSD_SPEED_MANT(resp)		MMC_RSP_BITS((resp), 99, 4)
#define SD_CSD_SPEED_EXP(resp)		MMC_RSP_BITS((resp), 96, 3)
#define  SD_CSD_SPEED_25_MHZ		0x32
#define  SD_CSD_SPEED_50_MHZ		0x5a
#define SD_CSD_CCC(resp)		MMC_RSP_BITS((resp), 84, 12)
#define  SD_CSD_CCC_BASIC		(1 << 0)	
#define  SD_CSD_CCC_BR			(1 << 2)	
#define  SD_CSD_CCC_BW			(1 << 4)	
#define  SD_CSD_CCC_ERACE		(1 << 5)	
#define  SD_CSD_CCC_WP			(1 << 6)	
#define  SD_CSD_CCC_LC			(1 << 7)	
#define  SD_CSD_CCC_AS			(1 << 8)	
#define  SD_CSD_CCC_IOM			(1 << 9)	
#define  SD_CSD_CCC_SWITCH		(1 << 10)	
#define SD_CSD_READ_BL_LEN(resp)	MMC_RSP_BITS((resp), 80, 4)
#define SD_CSD_READ_BL_PARTIAL(resp)	MMC_RSP_BITS((resp), 79, 1)
#define SD_CSD_WRITE_BLK_MISALIGN(resp)	MMC_RSP_BITS((resp), 78, 1)
#define SD_CSD_READ_BLK_MISALIGN(resp)	MMC_RSP_BITS((resp), 77, 1)
#define SD_CSD_DSR_IMP(resp)		MMC_RSP_BITS((resp), 76, 1)
#define SD_CSD_C_SIZE(resp)		MMC_RSP_BITS((resp), 62, 12)
#define SD_CSD_CAPACITY(resp)		((SD_CSD_C_SIZE((resp))+1) << \
					 (SD_CSD_C_SIZE_MULT((resp))+2))
#define SD_CSD_VDD_R_CURR_MIN(resp)	MMC_RSP_BITS((resp), 59, 3)
#define SD_CSD_VDD_R_CURR_MAX(resp)	MMC_RSP_BITS((resp), 56, 3)
#define SD_CSD_VDD_W_CURR_MIN(resp)	MMC_RSP_BITS((resp), 53, 3)
#define SD_CSD_VDD_W_CURR_MAX(resp)	MMC_RSP_BITS((resp), 50, 3)
#define  SD_CSD_VDD_RW_CURR_100mA	0x7
#define  SD_CSD_VDD_RW_CURR_80mA	0x6
#define SD_CSD_V2_C_SIZE(resp)		MMC_RSP_BITS((resp), 48, 22)
#define SD_CSD_V2_CAPACITY(resp)	((SD_CSD_V2_C_SIZE((resp))+1) << 10)
#define SD_CSD_V2_BL_LEN		0x9 
#define SD_CSD_C_SIZE_MULT(resp)	MMC_RSP_BITS((resp), 47, 3)
#define SD_CSD_ERASE_BLK_EN(resp)	MMC_RSP_BITS((resp), 46, 1)
#define SD_CSD_SECTOR_SIZE(resp)	MMC_RSP_BITS((resp), 39, 7) 
#define SD_CSD_WP_GRP_SIZE(resp)	MMC_RSP_BITS((resp), 32, 7) 
#define SD_CSD_WP_GRP_ENABLE(resp)	MMC_RSP_BITS((resp), 31, 1)
#define SD_CSD_R2W_FACTOR(resp)		MMC_RSP_BITS((resp), 26, 3)
#define SD_CSD_WRITE_BL_LEN(resp)	MMC_RSP_BITS((resp), 22, 4)
#define  SD_CSD_RW_BL_LEN_2G		0xa
#define  SD_CSD_RW_BL_LEN_1G		0x9
#define SD_CSD_WRITE_BL_PARTIAL(resp)	MMC_RSP_BITS((resp), 21, 1)
#define SD_CSD_FILE_FORMAT_GRP(resp)	MMC_RSP_BITS((resp), 15, 1)
#define SD_CSD_COPY(resp)		MMC_RSP_BITS((resp), 14, 1)
#define SD_CSD_PERM_WRITE_PROTECT(resp)	MMC_RSP_BITS((resp), 13, 1)
#define SD_CSD_TMP_WRITE_PROTECT(resp)	MMC_RSP_BITS((resp), 12, 1)
#define SD_CSD_FILE_FORMAT(resp)	MMC_RSP_BITS((resp), 10, 2)


static struct sdhost_state * sd_ctrl = (void *)0;

static void print_csd(uint32_t *csd) {
    uint32_t block_length, capacity_bytes, clock_div;
    if (SD_CSD_CSDVER(csd) == SD_CSD_CSDVER_2_0) {
			printf("    CSD     : Ver 2.0\n");
			printf("    Capacity: %d\n", SD_CSD_V2_CAPACITY(csd));
			printf("    Size    : %d\n", SD_CSD_V2_C_SIZE(csd));

			block_length = 1 << SD_CSD_V2_BL_LEN;

			
			capacity_bytes = (SD_CSD_V2_CAPACITY(csd) * block_length);

			clock_div = 5;
		} else if (SD_CSD_CSDVER(csd) == SD_CSD_CSDVER_1_0) {
			printf("    CSD     : Ver 1.0\n");
			printf("    Capacity: %d\n", SD_CSD_CAPACITY(csd));
			printf("    Size    : %d\n", SD_CSD_C_SIZE(csd));

			block_length =  1 << SD_CSD_READ_BL_LEN(csd);

			
			capacity_bytes = (SD_CSD_CAPACITY(csd) * block_length);

			clock_div = 10;
		} else {
			printf("ERROR: Unknown CSD version 0x%x!\n", SD_CSD_CSDVER(csd));
			return;
		}

		printf(" block length: %d capacity_bytes: %lu clock_div: %d \n", block_length, capacity_bytes, clock_div);
        // printf(" block length: %d capacity_bytes: %d clock_div: %d ", block_length, capacity_bytes, clock_div);
}


bool mmc_read_blocks(uint32_t startBlock, uint32_t numBlocks, uint8_t *dest)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	if (numBlocks > 1)
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

	if (sd_ctrl->mmc->high_capacity)
		cmd.cmdarg = startBlock;
	else
		cmd.cmdarg = startBlock * sd_ctrl->mmc->read_bl_len;

	cmd.resp_type = MMC_RSP_R1;

	data.dest = dest;
	data.blocks = numBlocks;
	data.blocksize = sd_ctrl->mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	if (bcm2835_send_cmd(sd_ctrl, &cmd, &data))
		return true;

	if (numBlocks > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (bcm2835_send_cmd(sd_ctrl, &cmd, (void *)0)) {
			return true;
		}
	}
	return false;
}

static int use_sdhost() {

    struct sdhost_state * sd_host = sdhostInit();
    if(!sd_host) {
        printf("Unable initialize sd host \n");
        return -1;
    }
    
    // init_sdhost();
    MicroDelay(1000);

    // // uint32_t resp[4] = {0};

    struct mmc_cmd cmd = { 0 };
    cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;
 
    uint32_t x = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
    printf("x: %x \n", x);

     MicroDelay(1000);

    cmd.cmdidx = SD_CMD_SEND_IF_COND;
	cmd.resp_type = MMC_RSP_R7;
    cmd.cmdarg = ((sd_host->mmc->cfg->voltages & 0xff8000) != 0) << 8 | 0xaa; //0x1AA;

    bcm2835_send_cmd(sd_host, &cmd, (void *)0);
	printf("Resp type: %d respo: %x %x %x %x \n", MMC_RSP_R3, cmd.response[0], cmd.response[1], cmd.response[2],cmd.response[3]);

    uint32_t t = (1<<21); // Something related  to operating voltage
    if(cmd.response[0] == 0x1aa) {
        printf("HCS card found. \n");
        t |= (1<<30); // Sd only
    }

    uint32_t is_success = 0; 
    uint32_t timeout_count = 100;
    for(uint32_t count = 0; count < timeout_count; count++) {

        cmd.cmdidx = MMC_CMD_APP_CMD;
        cmd.resp_type = MMC_RSP_NONE;
        cmd.cmdarg = 0;
        is_success = bcm2835_send_cmd(sd_host, &cmd, (void *)0);

        MicroDelay(100);
        cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
        cmd.resp_type = MMC_RSP_R3;
        cmd.cmdarg = t;
        is_success = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
        // printf("Resp APP CMD: respo: %x %x %x %x \n", cmd.response[0], cmd.response[1], cmd.response[2],cmd.response[3]);
        if(cmd.response[0] & (1U<<31)) {  // Sd card is powered up
            // printf("SD card has arrived!\n");
            break;
        }
        if(count >= (timeout_count - 1) ) {
            printf("Sd card won't power up. \n");
            return -1;
        }
        MicroDelay(1000);
    }

    if(!is_success) {
        sd_host->mmc->ocr = cmd.response[0];
    }

    printf("SD card has arrived! OCR = %x\n", cmd.response[0]);
    if((cmd.response[0] & (1U<<30)) == (1U<<30)) {
        printf("This is an SDHC card!\n");
        sd_host->mmc->rca =1;
        sd_host->mmc->high_capacity = 1;
    }

    MicroDelay(1000);

    cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
    cmd.resp_type = MMC_RSP_R2;
    cmd.cmdarg = 0;

    is_success = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
	if (cmd.response[0] == 0) {
		int retries = 6;
		
		//It has been seen that SEND_CID may fail on the first
	    //	attempt, let's try a few more time
        cmd.cmdidx = MMC_CMD_ALL_SEND_CID;
        cmd.resp_type = MMC_RSP_R2;
        cmd.cmdarg = 0;
		do {
			is_success = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
			if (!is_success && (cmd.response[0] != 0)) {
                break;
            }
            MicroDelay(1000);
		} while (retries--);
	}

    if(cmd.response[0] == 0) {
        printf("Failure to get CID \n");
        
    } else {
        printf("Got CID \n");
        printf("Resp APP CID: respo: %x %x %x %x \n", cmd.response[0], cmd.response[1], cmd.response[2],cmd.response[3]);
        // sd_host->mmc->cid = 
        char pnm[8];
        SD_CID_PNM_CPY(cmd.response, pnm);
        printf("    Product : %s\n", pnm);
    }

    cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
    cmd.cmdarg =  1 << 16;
    cmd.resp_type = MMC_RSP_R6;
    is_success = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
    if(!is_success) {
        sd_host->mmc->rca = (cmd.response[0] >> 16) & 0xffff;
        printf("SD_CMD_SEND_RELATIVE_ADDR: RCA: %x cmd.response[0]:%x \n", sd_host->mmc->rca, cmd.response[0]);
    } else {
          printf("Cant do SD_CMD_SEND_RELATIVE_ADDR\n");
          return -1;
    }



    MicroDelay(1000);

    cmd.cmdidx = MMC_CMD_SEND_CSD;
    cmd.resp_type = MMC_RSP_R2;
    cmd.cmdarg = (sd_host->mmc->rca) <<16;
    bcm2835_send_cmd(sd_host, &cmd, (void *)0);
    printf("Resp APP CSD:  %x%x%x%x \n", cmd.response[0], cmd.response[1], cmd.response[2],cmd.response[3]);

    if(cmd.response != 0 ) {
        print_csd(&cmd.response[0]);
    }
    
    cmd.cmdidx = MMC_CMD_SELECT_CARD;
    cmd.cmdarg =  (sd_host->mmc->rca) <<16;
    cmd.resp_type = MMC_RSP_R1;
    is_success = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
    if(is_success) {
        printf("Can't select the card \n");
    }

    cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 512;
    is_success = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
    if(is_success) {
        printf("Can't set the block length \n");
    }
    sd_host->mmc->read_bl_len = 512;
    printf("Completed SD Card Init");
    sd_ctrl = sd_host;
    return 0;
}

int init_sdcard() {
   return use_sdhost();
}