/*
 * Broadcom bcm4330 wifi (sdio interface)
 */

#include<device/plan9_ether4330.h>
#include <device/gpio.h>
#include <device/plan9_emmc.h>
#include <kernel/systimer.h>
#include <plibc/stdio.h>
#include <plibc/string.h>
#include <mem/kernel_alloc.h>

#define USED(x) if(x);else{}
#define nelem(x)	(sizeof(x)/sizeof((x)[0]))

extern int sdiocardintr(int);

#define CACHELINESZ 64	/* temp */

enum{
	SDIODEBUG = 0,
	SBDEBUG = 0,
	EVENTDEBUG = 0,
	VARDEBUG = 0,
	FWDEBUG  = 0,

	Corescansz = 512,
	Uploadsz = 2048,

	Wifichan = 0,		/* default channel */
	Firmwarecmp	= 1,

	ARMcm3		= 0x82A,
	ARM7tdmi	= 0x825,
	ARMcr4		= 0x83E,

	Fn0	= 0,
	Fn1 	= 1,
	Fn2	= 2,
	Fbr1	= 0x100,
	Fbr2	= 0x200,

	/* CCCR */
	Ioenable	= 0x02,
	Ioready		= 0x03,
	Intenable	= 0x04,
	Intpend		= 0x05,
	Ioabort		= 0x06,
	Busifc		= 0x07,
	Capability	= 0x08,
	Blksize		= 0x10,
	Highspeed	= 0x13,

	/* SDIOCommands */
	GO_IDLE_STATE		= 0,
	SEND_RELATIVE_ADDR	= 3,
	IO_SEND_OP_COND		= 5,
	SELECT_CARD		= 7,
	VOLTAGE_SWITCH 		= 11,
	IO_RW_DIRECT 		= 52,
	IO_RW_EXTENDED 		= 53,

	/* SELECT_CARD args */
	Rcashift	= 16,

	/* SEND_OP_COND args */
	Hcs	= 1<<30,	/* host supports SDHC & SDXC */
	V3_3	= 3<<20,	/* 3.2-3.4 volts */
	V2_8	= 3<<15,	/* 2.7-2.9 volts */
	V2_0	= 1<<8,		/* 2.0-2.1 volts */
	S18R	= 1<<24,	/* switch to 1.8V request */

	/* Sonics Silicon Backplane (access to cores on chip) */
	Sbwsize	= 0x8000,
	Sb32bit	= 0x8000,
	Sbaddr	= 0x1000a,
		Enumbase	= 	0x18000000,
	Framectl= 0x1000d,
		Rfhalt		=	0x01,
		Wfhalt		=	0x02,
	Clkcsr	= 0x1000e,
		ForceALP	=	0x01,	/* active low-power clock */
		ForceHT		= 	0x02,	/* high throughput clock */
		ForceILP	=	0x04,	/* idle low-power clock */
		ReqALP		=	0x08,
		ReqHT		=	0x10,
		Nohwreq		=	0x20,
		ALPavail	=	0x40,
		HTavail		=	0x80,
	Pullups	= 0x1000f,
	Wfrmcnt	= 0x10019,
	Rfrmcnt	= 0x1001b,
		
	/* core control regs */
	Ioctrl		= 0x408,
	Resetctrl	= 0x800,

	/* socram regs */
	Coreinfo	= 0x00,
	Bankidx		= 0x10,
	Bankinfo	= 0x40,
	Bankpda		= 0x44,

	/* armcr4 regs */
	Cr4Cap		= 0x04,
	Cr4Bankidx	= 0x40,
	Cr4Bankinfo	= 0x44,
	Cr4Cpuhalt	= 0x20,

	/* chipcommon regs */
	Gpiopullup	= 0x58,
	Gpiopulldown	= 0x5c,
	Chipctladdr	= 0x650,
	Chipctldata	= 0x654,

	/* sdio core regs */
	Intstatus	= 0x20,
		Fcstate		= 1<<4,
		Fcchange	= 1<<5,
		FrameInt	= 1<<6,
		MailboxInt	= 1<<7,
	Intmask		= 0x24,
	Sbmbox		= 0x40,
	Sbmboxdata	= 0x48,
	Hostmboxdata= 0x4c,
		Fwready		= 0x80,

	/* wifi control commands */
	GetVar	= 262,
	SetVar	= 263,

	/* status */
	Disconnected=	0,
	Connecting,
	Connected,
};

// static Ctlr ctrl = {0};

enum{
	Wpa		= 1,
	Wep		= 2,
	Wpa2		= 3,
	WNameLen	= 32,
	WNKeys		= 4,
	WKeyLen		= 32,
	WMinKeyLen	= 5,
	WMaxKeyLen	= 13,
};

typedef struct WKey WKey;
struct WKey
{
	uint16_t	len;
	char	dat[WKeyLen];
};

