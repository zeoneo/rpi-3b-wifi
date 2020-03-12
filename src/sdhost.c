#include<stdbool.h>
#include <plibc/stdio.h>
#include <plibc/stddef.h>
#include <kernel/rpi-base.h>
#include <kernel/systimer.h>
#include <device/sdhost.h>
#include <device/gpio.h>
#include <kernel/rpi-interrupts.h>
#include <kernel/rpi-mailbox-interface.h>

#define DEBUG_INFO 1
#if DEBUG_INFO == 1
#define LOG_DEBUG(...) printf(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#define min(x, y) ({				\
	x < y ? x : y; })



#define msleep(a) MicroDelay(a * 1000)

#define SDCMD  (0x00 >> 2) /* Command to SD card              - 16 R/W */
#define SDARG  (0x04 >> 2)/* Argument to SD card             - 32 R/W */
#define SDTOUT (0x08 >> 2) /* Start value for timeout counter - 32 R/W */
#define SDCDIV (0x0c >> 2)/* Start value for clock divider   - 11 R/W */
#define SDRSP0 (0x10 >> 2)/* SD card response (31:0)         - 32 R   */
#define SDRSP1 (0x14 >> 2)/* SD card response (63:32)        - 32 R   */
#define SDRSP2 (0x18 >> 2)/* SD card response (95:64)        - 32 R   */
#define SDRSP3 (0x1c >> 2)/* SD card response (127:96)       - 32 R   */
#define SDHSTS (0x20 >> 2)/* SD host status                  - 11 R/W */
#define SDVDD  (0x30 >> 2)/* SD card power control           -  1 R/W */
#define SDEDM  (0x34 >> 2)/* Emergency Debug Mode            - 13 R/W */
#define SDHCFG (0x38 >> 2)/* Host configuration              -  2 R/W */
#define SDHBCT (0x3c >> 2)/* Host byte count (debug)         - 32 R/W */
#define SDDATA (0x40 >> 2)/* Data to/from SD card            - 32 R/W */
#define SDHBLC (0x50 >> 2)/* Host block count (SDIO/SDHC)    -  9 R/W */

#define HC_CMD_ENABLE			0x8000
#define SDCMD_FAIL_FLAG			0x4000
#define SDCMD_BUSYWAIT			0x800
#define SDCMD_NO_RESPONSE		0x400
#define SDCMD_LONG_RESPONSE		0x200
#define SDCMD_WRITE_CMD			0x80
#define SDCMD_READ_CMD			0x40
#define SDCMD_CMD_MASK			0x3f

#define SDCDIV_MAX_CDIV			0x7ff

#define SDHSTS_BUSY_IRPT		0x400
#define SDHSTS_BLOCK_IRPT		0x200
#define SDHSTS_SDIO_IRPT		0x100
#define SDHSTS_REW_TIME_OUT		0x80
#define SDHSTS_CMD_TIME_OUT		0x40
#define SDHSTS_CRC16_ERROR		0x20
#define SDHSTS_CRC7_ERROR		0x10
#define SDHSTS_FIFO_ERROR		0x08
#define SDHSTS_DATA_FLAG		0x01

#define SDHSTS_CLEAR_MASK		(SDHSTS_BUSY_IRPT | \
					 SDHSTS_BLOCK_IRPT | \
					 SDHSTS_SDIO_IRPT | \
					 SDHSTS_REW_TIME_OUT | \
					 SDHSTS_CMD_TIME_OUT | \
					 SDHSTS_CRC16_ERROR | \
					 SDHSTS_CRC7_ERROR | \
					 SDHSTS_FIFO_ERROR)

#define SDHSTS_TRANSFER_ERROR_MASK	(SDHSTS_CRC7_ERROR | \
					 SDHSTS_CRC16_ERROR | \
					 SDHSTS_REW_TIME_OUT | \
					 SDHSTS_FIFO_ERROR)

