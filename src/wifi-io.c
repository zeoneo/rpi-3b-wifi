    #include<device/wifi-io.h>
#include<device/emmc-sdio.h>
#include <kernel/systimer.h>
#include <plibc/stdio.h>
#include <string.h>
#include <mem/kernel_alloc.h>

#define USED(x) if(x);else{}

static uint32_t cmd_resp [4] = {0};
static uint32_t ocr = 0;
static uint32_t rca = 0;
static uint32_t chipid;
static uint32_t chiprev;
// static char	*regufile;
// static union {
// 		uint32_t i;
// 		uint8_t c[4];
// } resetvec;
static uint32_t armcore;
static uint32_t chipcommon;
static uint32_t armctl;
static uint32_t armregs;
static uint32_t d11ctl;
static uint32_t socramregs;
static uint32_t socramctl;
static uint32_t sdregs;
static uint32_t sdiorev;
static uint32_t socramrev;
static uint32_t socramsize;
static uint32_t rambase;
// static uint16_t reqid;
// static uint8_t fcmask;
// static uint8_t txwindow;
// static uint8_t txseq;
// static uint8_t	rxseq;

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
// uint32_t binarysize = (uint32_t)&wifibinary_end - (uint32_t)&wifibinary_start;



enum {
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

    Fn0	    = 0,
	Fn1 	= 1,
	Fn2	    = 2,
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

static int sdiord(uint32_t fn, uint32_t addr);
static void sdiowr(int fn, int addr, int data);
static void sdioset(uint32_t fn, uint32_t addr, uint32_t bits);
static bool prepare_sdio();

#define HOWMANY(x, y)	(((x)+((y)-1))/(y))
#define ROUNDUP(x, y)	(HOWMANY((x), (y))*(y))	/* ceiling */
#define ROUNDDN(x, y)	(((x)/(y))*(y))		/* floor */
#define	ROUND(s, sz)	(((s)+(sz-1))&~(sz-1))


// static uint8_t * put2(uint8_t *p, uint16_t v)
// {
// 	p[0] = v;
// 	p[1] = v >> 8;
// 	return p + 2;
// }

static uint8_t*put4(uint8_t *p, uint32_t v)
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


/*
 * Chip register and memory access via SDIO
 */

static void cfgw(uint32_t off, int val)
{
	sdiowr(Fn1, off, val);
}

static uint32_t cfgr(uint32_t off)
{
	return sdiord(Fn1, off);
}

static uint32_t
sdiocmd_locked(uint32_t cmd_idx, uint32_t arg)
{
	uint32_t resp[4];
	sdio_send_command(cmd_idx, arg, &resp[0]);
	return resp[0];
}


static void sdiorwext(uint32_t fn, uint32_t write, void *a, uint32_t len, uint32_t addr, uint32_t incr)
{
	uint32_t bsize, blk, bcount, effective_length;

	bsize = fn == Fn2 ? 512 : 64;
    printf("\n<----------Data Trasnsfer Started: Total len: %d \n", len);
	while(len > 0){
		if(len >= 511 * bsize){
			blk = 1;
			bcount = 511;
			effective_length = bcount * bsize;
		}else if(len > bsize){
			blk = 1;
			bcount = len/bsize;
			effective_length = bcount * bsize;
		}else{
			blk = 0;
			bcount = len;
			effective_length = len;
		}

        
		if(blk) {
            printf("\t SDIO IO OP: bsize: %d bcount: %d\n", bsize, bcount);
            sdio_iosetup(write, a, bsize, bcount);
            
        } else {
            printf("\t SDIO IO OP: bsize: %d bcount: %d\n", bcount, 1);
            sdio_iosetup(write, a, bcount, 1);
        }

		sdiocmd_locked(IX_IO_RW_DIRECT_EXTENDED, write<<31 | (fn&7)<<28 | blk<<27 | incr<<26 | (addr&0x1FFFF)<<9 | (bcount&0x1FF));

		sdio_do_io(write, a, effective_length);
        

		len -= effective_length;
		a = (char*)a + effective_length;
		if(incr) {
            addr += effective_length;
        }
	}
    printf("\n<----------Data Trasnsfer Ended - Length remaining: %d \n\n", len);
    
}



#define CACHELINESZ 64	/* temp */
uint32_t cfgreadl(int fn, uint32_t off)
{
	uint8_t cbuf[2*CACHELINESZ];
	uint8_t *p;

	p = (uint8_t *) ROUND((uint32_t) &cbuf[0], CACHELINESZ);
	memset(p, 0, 4);
	sdiorwext(fn, 0, p, 4, off|Sb32bit, 1);
	
    printf("cfgreadl %lx: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);

	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
}


static void
cfgwritel(uint32_t fn, uint32_t off, uint32_t data)
{
	uint8_t cbuf[2*CACHELINESZ];
	uint8_t *p;
	// int retry;

	p = (uint8_t*)ROUND((uint32_t) &cbuf[0], CACHELINESZ);
	put4(p, data);
	printf("cfgwritel 0x%x: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
	// retry = 0;
	// while(waserror()){
	// 	printf("ether4330: cfgwritel retry %lux %ux\n", off, data);
	// 	sdioabort(fn);
	// 	if(++retry == 3)
	// 		nexterror();
	// }
	sdiorwext(fn, 1, p, 4, off| Sb32bit , 1);
    printf("sdiorwext after \n");
	// poperror();
}


static void sbwindow(uint32_t addr)
{
	addr &= ~(Sbwsize-1);
	cfgw(Sbaddr, addr>>8);
	cfgw(Sbaddr+1, addr>>16);
	cfgw(Sbaddr+2, addr>>24);
}

static void sbrw(uint32_t fn, uint32_t write, uint8_t *buf, uint32_t len, uint32_t off)
{
	int n;
	USED(fn);
    printf("--length: %d \n", len);
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
	} else{
		// if(len >= 4){
		// 	n = len;
		// 	n &= ~3;
        //     printf("--sub length: %d \n", n);
		// 	sdiorwext(Fn1, write, buf, n, off|Sb32bit, 1);
		// 	off += n;
		// 	buf += n;
		// 	len -= n;
        //     printf("-- length after: %d \n", len);
		// }
        printf("-- length after2: %d \n", len);
		while(len > 0){
			*buf = sdiord(Fn1, off|Sb32bit);
			off++;
			buf++;
			len--;
		}
	}
}


static void sbmem(uint32_t write, uint32_t *buf, uint32_t len, uint32_t off)
{
	uint32_t n;

	n = ROUNDUP(off, Sbwsize) - off;
	if(n == 0) {
        n = Sbwsize;
    }
	printf("sbmem length: %d \n ", n);
	while(len > 0){
		if(n > len) {
            n = len;
        }
		sbwindow(off);
		sbrw(Fn1, write, (void *)buf, n, off & (Sbwsize-1));
		off += n;
		buf += n;
		len -= n;
		n = Sbwsize;
	}
}


#define nil (void *)0
static void corescan(uint32_t r)
{
	uint8_t *buf;
	uint32_t i, coreid, corerev;
	uint32_t addr;

	buf = mem_allocate(Corescansz);
	if(buf == nil) {
        printf(" Could not allocate memory of size : %d \n", Corescansz);
        return;
    }
		
	sbmem(0, (void *)buf, Corescansz, r);
	coreid = 0;
	corerev = 0;
	for(i = 0; i < Corescansz; i += 4){
		switch(buf[i] & 0xF){
		case 0xF:	/* end */
            printf(" Switch 0xf \n");
			mem_deallocate(buf);
			return;
		case 0x1:	/* core info */
			if((buf[i+4]&0xF) != 0x1) {
                break;
            }
			coreid = (buf[i+1] | buf[i+2]<<8) & 0xFFF;
			i += 4;
			corerev = buf[i+3];
			break;
		case 0x05:	/* address */
			addr = buf[i+1]<<8 | buf[i+2]<<16 | buf[i+3]<<24;
			addr &= ~0xFFF;
			printf("core %x %s 0x%x\n", coreid, buf[i]&0xC0? "ctl" : "mem", addr);
			switch(coreid){
			case 0x800:
				if((buf[i] & 0xC0) == 0) {
                    printf(" Switch chipcommon \n");
                    chipcommon = addr;
                }
				break;
			case ARMcm3:
			case ARM7tdmi:
			case ARMcr4:
				armcore = coreid;
				if(buf[i] & 0xC0){
					if(armctl == 0) {
                        armctl = addr;
                         printf(" Switch armctl \n");
                    }
				} else{
					if(armregs == 0) {
                        armregs = addr;
                         printf(" Switch armregs \n");
                    }
				}
				break;
			case 0x80E:
				if(buf[i] & 0xC0) {
                    socramctl = addr;
                } else if(socramregs == 0) {
                    socramregs = addr;
                    printf(" Switch socramregs \n");
                }
				socramrev = corerev;
				break;
			case 0x829:
				if((buf[i] & 0xC0) == 0) {
                    sdregs = addr;
                        printf(" Switch sdregs \n");
                }
				sdiorev = corerev;
				break;
			case 0x812:
				if(buf[i] & 0xC0) {
                    d11ctl = addr;
                     printf(" Switch d11ctl \n");
                }
				break;
			}
		}
	}
	mem_deallocate(buf);
}

static void
sbdisable(uint32_t regs, uint32_t pre, uint32_t ioctl)
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

	while((cfgreadl(Fn1, regs + Resetctrl) & 1) == 0);
	cfgwritel(Fn1, regs + Ioctrl, 3|ioctl);
	cfgreadl(Fn1, regs + Ioctrl);
}


static void
sbreset(uint32_t regs, uint32_t pre, uint32_t ioctl)
{
	sbdisable(regs, pre, ioctl);
	sbwindow(regs);
	
    printf("sbreset %#p %#lux %#lux ->", regs, cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));
	