#define HOWMANY(x, y)	(((x)+((y)-1))/(y))
#define ROUNDUP(x, y)	(HOWMANY((x), (y))*(y))	/* ceiling */
#define ROUNDDN(x, y)	(((x)/(y))*(y))		/* floor */
#define	ROUND(s, sz)	(((s)+(sz-1))&~(sz-1))

typedef struct  {
	int	joinstatus;
	int	cryptotype;
	int	chanid;
	char	essid[WNameLen + 1];
	WKey	keys[WNKeys];
	int	scansecs;
	int	status;
	int	chipid;
	int	chiprev;
	int	armcore;
	char	*regufile;
	union {
		uint32_t i;
		uint8_t c[4];
	} resetvec;
	uint32_t	chipcommon;
	uint32_t	armctl;
	uint32_t	armregs;
	uint32_t	d11ctl;
	uint32_t	socramregs;
	uint32_t	socramctl;
	uint32_t	sdregs;
	int	sdiorev;
	int	socramrev;
	uint32_t	socramsize;
	uint32_t	rambase;
	short	reqid;
	uint8_t	fcmask;
	uint8_t	txwindow;
	uint8_t	txseq;
	uint8_t	rxseq;
} Ctlr;

static Ctlr ctrl = { 0 };

enum{
	CMauth,
	CMchannel,
	CMcrypt,
	CMessid,
	CMkey1,
	CMkey2,
	CMkey3,
	CMkey4,
	CMrxkey,
	CMrxkey0,
	CMrxkey1,
	CMrxkey2,
	CMrxkey3,
	CMtxkey,
	CMdebug,
	CMjoin,
};

// static Cmdtab cmds[] = {
// 	{CMauth,	"auth", 2},
// 	{CMchannel,	"channel", 2},
// 	{CMcrypt,	"crypt", 2},
// 	{CMessid,	"essid", 2},
// 	{CMkey1,	"key1",	2},
// 	{CMkey2,	"key1",	2},
// 	{CMkey3,	"key1",	2},
// 	{CMkey4,	"key1",	2},
// 	{CMrxkey,	"rxkey", 3},
// 	{CMrxkey0,	"rxkey0", 3},
// 	{CMrxkey1,	"rxkey1", 3},
// 	{CMrxkey2,	"rxkey2", 3},
// 	{CMrxkey3,	"rxkey3", 3},
// 	{CMtxkey,	"txkey", 3},
// 	{CMdebug,	"debug", 2},
// 	{CMjoin,	"join", 5},
// };

#if !defined(__STRINGIFY2)
#define __STRINGIFY2(__p) #__p
#define __STRINGIFY(__p) __STRINGIFY2(__p)
#endif

#define INCLUDE_BINARY_FILE(__variable, __fileName, __section)					 \
__asm__ (                                                                        \
    ".pushsection " __section                                          "\n\t"    \
    ".globl " __STRINGIFY(__variable) "_start;"                        "\n\t"    \
    ".balign 4"                                                        "\n\t"    \
    __STRINGIFY(__variable) "_start: .incbin \"" __fileName "\""       "\n\t"    \
    ".globl " __STRINGIFY(__variable) "_end;"		                   "\n\t"    \
    __STRINGIFY(__variable) "_end: .4byte 0;"                          "\n\t"    \
    ".balign 4"                                                        "\n\t"    \
    ".popsection"                                                      "\n\t"    \
);\
extern const uint8_t __variable ## _start;\
extern const  uint8_t __variable ## _end;

INCLUDE_BINARY_FILE(wifibinary, "lib/brcmfmac43430-sdio.bin", ".rodata.brcmfmac43430_sdio_bin");
INCLUDE_BINARY_FILE(wifitext, "lib/brcmfmac43430-sdio.txt", ".rodata.brcmfmac43430_sdio_txt");

typedef struct Sdpcm Sdpcm;
typedef struct Cmd Cmd;
struct Sdpcm {
	uint8_t	len[2];
	uint8_t	lenck[2];
	uint8_t	seq;
	uint8_t	chanflg;
	uint8_t	nextlen;
	uint8_t	doffset;
	uint8_t	fcmask;
	uint8_t	window;
	uint8_t	version;
	uint8_t	pad;
};

struct Cmd {
	uint8_t	cmd[4];
	uint8_t	len[4];
	uint8_t	flags[2];
	uint8_t	id[2];
	uint8_t	status[4];
};
typedef struct {
	uint8_t *file_buf;
	uint8_t *current_buf;
	uint32_t size;
} FileInfo;

FileInfo bin_file = { 0 };
FileInfo text_file = { 0 };

static char config40181[] = "bcmdhd.cal.40181";
static char config40183[] = "bcmdhd.cal.40183.26MHz";

