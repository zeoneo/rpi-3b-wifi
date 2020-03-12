#include<stdbool.h>
#include <plibc/stdio.h>
#include <plibc/stddef.h>
#include <kernel/rpi-base.h>
#include <kernel/systimer.h>
#include <device/sdhost1.h>
#include <device/gpio.h>
#include <kernel/rpi-mailbox-interface.h>
#include <kernel/rpi-interrupts.h>

#define DEBUG_INFO 1
#if DEBUG_INFO == 1
#define LOG_DEBUG(...) printf(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#define msleep(a) MicroDelay(a * 1000)

#define SDHOSTREGS	(PERIPHERAL_BASE + 0x202000)

#define Mhz 1000 * 1000
enum {
	Extfreq		= 250*Mhz,	/* guess external clock frequency if vcore doesn't say */
	Initfreq	= 400000,	/* initialisation frequency for MMC */
	SDfreq		= 25*Mhz,	/* standard SD frequency */
	SDfreqhs	= 50*Mhz,	/* SD high speed frequency */
	FifoDepth	= 4,		/* "Limit fifo usage due to silicon bug" (linux driver) */

	GoIdle		= 0,		/* mmc/sdio go idle state */
	MMCSelect	= 7,		/* mmc/sd card select command */
	Setbuswidth	= 6,		/* mmc/sd set bus width command */
	Switchfunc	= 6,		/* mmc/sd switch function command */
	Stoptransmission = 12,	/* mmc/sd stop transmission command */
	Appcmd = 55,			/* mmc/sd application command prefix */
};

enum {
	/* Controller registers */
	Cmd		= 0x00>>2,
	Arg		= 0x04>>2,
	Timeout		= 0x08>>2,
	Clkdiv		= 0x0c>>2,

	Resp0		= 0x10>>2,
	Resp1		= 0x14>>2,
	Resp2		= 0x18>>2,
	Resp3		= 0x1c>>2,

	Status		= 0x20>>2,
	Poweron		= 0x30>>2,
	Dbgmode		= 0x34>>2,
	Hconfig		= 0x38>>2,
	Blksize		= 0x3c>>2,
	Data		= 0x40>>2,
	Blkcount	= 0x50>>2,

	/* Cmd */
	Start		= 1<<15,
	Failed		= 1<<14,
	Respmask	= 7<<9,
	Resp48busy	= 4<<9,
	Respnone	= 2<<9,
	Resp136		= 1<<9,
	Resp48		= 0<<9,
	Host2card	= 1<<7,
	Card2host	= 1<<6,

	/* Status */
	Busyint		= 1<<10,
	Blkint		= 1<<9,
	Sdioint		= 1<<8,
	Rewtimeout	= 1<<7,
	Cmdtimeout	= 1<<6,
	Crcerror	= 3<<4,
	Fifoerror	= 1<<3,
	Dataflag	= 1<<0,
	Intstatus	= (Busyint|Blkint|Sdioint|Dataflag),
	Errstatus	= (Rewtimeout|Cmdtimeout|Crcerror|Fifoerror),

	/* Hconfig */
	BusyintEn	= 1<<10,
	BlkintEn	= 1<<8,
	SdiointEn	= 1<<5,
	DataintEn	= 1<<4,
	Slowcard	= 1<<3,
	Extbus4		= 1<<2,
	Intbuswide	= 1<<1,
	Cmdrelease	= 1<<0,
};

static int cmdinfo[64] = {
[0]  Start | Respnone,
[2]  Start | Resp136,
[3]  Start | Resp48,
[5]  Start | Resp48,
[6]  Start | Resp48,
[63]  Start | Resp48 | Card2host,
[7]  Start | Resp48busy,
[8]  Start | Resp48,
[9]  Start | Resp136,
[11] Start | Resp48,
[12] Start | Resp48busy,
[13] Start | Resp48,
[16] Start | Resp48,
[17] Start | Resp48 | Card2host,
[18] Start | Resp48 | Card2host,
[24] Start | Resp48 | Host2card,
[25] Start | Resp48 | Host2card,
[41] Start | Resp48,
[52] Start | Resp48,
[53] Start | Resp48,
[55] Start | Resp48,
};

typedef struct Ctlr Ctlr;

struct Ctlr {
	uint32_t	bcount;
	int	done;
	uint32_t extclk;
	int	appcmd;
};

static Ctlr sdhost;