    while((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0){
		cfgwritel(Fn1, regs + Resetctrl, 0);
		MicroDelay(40);
	}
	cfgwritel(Fn1, regs + Ioctrl, 1|ioctl);
	cfgreadl(Fn1, regs + Ioctrl);
	printf("%#lux %#lux\n", cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));
}

static void
ramscan()
{
	uint32_t r, n, size;
	uint32_t banks, i;

	if(armcore == ARMcr4){
		r = armregs;
		sbwindow(r);
		n = cfgreadl(Fn1, r + Cr4Cap);
		printf("cr4 banks %lux\n", n);
		banks = ((n>>4) & 0xF) + (n & 0xF);
		size = 0;
		for(i = 0; i < banks; i++){
			cfgwritel(Fn1, r + Cr4Bankidx, i);
			n = cfgreadl(Fn1, r + Cr4Bankinfo);
			printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
			size += 8192 * ((n & 0x3F) + 1);
		}
		socramsize = size;
		rambase = 0x198000;
		return;
	}
	if(socramrev <= 7 || socramrev == 12){
		printf("ether4330: SOCRAM rev %d not supported\n", socramrev);
        return;
	}
	sbreset(socramctl, 0, 0);
	r = socramregs;
	sbwindow(r);
	n = cfgreadl(Fn1, r + Coreinfo);
	printf("socramrev %d coreinfo %lux\n", socramrev, n);
	banks = (n>>4) & 0xF;
	size = 0;
	for(i = 0; i < banks; i++){
		cfgwritel(Fn1, r + Bankidx, i);
		n = cfgreadl(Fn1, r + Bankinfo);
		if(SBDEBUG) printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
		size += 8192 * ((n & 0x3F) + 1);
	}
	socramsize = size;
	rambase = 0;
	if(chipid == 43430){
		cfgwritel(Fn1, r + Bankidx, 3);
		cfgwritel(Fn1, r + Bankpda, 0);
	}
}