#define SDHSTS_ERROR_MASK		(SDHSTS_CMD_TIME_OUT | \
					 SDHSTS_TRANSFER_ERROR_MASK)

#define SDHCFG_BUSY_IRPT_EN	BIT(10)
#define SDHCFG_BLOCK_IRPT_EN	BIT(8)
#define SDHCFG_SDIO_IRPT_EN	BIT(5)
#define SDHCFG_DATA_IRPT_EN	BIT(4)
#define SDHCFG_SLOW_CARD	BIT(3)
#define SDHCFG_WIDE_EXT_BUS	BIT(2)
#define SDHCFG_WIDE_INT_BUS	BIT(1)
#define SDHCFG_REL_CMD_LINE	BIT(0)

#define SDVDD_POWER_OFF		0
#define SDVDD_POWER_ON		1

#define SDEDM_FORCE_DATA_MODE	BIT(19)
#define SDEDM_CLOCK_PULSE	BIT(20)
#define SDEDM_BYPASS		BIT(21)

#define SDEDM_FIFO_FILL_SHIFT	4
#define SDEDM_FIFO_FILL_MASK	0x1f
static uint32_t edm_fifo_fill(uint32_t edm)
{
	return (edm >> SDEDM_FIFO_FILL_SHIFT) & SDEDM_FIFO_FILL_MASK;
}

#define SDEDM_WRITE_THRESHOLD_SHIFT	9
#define SDEDM_READ_THRESHOLD_SHIFT	14
#define SDEDM_THRESHOLD_MASK		0x1f

#define SDEDM_FSM_MASK		0xf
#define SDEDM_FSM_IDENTMODE	0x0
#define SDEDM_FSM_DATAMODE	0x1
#define SDEDM_FSM_READDATA	0x2
#define SDEDM_FSM_WRITEDATA	0x3
#define SDEDM_FSM_READWAIT	0x4
#define SDEDM_FSM_READCRC	0x5
#define SDEDM_FSM_WRITECRC	0x6
#define SDEDM_FSM_WRITEWAIT1	0x7
#define SDEDM_FSM_POWERDOWN	0x8
#define SDEDM_FSM_POWERUP	0x9
#define SDEDM_FSM_WRITESTART1	0xa
#define SDEDM_FSM_WRITESTART2	0xb
#define SDEDM_FSM_GENPULSES	0xc
#define SDEDM_FSM_WRITEWAIT2	0xd
#define SDEDM_FSM_STARTPOWDOWN	0xf

#define SDDATA_FIFO_WORDS	16

#define FIFO_READ_THRESHOLD	4
#define FIFO_WRITE_THRESHOLD	4
#define SDDATA_FIFO_PIO_BURST	8

#define SDHST_TIMEOUT_MAX_USEC	1000
#define SDHOSTREGS	(PERIPHERAL_BASE + 0x202000)

struct sdhost_state host_state = {0};
struct mmc_config host_cfg = {0};
struct mmc host_mmc = {0};

static void writel(uint32_t val, uint32_t reg)
{
	uint32_t *r = (uint32_t *)SDHOSTREGS;
	r[reg] = val;
}

static uint32_t readl(uint32_t reg)
{
	uint32_t *r = (uint32_t *)SDHOSTREGS;
	return r[reg];
}

#define USED(x) if(x);else{}