#define FIELD(v, o, w)	(((v) & ((1<<(w))-1))<<(o))
#define FCLR(d, o, w)	((d) & ~(((1<<(w))-1)<<(o)))

// #define FEXT(d, o, w)	(((d)>>(o)) & ((1<<(w))-1))

#define FINS(d, o, w, v) (FCLR((d), (o), (w))|FIELD((v), (o), (w)))
// #define FSET(d, o, w)	((d)|(((1<<(w))-1)<<(o)))


static void WR(uint32_t reg, uint32_t val)
{
	uint32_t *r = (uint32_t *)SDHOSTREGS;
	if(0) {
        LOG_DEBUG("WR %2.2ux %ux\n", reg<<2, val);
    }
	r[reg] = val;
}

// static int datadone()
// {
// 	return sdhost.done;
// }

static void sdhostclock(uint32_t freq)
{
	uint32_t div;

	div = sdhost.extclk / freq;
	if(sdhost.extclk / freq > freq) {
        div++;
    }
		
	if(div < 2) {
        div = 2;
    }
		
	WR(Clkdiv, div - 2);
}

static uint32_t get_core_clock() {
    RPI_PropertyInit();
	RPI_PropertyAddTag(TAG_GET_CLOCK_RATE, 0x04);
    RPI_PropertyProcess();
	rpi_mailbox_property_t *mp = RPI_PropertyGet(TAG_GET_CLOCK_RATE);

    uint32_t clk_id = (uint32_t)(mp->data.buffer_32[0]);
    uint32_t clk_rate = (uint32_t)(mp->data.buffer_32[1]);
	printf(" clk_id: %d clk_rate: %d ", clk_id, clk_rate);
    return clk_rate;
}

static void sdhost_interrupt()
{	
	uint32_t *r;
	int i;

	r = (uint32_t*)SDHOSTREGS;
	i = r[Status];
	WR(Status, i);
	if(i & Busyint){
		sdhost.done = 1;
		LOG_DEBUG("\n Interrupt marking data done \n");
	}
}
static void clear_irq() {
    LOG_DEBUG("\n Interrupt marking data done 2\n");
}
static void sdhostenable(void)
{
	WR(Poweron, 1);
	msleep(10);
	WR(Hconfig, Intbuswide | Slowcard | BusyintEn);
	WR(Clkdiv, 0x7FF);
    register_irq_handler(IRQ_SDHOST, sdhost_interrupt, clear_irq);
}

uint32_t init_sdhost()
{
	uint32_t *r;
	uint32_t clk;
	int i;

	/* disconnect emmc and connect sdhost to SD card gpio pins */
	for(i = 48; i <= 53; i++)
	{
        select_alt_func(i, Alt0);
    }	
    
	clk = get_core_clock();
	if(clk == 0){
		clk = Extfreq;
		printf("sdhost: assuming external clock %lud Mhz\n", clk/1000000);
	}
	sdhost.extclk = clk;
	sdhostclock(Initfreq);
	r = (uint32_t*)SDHOSTREGS;
	WR(Poweron, 0);
	WR(Timeout, 0xF00000);
	WR(Dbgmode, FINS(r[Dbgmode], 9, 10, (FifoDepth | FifoDepth<<5)));
    sdhostenable();
	return 0;
}
#define USED(x) if(x);else{}
void sdhost1_iosetup(int write, void *buf, int bsize, int bcount)
{
	USED(write);
	USED(buf);

	sdhost.bcount = bcount;
	WR(Blksize, bsize);
	WR(Blkcount, bcount);
}


// void sdhost1_io(int write, uint8_t *buf, int len)
// {
// 	uint32_t *r;
// 	int piolen;
// 	uint32_t w;

// 	r = (uint32_t*)SDHOSTREGS;
// 	if((len&3) != 0) {
//         LOG_DEBUG("Length error");
//         return;
//     }
// 	// assert((PTR2UINT(buf)&3) == 0);
//     if(((uint32_t)buf & 3) != 0) {
//         LOG_DEBUG("align Length error");
//         return;
//     }