static bool sb_init() {
    sbwindow(Enumbase);

	uint32_t r;

	// int8_t buf[16];
    r = cfgreadl(Fn1, Enumbase);
	chipid = r & 0xFFFF;
	printf("Chip Id %x %d \n", chipid, chipid);

    switch(chipid){
		case 0x4330:
		case 43362:
		case 43430:
		case 0x4345:
			chiprev = (r>>16)&0xF;
			break;
		default:
			printf("ether4330: chipid %#x (%d) not supported\n", chipid, chipid);
            return false;
	}
	r = cfgreadl(Fn1, Enumbase + 63*4);
    corescan(r);

    if(armctl == 0 || d11ctl == 0 || (armcore == ARMcm3 && (socramctl == 0 || socramregs == 0))) {
        printf("corescan didn't find essential cores\n");
        return false;
    }   
		
	if(armcore == ARMcr4) {
        printf("Reseting sonic backplane \n");
        sbreset(armctl, Cr4Cpuhalt, Cr4Cpuhalt);
    } else {
        printf("Disabled sonic backplane \n");
        sbdisable(armctl, 0, 0);
    }

    // sbreset(d11ctl, 8|4, 4);
	// ramscan();
	// printf("ARM %#p D11 %#p SOCRAM %#p,%#p %lud bytes @ %#p\n", armctl, d11ctl, socramctl, socramregs, socramsize, rambase);
	// cfgw(Clkcsr, 0);
    // MicroDelay(10);
	
    // printf("chipclk: %x\n", cfgr(Clkcsr));
    // cfgw(Clkcsr, Nohwreq | ReqALP);

	// while((cfgr(Clkcsr) & (HTavail|ALPavail)) == 0) {
    //     MicroDelay(10);
    // }
		
	// cfgw(Clkcsr, Nohwreq | ForceALP);
	// MicroDelay(65);
    // printf("chipclk: %x\n", cfgr(Clkcsr));
	// cfgw(Pullups, 0);
	// sbwindow(chipcommon);
	// cfgwritel(Fn1, chipcommon + Gpiopullup, 0);
	// cfgwritel(Fn1, chipcommon + Gpiopulldown, 0);
	// if(chipid != 0x4330 && chipid != 43362) {
    //     return true;
    // }
	// cfgwritel(Fn1, chipcommon + Chipctladdr, 1);
	// if(cfgreadl(Fn1, chipcommon + Chipctladdr) != 1) {
    //     printf("ether4330: can't set Chipctladdr\n");
    // } else {
	// 	r = cfgreadl(Fn1, chipcommon + Chipctldata);
	// 	if(SBDEBUG) printf("chipcommon PMU (%lux) %lux", cfgreadl(Fn1, chipcommon + Chipctladdr), r);
	// 	/* set SDIO drive strength >= 6mA */
	// 	r &= ~0x3800;
	// 	if(chipid == 0x4330) {
    //         r |= 3<<11;
    //     } else {
    //         r |= 7<<11;
    //     }
	// 	cfgwritel(Fn1, chipcommon + Chipctldata, r);
	// 	printf("-> %lux (= %lux)\n", r, cfgreadl(Fn1, chipcommon + Chipctldata));
	// }

    //

    return true;
}