struct {
	int chipid;
	int chiprev;
	char *fwfile;
	char *cfgfile;
	char *regufile;
} firmware[] = {
	{ 0x4330, 3,	"fw_bcm40183b1.bin", config40183, 0 },
	{ 0x4330, 4,	"fw_bcm40183b2.bin", config40183, 0 },
	{ 43362, 0,	"fw_bcm40181a0.bin", config40181, 0 },
	{ 43362, 1,	"fw_bcm40181a2.bin", config40181, 0 },
	{ 43430, 1,	"brcmfmac43430-sdio.bin", "brcmfmac43430-sdio.txt", 0 },
	{ 0x4345, 6, "brcmfmac43455-sdio.bin", "brcmfmac43455-sdio.txt", "brcmfmac43455-sdio.clm_blob" },
};

// static QLock sdiolock;
// static int iodebug;

// static void etherbcmintr(void *);
// static void bcmevent(Ctlr*, uint8_t*, int);
// static void wlscanresult(uint8_t*, int);
// static void wlsetvar(Ctlr*, char*, void*, int);

// static uint8_t* put2(uint8_t *p, short v)
// {
// 	p[0] = v;
// 	p[1] = v >> 8;
// 	return p + 2;
// }

static uint8_t* put4(uint8_t *p, long v)
{
	p[0] = v;
	p[1] = v >> 8;
	p[2] = v >> 16;
	p[3] = v >> 24;
	return p + 4;
}

// static uint16_t get2(uint8_t *p)
// {
// 	return p[0] | p[1]<<8;
// }

// static uint32_t get4(uint8_t *p)
// {
// 	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
// }

// static void dump(char *s, void *a, int n)
// {
// 	int i;
// 	uint8_t *p;

// 	p = a;
// 	printf("%s:", s);
// 	for(i = 0; i < n; i++)
// 		printf("%c%2.2x", i&15? ' ' : '\n', *p++);
// 	printf("\n");
// }

/*
 * SDIO communication with dongle
 */
static uint32_t
sdiocmd_locked(int cmd, uint32_t arg)
{
	uint32_t resp[4];

	emmccmd(cmd, arg, resp);
	return resp[0];
}

static uint32_t
sdiocmd(int cmd, uint32_t arg)
{
	uint32_t r;

	// qlock(&sdiolock);
	// if(waserror()){
	// 	if(SDIODEBUG) printf("sdiocmd error: cmd %d arg %lux\n", cmd, arg);
	// 	qunlock(&sdiolock);
	// 	nexterror();
	// }
	r = sdiocmd_locked(cmd, arg);
	// qunlock(&sdiolock);
	// poperror();
	return r;

}

static uint32_t
trysdiocmd(int cmd, uint32_t arg)
{
	uint32_t r;

	// if(waserror())
	// 	return 0;
	r = sdiocmd(cmd, arg);
	// poperror();
	return r;
}

static int
sdiord(int fn, int addr)
{
	int r;

	r = sdiocmd(IO_RW_DIRECT, (0<<31)|((fn&7)<<28)|((addr&0x1FFFF)<<9));
	if(r & 0xCF00){
		printf("ERROR ether4330: sdiord(%x, %x) fail: %2.2ux %2.2ux\n", fn, addr, (r>>8)&0xFF, r&0xFF);
		return 0;
	}
	return r & 0xFF;
}

static void
sdiowr(int fn, int addr, int data)
{
	int r;
	int retry;

	r = 0;
	for(retry = 0; retry < 10; retry++){
		r = sdiocmd(IO_RW_DIRECT, (1<<31)|((fn&7)<<28)|((addr&0x1FFFF)<<9)|(data&0xFF));
		if((r & 0xCF00) == 0)
			return;
	}
	printf("ERROR ether4330: sdiowr(%x, %x, %x) fail: %2.2ux %2.2ux\n", fn, addr, data, (r>>8)&0xFF, r&0xFF);
	// error(Eio);
}

static void
sdiorwext(int fn, int write, void *a, int len, int addr, int incr)
{
	int bsize, blk, bcount, m;

	bsize = fn == Fn2? 512 : 64;
	while(len > 0){
		if(len >= 511*bsize){
			blk = 1;
			bcount = 511;
			m = bcount*bsize;
		}else if(len > bsize){
			blk = 1;
			bcount = len/bsize;
			m = bcount*bsize;
		}else{
			blk = 0;
			bcount = len;
			m = bcount;
		}
		// qlock(&sdiolock);
		// if(waserror()){
			// printf("ether4330: sdiorwext fail: %s\n", up->errstr);
			// qunlock(&sdiolock);
		// 	nexterror();
		// }
		if(blk)
			emmciosetup(write, a, bsize, bcount);
		else
			emmciosetup(write, a, bcount, 1);
		sdiocmd_locked(IO_RW_EXTENDED,
			write<<31 | (fn&7)<<28 | blk<<27 | incr<<26 | (addr&0x1FFFF)<<9 | (bcount&0x1FF));
		emmcio(write, a, m);
		// qunlock(&sdiolock);
		// poperror();
		len -= m;
		a = (char*)a + m;
		if(incr)
			addr += m;
	}
}