static void bcm2835_dumpregs()
{
	LOG_DEBUG("=========== REGISTER DUMP ===========\n");
	LOG_DEBUG("SDCMD  0x%08x\n", readl(SDCMD));
	LOG_DEBUG("SDARG  0x%08x\n", readl(SDARG));
	LOG_DEBUG("SDTOUT 0x%08x\n", readl(SDTOUT));
	LOG_DEBUG("SDCDIV 0x%08x\n", readl(SDCDIV));
	LOG_DEBUG("SDRSP0 0x%08x\n", readl(SDRSP0));
	LOG_DEBUG("SDRSP1 0x%08x\n", readl(SDRSP1));
	LOG_DEBUG("SDRSP2 0x%08x\n", readl(SDRSP2));
	LOG_DEBUG("SDRSP3 0x%08x\n", readl(SDRSP3));
	LOG_DEBUG("SDHSTS 0x%08x\n", readl(SDHSTS));
	LOG_DEBUG("SDVDD  0x%08x\n", readl(SDVDD));
	LOG_DEBUG("SDEDM  0x%08x\n", readl(SDEDM));
	LOG_DEBUG("SDHCFG 0x%08x\n", readl(SDHCFG));
	LOG_DEBUG("SDHBCT 0x%08x\n", readl(SDHBCT));
	LOG_DEBUG("SDHBLC 0x%08x\n", readl(SDHBLC));
	LOG_DEBUG("============================\n");
}

static void bcm2835_reset_internal(struct sdhost_state *host)
{
	uint32_t temp;

	writel(SDVDD_POWER_OFF, SDVDD);
	writel(0, SDCMD);
	writel(0, SDARG);
	/* Set timeout to a big enough value so we don't hit it */
	writel(0xf00000, SDTOUT);
	writel(0, SDCDIV);
	/* Clear status register */
	writel(SDHSTS_CLEAR_MASK, SDHSTS);
	writel(0, SDHCFG);
	writel(0, SDHBCT);
	writel(0, SDHBLC);

	/* Limit fifo usage due to silicon bug */
	temp = readl(SDEDM);
	temp &= ~((SDEDM_THRESHOLD_MASK << SDEDM_READ_THRESHOLD_SHIFT) |
		  (SDEDM_THRESHOLD_MASK << SDEDM_WRITE_THRESHOLD_SHIFT));
	temp |= (FIFO_READ_THRESHOLD << SDEDM_READ_THRESHOLD_SHIFT) |
		(FIFO_WRITE_THRESHOLD << SDEDM_WRITE_THRESHOLD_SHIFT);
	writel(temp, SDEDM);
	/* Wait for FIFO threshold to populate */
	msleep(20);
	writel(SDVDD_POWER_ON, SDVDD);
	/* Wait for all components to go through power on cycle */
	msleep(40);
	host->clock = 250 * 1000 * 1000;
	writel(host->hcfg, SDHCFG);
	writel(host->cdiv, SDCDIV);
	// bcm2835_dumpregs();
}

static void sdhost_interrupt_clearer() {
	LOG_DEBUG("Interrupt clearer called");
}

static void sdhost_interrupt_handler()
{	

	uint32_t i = readl(SDHSTS);
	writel(i, SDHSTS);
	if(i & SDHCFG_BUSY_IRPT_EN){
		LOG_DEBUG("Interrupt Done");
	}
	LOG_DEBUG("Interrupt hnadler called.");
}

static int bcm2835_wait_transfer_complete()
{
	int timediff = 0;

	while (1) {
		uint32_t edm, fsm;

		edm = readl(SDEDM);
		fsm = edm & SDEDM_FSM_MASK;

		if ((fsm == SDEDM_FSM_IDENTMODE) ||
		    (fsm == SDEDM_FSM_DATAMODE))
			break;

		if ((fsm == SDEDM_FSM_READWAIT) ||
		    (fsm == SDEDM_FSM_WRITESTART1) ||
		    (fsm == SDEDM_FSM_READDATA)) {
			writel(edm | SDEDM_FORCE_DATA_MODE, SDEDM);
			break;
		}

		/* Error out after 100000 register reads (~1s) */
		if (timediff++ == 100000) {
			LOG_DEBUG("wait_transfer_complete - still waiting after %d retries\n", timediff);
			bcm2835_dumpregs();
			return -1;
		}
	}

	return 0;
}