bool startWifi () {
    // uint32_t binarysize = (uint32_t)&brcmfmac43430_sdio_bin_end - (uint32_t)&brcmfmac43430_sdio_bin_start;
    uint32_t binarysize = (uint32_t)&wifibinary_end - (uint32_t)&wifibinary_start;
    printf("Binary Size: %d \n", binarysize);
    bool is_success = prepare_sdio();
    is_success = sb_init();

    return is_success;
}

static int sdiord(uint32_t fn, uint32_t addr)
{
	bool is_success = sdio_send_command(IX_IO_RW_DIRECT, (0<<31)|((fn&7)<<28) | ((addr&0x1FFFF)<<9), &cmd_resp[0]);
    if(!is_success) {
        printf("Could not complete IO_RW_DIRECT: fn :%d addr: 0x%x \n", fn, addr);
        return 0;
    }
	if(cmd_resp[0] & 0xCF00){
		printf("ERROR: ether4330: sdiord(%x, %x) fail: %2.2x %2.2x\n", fn, addr, (cmd_resp[0]>>8)&0xFF, cmd_resp[0]&0xFF);
	}
	return cmd_resp[0] & 0xFF;
}

static void sdiowr(int fn, int addr, int data)
{
	uint32_t r;
	int retry;

	r = 0;
	for(retry = 0; retry < 10; retry++){
		sdio_send_command(IX_IO_RW_DIRECT, (1<<31)|((fn&7)<<28)|((addr&0x1FFFF)<<9)|(data&0xFF), &cmd_resp[0]);
        r = cmd_resp[0];
		if((r & 0xCF00) == 0) {
            return;
        }
	}
	printf("ERROR: ether4330: sdiowr(%x, %x, %x) fail: %2.2ux %2.2ux\n", fn, addr, data, (r>>8)&0xFF, r&0xFF);
}