// 	/*
// 	 * According to comments in the linux driver, the hardware "doesn't
// 	 * manage the FIFO DREQs properly for multi-block transfers" on input,
// 	 * so the dma must be stopped early and the last 3 words fetched with pio
// 	 */
// 	piolen = 0;
// 	if(!write && sdhost.bcount > 1){
// 		piolen = (FifoDepth-1) * sizeof(uint32_t);
// 		len -= piolen;
// 	}
// 	if(write)
// 		dmastart(DmaChanSdhost, DmaDevSdhost, DmaM2D,
// 			buf, &r[Data], len);
// 	else
// 		dmastart(DmaChanSdhost, DmaDevSdhost, DmaD2M,
// 			&r[Data], buf, len);
// 	if(dmawait(DmaChanSdhost) < 0)
// 		error(Eio);
	
//     if(!write){
// 		// cachedinvse(buf, len);
// 		buf += len;
// 		for(; piolen > 0; piolen -= sizeof(uint32_t)){
// 			if((r[Dbgmode] & 0x1F00) == 0){
// 				print("sdhost: FIFO empty after short dma read\n");
// 				error(Eio);
// 			}
// 			w = r[Data];
// 			*((uint32_t*)buf) = w;
// 			buf += sizeof(uint32_t);
// 		}
// 	}
// }

#define nelem(x) (sizeof(x)/sizeof((x)[0]))
// #define nil ((void*)0)
#define HZ 1000
int sdhost1_cmd(uint32_t cmd, uint32_t arg, uint32_t *resp)
{
	uint32_t volatile *r;
	uint32_t c;
	int i;
	uint64_t now;

	r = (uint32_t*)SDHOSTREGS;
	if(!(cmd < nelem(cmdinfo) && cmdinfo[cmd] != 0)) {
        LOG_DEBUG("Some error");
        return -1;
    }
	c = cmd | cmdinfo[cmd];
	/*
	 * CMD6 may be Setbuswidth or Switchfunc depending on Appcmd prefix
	 */
	if(cmd == Switchfunc && !sdhost.appcmd)
		c |= Card2host;
	if(cmd != Stoptransmission && (i = (r[Dbgmode] & 0xF)) > 2){
		printf("sdhost: previous command stuck: Dbg=%d Cmd=%x\n", i, r[Cmd]);
		return -1;
	}
	/*
	 * GoIdle indicates new card insertion: reset bus width & speed
	 */
	if(cmd == GoIdle){
		WR(Hconfig, r[Hconfig] & ~Extbus4);
		sdhostclock(Initfreq);
	}

	if(r[Status] & (Errstatus|Dataflag))
		WR(Status, Errstatus|Dataflag);
	sdhost.done = 0;
	WR(Arg, arg);
	WR(Cmd, c);

	now = timer_getTickCount64();
	
	while(r[Cmd] & Start) {
		if((timer_getTickCount64() - now) > 100000) {
			i = r[Cmd];
			break;
		}
	}
		
	if((i&(Start|Failed)) != 0){
		if(r[Status] != Cmdtimeout)
			printf("sdhost: cmd %ux arg %ux error stat %ux\n", i, arg, r[Status]);
		i = r[Status];
		WR(Status, i);
		// error(Eio);
        printf("error in send command : %x \n", r[Status]);
        return -1;
	}
	switch(c & Respmask){
	case Resp136:
		resp[0] = r[Resp0];
		resp[1] = r[Resp1];
		resp[2] = r[Resp2];
		resp[3] = r[Resp3];
		break;
	case Resp48:
	case Resp48busy:
		resp[0] = r[Resp0];
		break;
	case Respnone:
		resp[0] = 0;
		break;
	}
	if((c & Respmask) == Resp48busy){
        msleep(3000);
		// tsleep(&sdhost.r, datadone, 0, 3000);
	}
	switch(cmd) {
	case MMCSelect:
		/*
		 * Once card is selected, use faster clock
		 */
		msleep(1);
		sdhostclock(SDfreq);
		msleep(1);
		break;
	case Setbuswidth:
		if(sdhost.appcmd){
			/*
			 * If card bus width changes, change host bus width
			 */
			switch(arg){
			case 0:
				WR(Hconfig, r[Hconfig] & ~Extbus4);
				break;
			case 2:
				WR(Hconfig, r[Hconfig] | Extbus4);
				break;
			}
		}else{
			/*
			 * If card switched into high speed mode, increase clock speed
			 */
			if((arg&0x8000000F) == 0x80000001){
				msleep(1);
				sdhostclock(SDfreqhs);
				msleep(1);
			}
		}
		break;
	}
	sdhost.appcmd = (cmd == Appcmd);
	return 0;
}