static int bcm2835_transfer_block_pio(struct sdhost_state *host, bool is_read)
{
	struct mmc_data *data = host->data;
	size_t blksize = data->blocksize;
	int copy_words;
	uint32_t hsts = 0;
	uint32_t *buf;

	if (blksize % sizeof(uint32_t)) {
        LOG_DEBUG("Size error \n");
        return -1;
    }
		

	buf = is_read ? (uint32_t *)data->dest : (uint32_t *)data->src;

	if (is_read) {
        data->dest += blksize;
    } else {
        data->src += blksize;
    }
		

	copy_words = blksize / sizeof(uint32_t);

	/*
	 * Copy all contents from/to the FIFO as far as it reaches,
	 * then wait for it to fill/empty again and rewind.
	 */
	while (copy_words) {
		int burst_words, words;
		uint32_t edm;

		burst_words = min(SDDATA_FIFO_PIO_BURST, copy_words);
		edm = readl(SDEDM);
		if (is_read) {
            words = edm_fifo_fill(edm);
        } else {
            words = SDDATA_FIFO_WORDS - edm_fifo_fill(edm);
        }
			

		if (words < burst_words) {
			int fsm_state = (edm & SDEDM_FSM_MASK);

			if ((is_read &&
			     (fsm_state != SDEDM_FSM_READDATA &&
			      fsm_state != SDEDM_FSM_READWAIT &&
			      fsm_state != SDEDM_FSM_READCRC)) ||
			    (!is_read &&
			     (fsm_state != SDEDM_FSM_WRITEDATA &&
			      fsm_state != SDEDM_FSM_WRITEWAIT1 &&
			      fsm_state != SDEDM_FSM_WRITEWAIT2 &&
			      fsm_state != SDEDM_FSM_WRITECRC &&
			      fsm_state != SDEDM_FSM_WRITESTART1 &&
			      fsm_state != SDEDM_FSM_WRITESTART2))) {

				hsts = readl(SDHSTS);
				LOG_DEBUG("fsm %x, hsts %08x\n", fsm_state, hsts);
				if (hsts & SDHSTS_ERROR_MASK) {
                    break;
                }
			}
			continue;
		} else if (words > copy_words) {
			words = copy_words;
		}
		copy_words -= words;

		/* Copy current chunk to/from the FIFO */
		while (words) {
			if (is_read)
				*(buf++) = readl(SDDATA);
			else
				writel(*(buf++), SDDATA);
			words--;
		}
	}

	return 0;
}

static int bcm2835_transfer_pio(struct sdhost_state *host)
{
	uint32_t sdhsts;
	bool is_read;
	int ret = 0;

	is_read = (host->data->flags & MMC_DATA_READ) != 0;
	ret = bcm2835_transfer_block_pio(host, is_read);
	if (ret)
		return ret;

	sdhsts = readl(SDHSTS);
	if (sdhsts & (SDHSTS_CRC16_ERROR | SDHSTS_CRC7_ERROR | SDHSTS_FIFO_ERROR)) {
		LOG_DEBUG("%s transfer error - HSTS %08x\n", is_read ? "read" : "write", sdhsts);
		ret =  -1;
	} else if ((sdhsts & (SDHSTS_CMD_TIME_OUT |
			      SDHSTS_REW_TIME_OUT))) {
		LOG_DEBUG("%s timeout error - HSTS %08x\n", is_read ? "read" : "write", sdhsts);
		ret = -2;
	}

	return ret;
}

static void bcm2835_prepare_data(struct sdhost_state *host, struct mmc_data *data)
{
	// LOG_DEBUG("preparing data \n ");

	host->data = data;
	if (!data)
		return;

	/* Use PIO */
	host->blocks = data->blocks;

	writel(data->blocksize, SDHBCT);
	writel(data->blocks, SDHBLC);
}