static void sdioset(uint32_t fn, uint32_t addr, uint32_t bits)
{
	sdiowr(fn, addr, sdiord(fn, addr) | bits);
}

static bool prepare_sdio() {
    bool is_success = initSDIO();
    is_success = sdio_send_command(IX_GO_IDLE_STATE, 0, &cmd_resp[0]);
    if(is_success) {
        printf(" Sent go idle command \n");
    }

    is_success = sdio_send_command(IX_IO_SEND_OP_COND, 0 ,  &cmd_resp[0]);
    ocr = cmd_resp[0];
    if(is_success) {
        printf("Found card OCR : 0x%x\n", ocr);
        uint32_t i = 0;
        while((ocr & (1<<31)) == 0) {
            if(++i > 5){
                printf("ether4330: no response to sdio access: ocr = %x\n", ocr);
                return false;
            }
            // send op condition 3V_3
            is_success = sdio_send_command(IX_IO_SEND_OP_COND, 3<<20 ,  &cmd_resp[0]);
            ocr = cmd_resp[0];
            MicroDelay(100);
	    }
    } else {
        printf("Failed to get ocr. \n");
        return false;
    }
    printf("NEW OCR : 0x%x\n", ocr);
    is_success = sdio_send_command(IX_SEND_RELATIVE_ADDR, 0, &cmd_resp[0]);
    if(is_success) {
        rca = (cmd_resp[0] >> 16);
        printf("rca: 0x%x \n",rca);
        is_success = sdio_send_command(IX_SELECT_CARD, rca << 16, &cmd_resp[0]);
        if(!is_success) {
            printf("Could not select card \n");
            return false;
        }
        
    }

	sdioset(Fn0, Highspeed, 2);
	sdioset(Fn0, Busifc, 2);	/* bus width 4 */
	sdiowr(Fn0, Fbr1+Blksize, 64);
	sdiowr(Fn0, Fbr1+Blksize+1, 64>>8);
	sdiowr(Fn0, Fbr2+Blksize, 512);
	sdiowr(Fn0, Fbr2+Blksize+1, 512>>8);
	sdioset(Fn0, Ioenable, 1<<Fn1);
	sdiowr(Fn0, Intenable, 0);

    uint32_t i = 0;
    for(i = 0; !(sdiord(Fn0, Ioready) & 1<<Fn1); i++){
		if(i == 10){
			printf("ether4330: can't enable SDIO function\n");
			return false;
		}
		MicroDelay(100);
	}

    printf(" +++++Enabled SDIO IO functionality \n");
    return is_success;
}