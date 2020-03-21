#include<device/wifi-io.h>
#include<device/emmc-sdio.h>
#include <kernel/systimer.h>
#include <plibc/stdio.h>
#include <string.h>


static uint32_t cmd_resp [4] = {0};
static uint32_t ocr = 0;
static uint32_t rca = 0;

enum {
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

/*
 * Chip register and memory access via SDIO
 */

static void cfgw(uint32_t off, int val)
{
	sdiowr(Fn1, off, val);
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
	uint32_t bsize, blk, bcount, m;

	bsize = fn == Fn2 ? 512 : 64;
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

		if(blk) {
            sdio_iosetup(write, a, bsize, bcount);
        } else {
            sdio_iosetup(write, a, bcount, 1);
        }

		sdiocmd_locked(IX_IO_RW_DIRECT_EXTENDED, write<<31 | (fn&7)<<28 | blk<<27 | incr<<26 | (addr&0x1FFFF)<<9 | (bcount&0x1FF));

		sdio_do_io(write, a, m);

		len -= m;
		a = (char*)a + m;
		if(incr) {
            addr += m;
        }
	}
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

static void sbwindow(uint32_t addr)
{
	addr &= ~(Sbwsize-1);
	cfgw(Sbaddr, addr>>8);
	cfgw(Sbaddr+1, addr>>16);
	cfgw(Sbaddr+2, addr>>24);
}

static bool prepare_side_plane() {
    sbwindow(Enumbase);

	uint32_t r;
	uint32_t chipid;
	// int8_t buf[16];
    r = cfgreadl(Fn1, Enumbase);
	chipid = r & 0xFFFF;
	printf("Chip Id %x %d \n", chipid, chipid);

    return true;
}

bool startWifi () {
    bool is_success = prepare_sdio();
    is_success = prepare_side_plane();
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