static int bcm2835_read_wait_sdcmd()
{
	int timeout = 1000;
	while ((readl(SDCMD) & HC_CMD_ENABLE) && --timeout > 0) {
		MicroDelay(SDHST_TIMEOUT_MAX_USEC);
	}

	// Timeout counter is either zero or -1
	if (timeout <= 0) {
        LOG_DEBUG("%s: timeout (%d us)\n", __func__, SDHST_TIMEOUT_MAX_USEC);
    }
	return readl(SDCMD);
}

static int bcm2835_send_command(struct sdhost_state *host, struct mmc_cmd *cmd, struct mmc_data *data)
{
	uint32_t sdcmd, sdhsts;

	// LOG_DEBUG("Command Index %d \n", cmd->cmdidx);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY)) {
		LOG_DEBUG("unsupported response type!\n");
		return -1;
	}

	sdcmd = bcm2835_read_wait_sdcmd(host);
	if (sdcmd & HC_CMD_ENABLE) {
		LOG_DEBUG("previous command never completed.\n");
		bcm2835_dumpregs();
		return -2;
	}

	host->cmd = cmd;
	// LOG_DEBUG("Host cmd: %x host->cmd->resp_type:%d %d\n", host->cmd, host->cmd->resp_type, cmd->resp_type);

	/* Clear any error flags */
	sdhsts = readl(SDHSTS);
	// bcm2835_dumpregs();
	if (sdhsts & SDHSTS_ERROR_MASK) {
        writel(sdhsts, SDHSTS);
    }
		
	writel(0, SDARG);
	MicroDelay(1000);
	bcm2835_prepare_data(host, data);
	// LOG_DEBUG("Starting command execution arg: %x \n", cmd->cmdarg);
	writel(cmd->cmdarg, SDARG);

	sdcmd = cmd->cmdidx & SDCMD_CMD_MASK;
	// LOG_DEBUG("Starting command execution sdcmd: %x \n", sdcmd);

	host->use_busy = 0;
	if (!(cmd->resp_type & MMC_RSP_PRESENT)) {
		sdcmd |= SDCMD_NO_RESPONSE;
	} else {
		if (cmd->resp_type & MMC_RSP_136)
			sdcmd |= SDCMD_LONG_RESPONSE;
		if (cmd->resp_type & MMC_RSP_BUSY) {
			sdcmd |= SDCMD_BUSYWAIT;
			host->use_busy = 1;
		}
	}

	if (data) {
		if (data->flags & MMC_DATA_WRITE)
			sdcmd |= SDCMD_WRITE_CMD;
		if (data->flags & MMC_DATA_READ)
			sdcmd |= SDCMD_READ_CMD;
	}

	writel(sdcmd | HC_CMD_ENABLE, SDCMD);
	// LOG_DEBUG("Ebnding command execution sdcmd: %x \n", sdcmd);
	return 0;
}