static void
sdioset(int fn, int addr, int bits)
{
	sdiowr(fn, addr, sdiord(fn, addr) | bits);
}
	
static void
sdioinit(void)
{
	uint32_t ocr, rca;
	int i;
	
	for (uint32_t i = 48; i <= 53; i++) {
        select_alt_func(i, Alt0);
    }

	// FOllowing lines connect EMMC controller to SD CARD
    for ( uint32_t i = 34; i <= 39; i++)
    {
        select_alt_func(i, Alt3);
		if(i == 34) {
			disable_pulling(i);
		} else {
			pullup_pin(i);
		}
    }

	emmcinit();
	emmcenable();
	sdiocmd(GO_IDLE_STATE, 0);
	ocr = trysdiocmd(IO_SEND_OP_COND, 0);
	i = 0;
	while((ocr & (1<<31)) == 0){
		if(++i > 5){
			printf("ERROR ether4330: no response to sdio access: ocr = %lux\n", ocr);
			// error(Eio);
			return;
		}
		ocr = trysdiocmd(IO_SEND_OP_COND, V3_3);
		// tsleep(&up->sleep, return0, nil, 100);
		MicroDelay(100);
	}
	rca = sdiocmd(SEND_RELATIVE_ADDR, 0) >> Rcashift;
	sdiocmd(SELECT_CARD, rca << Rcashift);
	sdioset(Fn0, Highspeed, 2);
	sdioset(Fn0, Busifc, 2);	/* bus width 4 */
	sdiowr(Fn0, Fbr1+Blksize, 64);
	sdiowr(Fn0, Fbr1+Blksize+1, 64>>8);
	sdiowr(Fn0, Fbr2+Blksize, 512);
	sdiowr(Fn0, Fbr2+Blksize+1, 512>>8);
	sdioset(Fn0, Ioenable, 1<<Fn1);
	sdiowr(Fn0, Intenable, 0);
	for(i = 0; !(sdiord(Fn0, Ioready) & 1<<Fn1); i++){
		if(i == 10){
			printf("ERROR: ether4330: can't enable SDIO function\n");
			// error(Eio);
		}
		// tsleep(&up->sleep, return0, nil, 100);
		MicroDelay(100);
	}
}

// static void
// sdioreset(void)
// {
// 	sdiowr(Fn0, Ioabort, 1<<3);	/* reset */
// }

// static void
// sdioabort(int fn)
// {
// 	sdiowr(Fn0, Ioabort, fn);
// }

/*
 * Chip register and memory access via SDIO
 */

static void
cfgw(uint32_t off, int val)
{
	sdiowr(Fn1, off, val);
}

static int
cfgr(uint32_t off)
{
	return sdiord(Fn1, off);
}