static int bcm2835_finish_command(struct sdhost_state *host)
{
	struct mmc_cmd *cmd = host->cmd;
	uint32_t sdcmd;
	int ret = 0;

	sdcmd = bcm2835_read_wait_sdcmd(host);

	/* Check for errors */
	if (sdcmd & HC_CMD_ENABLE) {
		LOG_DEBUG("command never completed. sdcmd: %x HC_CMD_ENABLE: %x\n", sdcmd, HC_CMD_ENABLE);
		bcm2835_dumpregs();
		return -1;
	} else if (sdcmd & SDCMD_FAIL_FLAG) {

		uint32_t sdhsts = readl(SDHSTS);
		if(sdhsts & SDHSTS_ERROR_MASK) {
			LOG_DEBUG("command Failed Check Error. sdcmd: %x status: %x\n", sdcmd, sdhsts);
			return -1;
		}
		LOG_DEBUG("command Failed Unknown Error. sdcmd: %x status: %x\n", sdcmd, sdhsts);
		return -1;

		/* Clear the errors */
		writel(SDHSTS_ERROR_MASK, SDHSTS);

		if (!(sdhsts & SDHSTS_CRC7_ERROR) || (host->cmd->cmdidx != MMC_CMD_SEND_OP_COND)) {
			if (sdhsts & SDHSTS_CMD_TIME_OUT) {
				LOG_DEBUG("unexpected error cond1:%d cond2:%d \n", (!(sdhsts & SDHSTS_CRC7_ERROR)), (host->cmd->cmdidx != MMC_CMD_SEND_OP_COND));
				bcm2835_dumpregs(host);
				ret = -1;
			} else {
				LOG_DEBUG("unexpected command %d error\n", host->cmd->cmdidx);
				cmd->response[0] = readl(SDRSP0);
				cmd->response[1] = readl(SDRSP1);
				cmd->response[2] = readl(SDRSP2);
				cmd->response[3] = readl(SDRSP3);
				bcm2835_dumpregs(host);
				ret = -2;
			}
			return ret;
		}
	}
		// LOG_DEBUG("Is command %d AA\n", cmd->resp_type);
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		// LOG_DEBUG("Is command %d  BB\n", cmd->resp_type);
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[0] = readl(SDRSP0);
			cmd->response[1] = readl(SDRSP1);
			cmd->response[2] = readl(SDRSP2);
			cmd->response[3] = readl(SDRSP3);
			bcm2835_dumpregs();
		} else {
			cmd->response[0] = readl(SDRSP0);
			// LOG_DEBUG("Is else command %d \n", cmd->resp_type);
		}
	}
	// bcm2835_dumpregs(host);
	/* Processed actual command. */
	host->cmd = NULL;
	// printf("Returning bcm2835_finish_command \n");
	return ret;
}

static int bcm2835_check_cmd_error(struct sdhost_state *host, uint32_t intmask)
{
	int ret = -1;

	if (!(intmask & SDHSTS_ERROR_MASK))
		return 0;

	if (!host->cmd)
		return -1;

	LOG_DEBUG("sdhost_busy_irq: intmask %08x\n", intmask);
	if (intmask & SDHSTS_CRC7_ERROR) {
		ret = -2;
	} else if (intmask & (SDHSTS_CRC16_ERROR |
			      SDHSTS_FIFO_ERROR)) {
		ret = -2;
	} else if (intmask & (SDHSTS_REW_TIME_OUT | SDHSTS_CMD_TIME_OUT)) {
		ret = -3;
	}
	bcm2835_dumpregs();
	return ret;
}

static int bcm2835_check_data_error(struct sdhost_state *host, uint32_t intmask)
{
	int ret = 0;

	if (!host->data)
		return 0;
	if (intmask & (SDHSTS_CRC16_ERROR | SDHSTS_FIFO_ERROR))
		ret = -1;
	if (intmask & SDHSTS_REW_TIME_OUT)
		ret = -2;

	if (ret)
		LOG_DEBUG("%s:%d %d\n", __func__, __LINE__, ret);

	return ret;
}

static int bcm2835_transmit(struct sdhost_state *host)
{
	uint32_t intmask = readl(SDHSTS);
	int ret;

	/* Check for errors */
	ret = bcm2835_check_data_error(host, intmask);
	if (ret) {
		LOG_DEBUG("Data error");
		return ret;
	}
		

	ret = bcm2835_check_cmd_error(host, intmask);
	if (ret) {
		LOG_DEBUG("cmd error");
		return ret;
	}

	/* Handle wait for busy end */
	if (host->use_busy && (intmask & SDHSTS_BUSY_IRPT)) {
		writel(SDHSTS_BUSY_IRPT, SDHSTS);
		host->use_busy = false;
		bcm2835_finish_command(host);
	}

	/* Handle PIO data transfer */
	if (host->data) {
		ret = bcm2835_transfer_pio(host);
		if (ret)
			return ret;
		host->blocks--;
		if (host->blocks == 0) {
			/* Wait for command to complete for real */
			ret = bcm2835_wait_transfer_complete(host);
			if (ret)
				return ret;
			/* Transfer complete */
			host->data = NULL;
		}
	}
	LOG_DEBUG("Return %s \n", __func__);
	return 0;
}

static void bcm2835_set_clock(struct sdhost_state *host, unsigned int clock)
{
	int div;

	/* The SDCDIV register has 11 bits, and holds (div - 2).  But
	 * in data mode the max is 50MHz wihout a minimum, and only
	 * the bottom 3 bits are used. Since the switch over is
	 * automatic (unless we have marked the card as slow...),
	 * chosen values have to make sense in both modes.  Ident mode
	 * must be 100-400KHz, so can range check the requested
	 * clock. CMD15 must be used to return to data mode, so this
	 * can be monitored.
	 *
	 * clock 250MHz -> 0->125MHz, 1->83.3MHz, 2->62.5MHz, 3->50.0MHz
	 *                 4->41.7MHz, 5->35.7MHz, 6->31.3MHz, 7->27.8MHz
	 *
	 *		 623->400KHz/27.8MHz
	 *		 reset value (507)->491159/50MHz
	 *
	 * BUT, the 3-bit clock divisor in data mode is too small if
	 * the core clock is higher than 250MHz, so instead use the
	 * SLOW_CARD configuration bit to force the use of the ident
	 * clock divisor at all times.
	 */

	if (clock < 100000) {
		/* Can't stop the clock, but make it as slow as possible
		 * to show willing
		 */
		host->cdiv = SDCDIV_MAX_CDIV;
		writel(host->cdiv, SDCDIV);
		return;
	}

	div = host->max_clk / clock;
	if (div < 2)
		div = 2;
	if ((host->max_clk / div) > clock)
		div++;
	div -= 2;

	if (div > SDCDIV_MAX_CDIV)
		div = SDCDIV_MAX_CDIV;

	clock = host->max_clk / (div + 2);
	host->mmc->clock = clock;

	/* Calibrate some delays */

	host->ns_per_fifo_word = (1000000000 / clock) *
		((host->mmc->card_caps & MMC_MODE_4BIT) ? 8 : 32);

	host->cdiv = div;
	writel(host->cdiv, SDCDIV);

	/* Set the timeout to 500ms */
	writel(host->mmc->clock / 2, SDTOUT);
}

static inline int is_power_of_2(uint64_t x)
{
	return !(x & (x - 1));
}

int bcm2835_send_cmd(struct sdhost_state *host, struct mmc_cmd *cmd, struct mmc_data *data)
{
	uint32_t edm, fsm;
	int ret = 0;
	// LOG_DEBUG("received command : %d \n ", cmd->cmdidx);

	if (data && !is_power_of_2(data->blocksize)) {
		LOG_DEBUG("unsupported block size (%d bytes)\n", data->blocksize);

		if (cmd)
			return -1;
	}

	edm = readl(SDEDM);
	fsm = edm & SDEDM_FSM_MASK;

	if ((fsm != SDEDM_FSM_IDENTMODE) &&
	    (fsm != SDEDM_FSM_DATAMODE) &&
	    (cmd && cmd->cmdidx != MMC_CMD_STOP_TRANSMISSION)) {
		LOG_DEBUG("previous command (%d) not complete (EDM %08x)\n",
		       readl(SDCMD) & SDCMD_CMD_MASK, edm);
		bcm2835_dumpregs(host);

		if (cmd) {
			LOG_DEBUG("Error command \n");
			return -1;
		}
		return 0;
	}

	// LOG_DEBUG("Previous command  %x \n", readl(SDCMD));
	if (cmd) {
		ret = bcm2835_send_command(host, cmd, data);
		if (!ret && !host->use_busy) {
			ret = bcm2835_finish_command(host);
		}
	}

	/* Wait for completion of busy signal or data transfer */
	while (host->use_busy || host->data) {
		LOG_DEBUG("host->use_busy : %d host->data: %x \n",host->use_busy , host->data);
		ret = bcm2835_transmit(host);
		if (ret) {
			break;
		}
	}
	// LOG_DEBUG("Return from command: \n");
	return ret;
}