static uint32_t
cfgreadl(int fn, uint32_t off)
{
	uint8_t cbuf[2*CACHELINESZ];
	uint8_t *p;

	p = (uint8_t*)ROUND((uint32_t) &cbuf[0], CACHELINESZ);
	memset(p, 0, 4);
	sdiorwext(fn, 0, p, 4, off|Sb32bit, 1);
	if(SDIODEBUG) printf("cfgreadl %lux: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
}

static void
cfgwritel(int fn, uint32_t off, uint32_t data)
{
	uint8_t cbuf[2*CACHELINESZ];
	uint8_t *p;
	// int retry;

	p = (uint8_t*)ROUND((uint32_t) &cbuf[0], CACHELINESZ); //TODO check if this is working correctly
	put4(p, data);
	if(SDIODEBUG) printf("cfgwritel %lux: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
	// retry = 0;
	// while(waserror()){
	// 	printf("ether4330: cfgwritel retry %lux %ux\n", off, data);
	// 	sdioabort(fn);
	// 	// if(++retry == 3)
	// 	// 	nexterror();
	// }
	sdiorwext(fn, 1, p, 4, off|Sb32bit, 1);
	// poperror();
}

static void
sbwindow(uint32_t addr)
{
	addr &= ~(Sbwsize-1);
	cfgw(Sbaddr, addr>>8);
	cfgw(Sbaddr+1, addr>>16);
	cfgw(Sbaddr+2, addr>>24);
}

static void
sbrw(int fn, int write, uint8_t *buf, int len, uint32_t off)
{
	int n;
	USED(fn);

	// if(waserror()){
	// 	printf("ether4330: sbrw err off %lux len %ud\n", off, len);
	// 	nexterror();
	// }
	if(write){
		if(len >= 4){
			n = len;
			n &= ~3;
			sdiorwext(Fn1, write, buf, n, off|Sb32bit, 1);
			off += n;
			buf += n;
			len -= n;
		}
		while(len > 0){
			sdiowr(Fn1, off|Sb32bit, *buf);
			off++;
			buf++;
			len--;
		}
	}else{
		if(len >= 4){
			n = len;
			n &= ~3;
			sdiorwext(Fn1, write, buf, n, off|Sb32bit, 1);
			off += n;
			buf += n;
			len -= n;
		}
		while(len > 0){
			*buf = sdiord(Fn1, off|Sb32bit);
			off++;
			buf++;
			len--;
		}
	}
	// poperror();
}

static void
sbmem(int write, uint8_t *buf, int len, uint32_t off)
{
	uint32_t n;

	n = ROUNDUP(off, Sbwsize) - off;
	if(n == 0)
		n = Sbwsize;
	while(len > 0){
		if(n > len)
			n = len;
		sbwindow(off);
		sbrw(Fn1, write, buf, n, off & (Sbwsize-1));
		off += n;
		buf += n;
		len -= n;
		n = Sbwsize;
	}
}

// static void
// packetrw(int write, uint8_t *buf, int len)
// {
// 	int n;
// 	int retry;

// 	n = 2048;
// 	while(len > 0){
// 		if(n > len)
// 			n = ROUND(len, 4);
// 		retry = 0;
// 		while(waserror()){
// 			sdioabort(Fn2);
// 			if(++retry == 3)
// 				nexterror();
// 		}
// 		sdiorwext(Fn2, write, buf, n, Enumbase, 0);
// 		poperror();
// 		buf += n;
// 		len -= n;
// 	}
// }

/*
 * Configuration and control of chip cores via Silicon Backplane
 */

static void
sbdisable(uint32_t regs, int pre, int ioctl)
{
	sbwindow(regs);
	if((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0){
		cfgwritel(Fn1, regs + Ioctrl, 3|ioctl);
		cfgreadl(Fn1, regs + Ioctrl);
		return;
	}
	cfgwritel(Fn1, regs + Ioctrl, 3|pre);
	cfgreadl(Fn1, regs + Ioctrl);
	cfgwritel(Fn1, regs + Resetctrl, 1);
	MicroDelay(10);
	while((cfgreadl(Fn1, regs + Resetctrl) & 1) == 0)
		;
	cfgwritel(Fn1, regs + Ioctrl, 3|ioctl);
	cfgreadl(Fn1, regs + Ioctrl);
}

static void
sbreset(uint32_t regs, int pre, int ioctl)
{
	sbdisable(regs, pre, ioctl);
	sbwindow(regs);
	if(SBDEBUG) printf("sbreset %#p %#lux %#lux ->", regs,
		cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));
	while((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0){
		cfgwritel(Fn1, regs + Resetctrl, 0);
		MicroDelay(40);
	}
	cfgwritel(Fn1, regs + Ioctrl, 1|ioctl);
	cfgreadl(Fn1, regs + Ioctrl);
	if(SBDEBUG) printf("%#lux %#lux\n",
		cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));
}

static void
corescan(Ctlr *ctl, uint32_t r)
{
	uint8_t *buf;
	int i, coreid, corerev;
	uint32_t addr;

	buf = mem_allocate(Corescansz);
	if(buf == 0) {
		// error(Enomem);
		printf("Error no memory for buf \n");
		return;
	}
		
	sbmem(0, buf, Corescansz, r);
	coreid = 0;
	corerev = 0;
	for(i = 0; i < Corescansz; i += 4){
		switch(buf[i]&0xF){
		case 0xF:	/* end */
			mem_deallocate(buf);
			return;
		case 0x1:	/* core info */
			if((buf[i+4]&0xF) != 0x1)
				break;
			coreid = (buf[i+1] | buf[i+2]<<8) & 0xFFF;
			i += 4;
			corerev = buf[i+3];
			break;
		case 0x05:	/* address */
			addr = buf[i+1]<<8 | buf[i+2]<<16 | buf[i+3]<<24;
			addr &= ~0xFFF;
			if(SBDEBUG) printf("core %x %s %#p\n", coreid, buf[i]&0xC0? "ctl" : "mem", addr);
			switch(coreid){
			case 0x800:
				if((buf[i] & 0xC0) == 0)
					ctl->chipcommon = addr;
				break;
			case ARMcm3:
			case ARM7tdmi:
			case ARMcr4:
				ctl->armcore = coreid;
				if(buf[i] & 0xC0){
					if(ctl->armctl == 0)
						ctl->armctl = addr;
				}else{
					if(ctl->armregs == 0)
						ctl->armregs = addr;
				}
				break;
			case 0x80E:
				if(buf[i] & 0xC0)
					ctl->socramctl = addr;
				else if(ctl->socramregs == 0)
					ctl->socramregs = addr;
				ctl->socramrev = corerev;
				break;
			case 0x829:
				if((buf[i] & 0xC0) == 0)
					ctl->sdregs = addr;
				ctl->sdiorev = corerev;
				break;
			case 0x812:
				if(buf[i] & 0xC0)
					ctl->d11ctl = addr;
				break;
			}
		}
	}
	mem_deallocate(buf);
}

static void
ramscan(Ctlr *ctl)
{
	uint32_t r, n, size;
	int banks, i;

	if(ctl->armcore == ARMcr4){
		r = ctl->armregs;
		sbwindow(r);
		n = cfgreadl(Fn1, r + Cr4Cap);
		if(SBDEBUG) printf("cr4 banks %lux\n", n);
		banks = ((n>>4) & 0xF) + (n & 0xF);
		size = 0;
		for(i = 0; i < banks; i++){
			cfgwritel(Fn1, r + Cr4Bankidx, i);
			n = cfgreadl(Fn1, r + Cr4Bankinfo);
			if(SBDEBUG) printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
			size += 8192 * ((n & 0x3F) + 1);
		}
		ctl->socramsize = size;
		ctl->rambase = 0x198000;
		return;
	}
	if(ctl->socramrev <= 7 || ctl->socramrev == 12){
		printf("ERRROR ether4330: SOCRAM rev %d not supported\n", ctl->socramrev);
		// error(Eio);
		return;
	}
	sbreset(ctl->socramctl, 0, 0);
	r = ctl->socramregs;
	sbwindow(r);
	n = cfgreadl(Fn1, r + Coreinfo);
	if(SBDEBUG) printf("socramrev %d coreinfo %lux\n", ctl->socramrev, n);
	banks = (n>>4) & 0xF;
	size = 0;
	for(i = 0; i < banks; i++){
		cfgwritel(Fn1, r + Bankidx, i);
		n = cfgreadl(Fn1, r + Bankinfo);
		if(SBDEBUG) printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
		size += 8192 * ((n & 0x3F) + 1);
	}
	ctl->socramsize = size;
	ctl->rambase = 0;
	if(ctl->chipid == 43430){
		cfgwritel(Fn1, r + Bankidx, 3);
		cfgwritel(Fn1, r + Bankpda, 0);
	}
}

static void
sbinit(Ctlr *ctl)
{
	uint32_t r;
	int chipid;
	char buf[16];

	sbwindow(Enumbase);
	r = cfgreadl(Fn1, Enumbase);
	chipid = r & 0xFFFF;
	printf("Chip Id : 0x%x %d \n", chipid, chipid);
	printf("ether4330: chip %s rev %ld type %ld\n", buf, (r>>16)&0xF, (r>>28)&0xF);
	switch(chipid){
		case 0x4330:
		case 43362:
		case 43430:
		case 0x4345:
			ctl->chipid = chipid;
			ctl->chiprev = (r>>16)&0xF;
			break;
		default:
			printf("ERROR ether4330: chipid %#x (%d) not supported\n", chipid, chipid);
			// error(Eio);
			return;
	}
	r = cfgreadl(Fn1, Enumbase + 63*4);
	corescan(ctl, r);
	if(ctl->armctl == 0 || ctl->d11ctl == 0 ||
	   (ctl->armcore == ARMcm3 && (ctl->socramctl == 0 || ctl->socramregs == 0)))
		printf("corescan didn't find essential cores\n");
	if(ctl->armcore == ARMcr4)
		sbreset(ctl->armctl, Cr4Cpuhalt, Cr4Cpuhalt);
	else	
		sbdisable(ctl->armctl, 0, 0);
	sbreset(ctl->d11ctl, 8|4, 4);
	ramscan(ctl);
	if(SBDEBUG) printf("ARM %#p D11 %#p SOCRAM %#p,%#p %lud bytes @ %#p\n",
		ctl->armctl, ctl->d11ctl, ctl->socramctl, ctl->socramregs, ctl->socramsize, ctl->rambase);
	cfgw(Clkcsr, 0);
	MicroDelay(10);
	if(SBDEBUG) printf("chipclk: %x\n", cfgr(Clkcsr));
	cfgw(Clkcsr, Nohwreq | ReqALP);
	while((cfgr(Clkcsr) & (HTavail|ALPavail)) == 0)
		MicroDelay(10);
	cfgw(Clkcsr, Nohwreq | ForceALP);
	MicroDelay(65);
	if(SBDEBUG) printf("chipclk: %x\n", cfgr(Clkcsr));
	cfgw(Pullups, 0);
	sbwindow(ctl->chipcommon);
	cfgwritel(Fn1, ctl->chipcommon + Gpiopullup, 0);
	cfgwritel(Fn1, ctl->chipcommon + Gpiopulldown, 0);
	if(ctl->chipid != 0x4330 && ctl->chipid != 43362)
		return;
	cfgwritel(Fn1, ctl->chipcommon + Chipctladdr, 1);
	if(cfgreadl(Fn1, ctl->chipcommon + Chipctladdr) != 1)
		printf("ether4330: can't set Chipctladdr\n");
	else{
		r = cfgreadl(Fn1, ctl->chipcommon + Chipctldata);
		if(SBDEBUG) printf("chipcommon PMU (%lux) %lux", cfgreadl(Fn1, ctl->chipcommon + Chipctladdr), r);
		/* set SDIO drive strength >= 6mA */
		r &= ~0x3800;
		if(ctl->chipid == 0x4330)
			r |= 3<<11;
		else
			r |= 7<<11;
		cfgwritel(Fn1, ctl->chipcommon + Chipctldata, r);
		if(SBDEBUG) printf("-> %lux (= %lux)\n", r, cfgreadl(Fn1, ctl->chipcommon + Chipctldata));
	}
}

static void
sbenable(Ctlr *ctl)
{
	int i;

	if(SBDEBUG) printf("enabling HT clock...");
	cfgw(Clkcsr, 0);
	MicroDelay(1000000);
	cfgw(Clkcsr, ReqHT);
	for(i = 0; (cfgr(Clkcsr) & HTavail) == 0; i++){
		if(i == 50){
			printf("ether4330: can't enable HT clock: csr %x\n", cfgr(Clkcsr));
			// error(Eio);
			return ;
		}
		// tsleep(&up->sleep, return0, nil, 100);
		MicroDelay(100);
	}

	printf("HTCLOCK: 0x%x \n", cfgr(Clkcsr));

	cfgw(Clkcsr, cfgr(Clkcsr) | ForceHT);
	MicroDelay(1000 * 10);
	if(SBDEBUG) printf("chipclk: %x\n", cfgr(Clkcsr));
	sbwindow(ctl->sdregs);
	cfgwritel(Fn1, ctl->sdregs + Sbmboxdata, 4 << 16);	/* protocol version */
	cfgwritel(Fn1, ctl->sdregs + Intmask, FrameInt | MailboxInt | Fcchange);
	sdioset(Fn0, Ioenable, 1<<Fn2);
	for(i = 0; !(sdiord(Fn0, Ioready) & 1<<Fn2); i++){
		if(i == 10){
			printf("ERROR ether4330: can't enable SDIO function 2 - ioready %x\n", sdiord(Fn0, Ioready));
			// error(Eio);
			return;
		}
		// tsleep(&up->sleep, return0, nil, 100);
		MicroDelay(100);
	}
	sdiowr(Fn0, Intenable, (1<<Fn1) | (1<<Fn2) | 1);
}


/*
 * Firmware and config file uploading
 */

/*
 * Condense config file contents (in buffer buf with length n)
 * to 'var=value\0' list for firmware:
 *	- remove comments (starting with '#') and blank lines
 *	- remove carriage returns
 *	- convert newlines to nulls
 *	- mark end with two nulls
 *	- pad with nulls to multiple of 4 bytes total length
 */
static int
condense(uint8_t *buf, int n)
{
	uint8_t *p, *ep, *lp, *op;
	int c, skipping;

	skipping = 0;	/* true if in a comment */
	ep = buf + n;	/* end of input */
	op = buf;	/* end of output */
	lp = buf;	/* start of current output line */
	for(p = buf; p < ep; p++){
		switch(c = *p){
		case '#':
			skipping = 1;
			break;
		case '\0':
		case '\n':
			skipping = 0;
			if(op != lp){
				*op++ = '\0';
				lp = op;
			}
			break;
		case '\r':
			break;
		default:
			if(!skipping)
				*op++ = c;
			break;
		}
	}
	if(!skipping && op != lp)
		*op++ = '\0';
	*op++ = '\0';
	for(n = op - buf; n & 03; n++)
		*op++ = '\0';
	return n;
}

/*
 * Try to find firmware file in /boot or in /sys/lib/firmware.
 * Throw an error if not found.
 */
FileInfo *findfirmware(char *file)
{

	// I am supposed to write some code to locate file from file system here.
	// But for now I have loaded the wifi firmware files with kernel.
	// So if file names match I will return the pointer to corresponsing buffer.
	// It's big TODO here.: Replace with file APIs

	if(memcmp("brcmfmac43430-sdio.bin", file, 22) == 0) {
		printf("brcmfmac43430-sdio.bin Firmware file found. \n");
		bin_file.file_buf = (uint8_t *)&wifibinary_start;
		bin_file.current_buf = (uint8_t *)&wifibinary_start;
		bin_file.size = ((uint32_t)&wifibinary_end - (uint32_t)&wifibinary_start);
		return &bin_file;
	} else if(memcmp("brcmfmac43430-sdio.txt", file, 22) == 0) {
		printf("brcmfmac43430-sdio.txt Firmware file found. \n");
		text_file.file_buf = (uint8_t *)&wifitext_start;
		text_file.current_buf = (uint8_t *)&wifitext_start;
		text_file.size = ((uint32_t)&wifitext_end - (uint32_t)&wifitext_start);
		return &text_file;
	}
	printf("Could not locate any firmware file. File Name: %s \n", file);
	return NULL;
}

static uint32_t read_file_in_buffer(FileInfo *file_info, uint8_t *buffer, uint32_t size, uint32_t offset) {
	if(offset >= file_info->size) {
		return 0;
	}

	uint8_t *start_ptr = file_info->file_buf + offset;
	uint8_t *end_ptr = file_info->file_buf + file_info->size;
	uint32_t count = 0;
	while(1) {
		buffer[count] = *start_ptr;
		count++;
		start_ptr++;
		if(count == size) {
			// printf("Breaking due to size. \n");
			break;
		} else if (start_ptr == end_ptr) {
			printf("Breaking due to EOF. \n");
			break;
		}
	}
	// printf("Final Ptr: 0x%x offset: %d count: %d \n", start_ptr, offset, count);
	return count;
}

static int upload(Ctlr *ctl, char *file, int isconfig)
{
	FileInfo *file_info;
	uint8_t *buf = NULL;
	uint8_t *cbuf = NULL;
	uint32_t off, n;

	file_info = findfirmware(file);

	if(file_info == NULL) {
		printf(" Could not load the file. \n");
		return -1;
	}

	buf = mem_allocate(Uploadsz);

	if(buf == NULL) {
		printf("Could not allocate memory for buffer. \n");
		return -1;
	}

	if(Firmwarecmp){
		cbuf = mem_allocate(Uploadsz);
		if(buf == NULL) {
			printf("Could not allocate memory for compare buffer. \n");
			return -1;
		}
	}
	off = 0;
	for(;;){
		n = read_file_in_buffer(file_info, buf, Uploadsz, off);
		if(n <= 0) {
			printf("EOF reached. \n");
			break;
		}
			
		if(isconfig){
			n = condense(buf, n);
			off = ctl->socramsize - n - 4;
		} else if(off == 0) {
			memmove(ctl->resetvec.c, buf, sizeof(ctl->resetvec.c));
		}

		while(n&3) {
			buf[n++] = 0;
		}
		// printf("Transferring bytes: %d \n", n);
		sbmem(1, buf, n, ctl->rambase + off);
		if(isconfig) {
			// printf("ITs config file breaking. \n");
			break;
		}
		off += n;
	}

	printf("Completed transfer now off to compare.\n");
	if(Firmwarecmp) {
		printf("compare... \n");
		if(!isconfig) {
			off = 0;
		}
		for(;;) {
			if(!isconfig){
				n = read_file_in_buffer(file_info, buf, Uploadsz, off);
				if(n <= 0) {
					break;
				}
				while(n&3) {
					buf[n++] = 0;
				}
			}
			// printf("Reading data for comparison offset: %d \n", off);
			sbmem(0, cbuf, n, ctl->rambase + off);
			if(memcmp(buf, cbuf, n) != 0){
				printf(" Error ether4330: firmware load failed offset %d\n", off);
				return -1;
			}
			if(isconfig) {
				// printf("ITs config file breaking comparison. \n");
				break;
			}
			off += n;
		}
		printf("Successful comparison. \n");
	}
	printf("\n");
	mem_deallocate(buf);
	mem_deallocate(cbuf);
	return n;
}


static void
fwload(Ctlr *ctl)
{
	uint8_t buf[4];
	uint32_t i, n;

	i = 0;
	while(firmware[i].chipid != ctl->chipid ||
		   firmware[i].chiprev != ctl->chiprev){
		if(++i == nelem(firmware)){
			printf("ether4330: no firmware for chipid %x (%d) chiprev %d\n",
				ctl->chipid, ctl->chipid, ctl->chiprev);
			printf("no firmware");
		}
	}
	ctl->regufile = firmware[i].regufile;
	cfgw(Clkcsr, ReqALP);
	while((cfgr(Clkcsr) & ALPavail) == 0)
		MicroDelay(10);
	memset(buf, 0, 4);
	sbmem(1, buf, 4, ctl->rambase + ctl->socramsize - 4);
	if(FWDEBUG) printf("firmware load...");
	upload(ctl, firmware[i].fwfile, 0);
	if(FWDEBUG) printf("config load...");
	n = upload(ctl, firmware[i].cfgfile, 1);
	n /= 4;
	n = (n & 0xFFFF) | (~n << 16);
	put4(buf, n);
	sbmem(1, buf, 4, ctl->rambase + ctl->socramsize - 4);
	if(ctl->armcore == ARMcr4){
		sbwindow(ctl->sdregs);
		cfgwritel(Fn1, ctl->sdregs + Intstatus, ~0);
		if(ctl->resetvec.i != 0){
			if(SBDEBUG) printf("%ux\n", ctl->resetvec.i);
			sbmem(1, ctl->resetvec.c, sizeof(ctl->resetvec.c), 0);
		}
		sbreset(ctl->armctl, Cr4Cpuhalt, 0);
	}else
		sbreset(ctl->armctl, 0, 0);
}


void etherbcmattach()
{
	Ctlr *ctlr1 = (Ctlr *)&ctrl;

		if(ctlr1->chipid == 0){
			sdioinit();
			sbinit(ctlr1);
		}
		fwload(ctlr1);
		sbenable(ctlr1);
		
}