int bcm2835_set_ios(struct sdhost_state *host)
{
	struct mmc *mmc = host->mmc;

	if (!mmc->clock || mmc->clock != host->clock) {
		bcm2835_set_clock(host, mmc->clock);
		printf("Setting clock \n");
		host->clock = mmc->clock;
	}

	/* set bus width */
	host->hcfg &= ~SDHCFG_WIDE_EXT_BUS;
	if (mmc->bus_width == 4)
		host->hcfg |= SDHCFG_WIDE_EXT_BUS;

	host->hcfg |= SDHCFG_WIDE_INT_BUS;

	/* Disable clever clock switching, to cope with fast core clocks */
	host->hcfg |= SDHCFG_SLOW_CARD;

	writel(host->hcfg, SDHCFG);

	return 0;
}

static void bcm2835_add_host(struct sdhost_state *host)
{
	struct mmc_config *cfg = host->cfg;

	cfg->f_max = host->max_clk;
	cfg->f_min = host->max_clk / SDCDIV_MAX_CDIV;
	cfg->b_max = 65535;

	LOG_DEBUG("f_max %d, f_min %d\n", cfg->f_max, cfg->f_min);

	/* host controller capabilities */
	cfg->host_caps = MMC_MODE_4BIT | MMC_MODE_HS | MMC_MODE_HS_52MHz;

	/* report supported voltage ranges */
	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;

	/* Set interrupt enables */
	host->hcfg = SDHCFG_BUSY_IRPT_EN;

	bcm2835_reset_internal(host);
	bcm2835_set_ios(host);

	MicroDelay(100);

	register_irq_handler(IRQ_SDHOST, sdhost_interrupt_handler, sdhost_interrupt_clearer);

}

static struct sdhost_state * bcm2835_probe()
{
	struct sdhost_state *host_ptr = &host_state;
	host_ptr->cfg = &host_cfg;
	host_ptr->mmc = &host_mmc;

	// Allocate all the memory here;
	// host->max_clk = bcm2835_get_mmc_clock(BCM2835_MBOX_CLOCK_ID_CORE);
	host_ptr->max_clk  = 250 * 1000 * 1000; // 250 Mhz
	bcm2835_add_host(host_ptr);
	LOG_DEBUG("%s -> OK\n", __func__);
	return host_ptr;
}

struct sdhost_state * sdhostInit () {
	    // Following lines connect to SD card to SD HOST

    // RPI_PropertyInit();
	// RPI_PropertyAddTag(TAG_GET_POWER_STATE, 0x0, 0x3);
    // RPI_PropertyProcess();
	// rpi_mailbox_property_t *mp = RPI_PropertyGet(TAG_GET_POWER_STATE);

    // uint32_t width = (uint32_t)(mp->data.buffer_32[0]);
    // uint32_t height = (uint32_t)(mp->data.buffer_32[1]);
	// printf(" id: %d state: %d ", width, height);

    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_SET_POWER_STATE, 0x0, 0x3);
    RPI_PropertyProcess();


	

	// FOllowing lines connect EMMC controller to wifi
    for ( uint32_t i = 34; i <= 39; i++)
    {
        select_alt_func(i, Alt3);
        // if (i == 34)
        //     disable_pulling(i); // Pull off
        // else
        //     pullup_pin(i);
    }

	for (uint32_t i = 48; i <= 53; i++) {
		
		// select_alt_func(i, Alt0);
		// if(i == 0x30) {
		// 	disable_pulling(i);
		// 	printf("Disable pulling \n");
		// } else {
		// 	pullup_pin(i);
		// }

        select_alt_func(i, Alt0);
    }
	return bcm2835_probe();
}
