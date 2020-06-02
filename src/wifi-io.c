#include <device/gpio.h>
#include <device/plan9_emmc.h>
#include <device/wifi-io.h>
#include <kernel/systimer.h>
#include <mem/kernel_alloc.h>
#include <plibc/stdio.h>
#include <plibc/string.h>

#define USED(x)                                                                                                        \
    if (x)                                                                                                             \
        ;                                                                                                              \
    else {                                                                                                             \
    }
#define nelem(x) (sizeof(x) / sizeof((x)[0]))

static uint32_t cmd_resp[4] = {0};
typedef struct {
    uint32_t ocr;
    uint32_t rca;
    uint32_t chipid;
    uint32_t chiprev;
    char* regufile;
    union {
        uint32_t i;
        uint8_t c[4];
    } resetvec;
    uint32_t armcore;
    uint32_t chipcommon;
    uint32_t armctl;
    uint32_t armregs;
    uint32_t d11ctl;
    uint32_t socramregs;
    uint32_t socramctl;
    uint32_t sdregs;
    uint32_t sdiorev;
    uint32_t socramrev;
    uint32_t socramsize;
    uint32_t rambase;
} Ctrl;

static Ctrl ctrl = {0};
// 	uint16_t reqid;
// static uint8_t fcmask;
// static uint8_t txwindow;
// static uint8_t txseq;
// static uint8_t	rxseq;

#if !defined(__STRINGIFY2)
#define __STRINGIFY2(__p) #__p
#define __STRINGIFY(__p)  __STRINGIFY2(__p)
#endif

#define INCLUDE_BINARY_FILE(__variable, __fileName, __section)                                                         \
    __asm__(".pushsection " __section "\n\t"                                                                           \
            ".globl " __STRINGIFY(__variable) "_start;"                                                                \
                                              "\n\t"                                                                   \
                                              ".balign 4"                                                              \
                                              "\n\t" __STRINGIFY(                                                      \
                                                  __variable) "_start: .incbin \"" __fileName "\""                     \
                                                              "\n\t"                                                   \
                                                              ".globl " __STRINGIFY(                                   \
                                                                  __variable) "_end;"                                  \
                                                                              "\n\t" __STRINGIFY(                      \
                                                                                  __variable) "_end: .4byte 0;"        \
                                                                                              "\n\t"                   \
                                                                                              ".balign 4"              \
                                                                                              "\n\t"                   \
                                                                                              ".popsection"            \
                                                                                              "\n\t");                 \
    extern const uint8_t __variable##_start;                                                                           \
    extern const uint8_t __variable##_end;

INCLUDE_BINARY_FILE(wifibinary, "lib/brcmfmac43430-sdio.bin", ".rodata.brcmfmac43430_sdio_bin");
INCLUDE_BINARY_FILE(wifitext, "lib/brcmfmac43430-sdio.txt", ".rodata.brcmfmac43430_sdio_txt");

// uint32_t binarysize = (uint32_t)&wifibinary_end - (uint32_t)&wifibinary_start;

enum {
    SDIODEBUG  = 0,
    SBDEBUG    = 0,
    EVENTDEBUG = 0,
    VARDEBUG   = 0,
    FWDEBUG    = 0,

    Corescansz = 512,
    Uploadsz   = 2048,

    Wifichan    = 0, /* default channel */
    Firmwarecmp = 1,

    ARMcm3   = 0x82A,
    ARM7tdmi = 0x825,
    ARMcr4   = 0x83E,

    Fn0  = 0,
    Fn1  = 1,
    Fn2  = 2,
    Fbr1 = 0x100,
    Fbr2 = 0x200,

    /* CCCR */
    Ioenable   = 0x02,
    Ioready    = 0x03,
    Intenable  = 0x04,
    Intpend    = 0x05,
    Ioabort    = 0x06,
    Busifc     = 0x07,
    Capability = 0x08,
    Blksize    = 0x10,
    Highspeed  = 0x13,

    /* Sonics Silicon Backplane (access to cores on chip) */
    Sbwsize  = 0x8000,
    Sb32bit  = 0x8000,
    Sbaddr   = 0x1000a,
    Enumbase = 0x18000000,
    Framectl = 0x1000d,
    Rfhalt   = 0x01,
    Wfhalt   = 0x02,
    Clkcsr   = 0x1000e,
    ForceALP = 0x01, /* active low-power clock */
    ForceHT  = 0x02, /* high throughput clock */
    ForceILP = 0x04, /* idle low-power clock */
    ReqALP   = 0x08,
    ReqHT    = 0x10,
    Nohwreq  = 0x20,
    ALPavail = 0x40,
    HTavail  = 0x80,
    Pullups  = 0x1000f,
    Wfrmcnt  = 0x10019,
    Rfrmcnt  = 0x1001b,

    /* core control regs */
    Ioctrl    = 0x408,
    Resetctrl = 0x800,

    /* socram regs */
    Coreinfo = 0x00,
    Bankidx  = 0x10,
    Bankinfo = 0x40,
    Bankpda  = 0x44,

    /* armcr4 regs */
    Cr4Cap      = 0x04,
    Cr4Bankidx  = 0x40,
    Cr4Bankinfo = 0x44,
    Cr4Cpuhalt  = 0x20,

    /* chipcommon regs */
    Gpiopullup   = 0x58,
    Gpiopulldown = 0x5c,
    Chipctladdr  = 0x650,
    Chipctldata  = 0x654,

    /* sdio core regs */
    Intstatus  = 0x20,
    Fcstate    = 1 << 4,
    Fcchange   = 1 << 5,
    FrameInt   = 1 << 6,
    MailboxInt = 1 << 7,

    Intmask      = 0x24,
    Sbmbox       = 0x40,
    Sbmboxdata   = 0x48,
    Hostmboxdata = 0x4c,
    Fwready      = 0x80,

    /* wifi control commands */
    GetVar = 262,
    SetVar = 263,

    /* status */
    Disconnected = 0,
    Connecting,
    Connected,
};

static int sdiord(uint32_t fn, uint32_t addr);
static void sdiowr(int fn, int addr, int data);
static void sdioset(uint32_t fn, uint32_t addr, uint32_t bits);
static bool sdioinit();
static void sbreset(uint32_t regs, uint32_t pre, uint32_t ioctl);

#define HOWMANY(x, y) (((x) + ((y) -1)) / (y))
#define ROUNDUP(x, y) (HOWMANY((x), (y)) * (y)) /* ceiling */
#define ROUNDDN(x, y) (((x) / (y)) * (y))       /* floor */
#define ROUND(s, sz)  (((s) + (sz - 1)) & ~(sz - 1))

struct Cmd {
    uint8_t cmd[4];
    uint8_t len[4];
    uint8_t flags[2];
    uint8_t id[2];
    uint8_t status[4];
};

typedef struct {
    uint8_t* file_buf;
    uint8_t* current_buf;
    uint32_t size;
} FileInfo;

FileInfo bin_file  = {0};
FileInfo text_file = {0};

static char config40181[] = "bcmdhd.cal.40181";
static char config40183[] = "bcmdhd.cal.40183.26MHz";

struct {
    uint32_t chipid;
    uint32_t chiprev;
    char* fwfile;
    char* cfgfile;
    char* regufile;
} firmware[] = {
    {0x4330, 3, "fw_bcm40183b1.bin", config40183, 0},
    {0x4330, 4, "fw_bcm40183b2.bin", config40183, 0},
    {43362, 0, "fw_bcm40181a0.bin", config40181, 0},
    {43362, 1, "fw_bcm40181a2.bin", config40181, 0},
    {43430, 1, "brcmfmac43430-sdio.bin", "brcmfmac43430-sdio.txt", 0},
    {0x4345, 6, "brcmfmac43455-sdio.bin", "brcmfmac43455-sdio.txt", "brcmfmac43455-sdio.clm_blob"},
};

// static uint8_t * put2(uint8_t *p, uint16_t v)
// {
// 	p[0] = v;
// 	p[1] = v >> 8;
// 	return p + 2;
// }

static uint8_t* put4(uint8_t* p, uint32_t v) {
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

static void cfgw(uint32_t off, int val) {
    sdiowr(Fn1, off, val);
}

static uint32_t cfgr(uint32_t off) {
    return sdiord(Fn1, off);
}

static uint32_t sdiocmd_locked(uint32_t cmd_idx, uint32_t arg) {
    uint32_t resp[4];
    emmccmd(cmd_idx, arg, &resp[0]);
    return resp[0];
}

static void sdiorwext(uint32_t fn, uint32_t write, void* a, uint32_t len, uint32_t addr, uint32_t incr) {
    uint32_t bsize, blk, bcount, effective_length;

    bsize = fn == Fn2 ? 512 : 64;
    // printf("\n<----------Data Trasnsfer Started: Total len: %d \n", len);
    while (len > 0) {
        if (len >= 511 * bsize) {
            blk              = 1;
            bcount           = 511;
            effective_length = bcount * bsize;
        } else if (len > bsize) {
            blk              = 1;
            bcount           = len / bsize;
            effective_length = bcount * bsize;
        } else {
            blk              = 0;
            bcount           = len;
            effective_length = len;
        }

        if (blk) {
            // printf("\t SDIO IO OP: bsize: %d bcount: %d\n", bsize, bcount);
            emmciosetup(write, a, bsize, bcount);

        } else {
            // printf("\t SDIO IO OP: bsize: %d bcount: %d\n", bcount, 1);
            emmciosetup(write, a, bcount, 1);
        }

        sdiocmd_locked(53, write << 31 | (fn & 7) << 28 | blk << 27 | incr << 26 | (addr & 0x1FFFF) << 9 |
                               (bcount & 0x1FF));

        emmcio(write, a, effective_length);

        len -= effective_length;
        a = (char*) a + effective_length;
        if (incr) {
            addr += effective_length;
        }
    }
    // printf("\n<----------Data Trasnsfer Ended - Length remaining: %d \n\n", len);
}

#define CACHELINESZ 64 /* temp */
uint32_t cfgreadl(int fn, uint32_t off) {
    uint8_t cbuf[2 * CACHELINESZ];
    uint8_t* p;

    p = (uint8_t*) ROUND((uint32_t) &cbuf[0], CACHELINESZ); // TODO check if it's failing because of this.
    memset(p, 0, 4);
    sdiorwext(fn, 0, p, 4, off | Sb32bit, 1);

    // printf("cfgreadl %lx: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);

    return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
}

static void cfgwritel(uint32_t fn, uint32_t off, uint32_t data) {
    uint8_t cbuf[2 * CACHELINESZ];
    uint8_t* p;
    // int retry;

    p = (uint8_t*) ROUND((uint32_t) &cbuf[0], CACHELINESZ);
    put4(p, data);
    // printf("cfgwritel 0x%x: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
    sdiorwext(fn, 1, p, 4, off | Sb32bit, 1);
}

static void sbwindow(uint32_t addr) {
    addr &= ~(Sbwsize - 1);
    cfgw(Sbaddr, addr >> 8);
    cfgw(Sbaddr + 1, addr >> 16);
    cfgw(Sbaddr + 2, addr >> 24);
}

static void sbrw(uint32_t fn, uint32_t write, uint8_t* buf, uint32_t len, uint32_t off) {
    int n;
    USED(fn);
    if (write) {
        if (len >= 4) {
            n = len;
            n &= ~3;

            sdiorwext(Fn1, write, buf, n, off | Sb32bit, 1);
            off += n;
            buf += n;
            len -= n;
        }
        while (len > 0) {
            sdiowr(Fn1, off | Sb32bit, *buf);
            off++;
            buf++;
            len--;
        }
    } else {
        if (len >= 4) {
            n = len;
            n &= ~3;
            sdiorwext(Fn1, write, buf, n, off | Sb32bit, 1);
            off += n;
            buf += n;
            len -= n;
        }
        while (len > 0) {
            *buf = sdiord(Fn1, off | Sb32bit);
            off++;
            buf++;
            len--;
        }
    }
}

static void sbmem(uint32_t write, void* buf, uint32_t len, uint32_t off) {
    uint32_t n;

    n = ROUNDUP(off, Sbwsize) - off;
    if (n == 0) {
        n = Sbwsize;
    }
    while (len > 0) {
        if (n > len) {
            n = len;
        }
        sbwindow(off);
        sbrw(Fn1, write, buf, n, off & (Sbwsize - 1));
        off += n;
        buf += n;
        len -= n;
        n = Sbwsize;
    }
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
static int condense(uint8_t* buf, uint8_t n) {
    uint8_t *p, *ep, *lp, *op;
    uint32_t c, skipping;

    skipping = 0;       /* true if in a comment */
    ep       = buf + n; /* end of input */
    op       = buf;     /* end of output */
    lp       = buf;     /* start of current output line */
    for (p = buf; p < ep; p++) {
        switch (c = *p) {
        case '#':
            skipping = 1;
            break;
        case '\0':
        case '\n':
            skipping = 0;
            if (op != lp) {
                *op++ = '\0';
                lp    = op;
            }
            break;
        case '\r':
            break;
        default:
            if (!skipping) {
                *op++ = c;
            }
            break;
        }
    }
    if (!skipping && op != lp) {
        *op++ = '\0';
    }
    *op++ = '\0';
    for (n = op - buf; n & 03; n++) {
        *op++ = '\0';
    }
    return n;
}

/*
 * Try to find firmware file in /boot or in /sys/lib/firmware.
 * Throw an error if not found.
 */
FileInfo* findfirmware(char* file) {

    // I am supposed to write some code to locate file from file system here.
    // But for now I have loaded the wifi firmware files with kernel.
    // So if file names match I will return the pointer to corresponsing buffer.
    // It's big TODO here.: Replace with file APIs

    if (memcmp("brcmfmac43430-sdio.bin", file, 22) == 0) {
        printf("brcmfmac43430-sdio.bin Firmware file found. \n");
        bin_file.file_buf    = (uint8_t*) &wifibinary_start;
        bin_file.current_buf = (uint8_t*) &wifibinary_start;
        bin_file.size        = ((uint32_t) &wifibinary_end - (uint32_t) &wifibinary_start);
        return &bin_file;
    } else if (memcmp("brcmfmac43430-sdio.txt", file, 22) == 0) {
        printf("brcmfmac43430-sdio.txt Firmware file found. \n");
        text_file.file_buf    = (uint8_t*) &wifitext_start;
        text_file.current_buf = (uint8_t*) &wifitext_start;
        text_file.size        = ((uint32_t) &wifitext_end - (uint32_t) &wifitext_start);
        return &text_file;
    }
    printf("Could not locate any firmware file. File Name: %s \n", file);
    return NULL;
}

static uint32_t read_file_in_buffer(FileInfo* file_info, uint8_t* buffer, uint32_t size, uint32_t offset) {
    if (offset >= file_info->size) {
        return 0;
    }

    uint8_t* start_ptr = file_info->file_buf + offset;
    uint8_t* end_ptr   = file_info->file_buf + file_info->size;
    uint32_t count     = 0;
    while (1) {
        buffer[count] = *start_ptr;
        count++;
        start_ptr++;
        if (count == size) {
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

static int upload(char* file, int isconfig) {
    FileInfo* file_info;
    uint8_t* buf  = NULL;
    uint8_t* cbuf = NULL;
    uint32_t off, n;

    file_info = findfirmware(file);

    if (file_info == NULL) {
        printf(" Could not load the file. \n");
        return -1;
    }

    buf = mem_allocate(Uploadsz);

    if (buf == NULL) {
        printf("Could not allocate memory for buffer. \n");
        return -1;
    }

    if (Firmwarecmp) {
        cbuf = mem_allocate(Uploadsz);
        if (buf == NULL) {
            printf("Could not allocate memory for compare buffer. \n");
            return -1;
        }
    }
    off = 0;
    for (;;) {
        n = read_file_in_buffer(file_info, buf, Uploadsz, off);
        if (n <= 0) {
            printf("EOF reached. \n");
            break;
        }

        if (isconfig) {
            n   = condense(buf, n);
            off = ctrl.socramsize - n - 4;
        } else if (off == 0) {
            memmove(ctrl.resetvec.c, buf, sizeof(ctrl.resetvec.c));
        }

        while (n & 3) {
            buf[n++] = 0;
        }
        // printf("Transferring bytes: %d \n", n);
        sbmem(1, buf, n, ctrl.rambase + off);
        if (isconfig) {
            // printf("ITs config file breaking. \n");
            break;
        }
        off += n;
    }

    printf("Completed transfer now off to compare.\n");
    if (Firmwarecmp) {
        printf("compare... \n");
        if (!isconfig) {
            off = 0;
        }
        for (;;) {
            if (!isconfig) {
                n = read_file_in_buffer(file_info, buf, Uploadsz, off);
                if (n <= 0) {
                    break;
                }
                while (n & 3) {
                    buf[n++] = 0;
                }
            }
            // printf("Reading data for comparison offset: %d \n", off);
            sbmem(0, cbuf, n, ctrl.rambase + off);
            if (memcmp(buf, cbuf, n) != 0) {
                printf(" Error ether4330: firmware load failed offset %d\n", off);
                return -1;
            }
            if (isconfig) {
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

static void sbenable() {
    uint32_t i;

    printf("enabling HT clock...\n");
    cfgw(Clkcsr, 0);
    MicroDelay(100);
    cfgw(Clkcsr, ReqHT);
    MicroDelay(100);
    for (i = 0; (cfgr(Clkcsr) & HTavail) == 0; i++) {
        if (i == 100) {
            printf("Error: ether4330: can't enable HT clock: csr %x\n", cfgr(Clkcsr));
            break;
        }
        MicroDelay(100);
        // cfgw(Clkcsr, ReqHT);
        // MicroDelay(100);
    }
    cfgw(Clkcsr, cfgr(Clkcsr) | ForceHT);
    MicroDelay(10);
    printf("chipclk: %x is high clock enabled: %d \n", cfgr(Clkcsr), (cfgr(Clkcsr) & HTavail));
    sbwindow(ctrl.sdregs);
    cfgwritel(Fn1, ctrl.sdregs + Sbmboxdata, 4 << 16); /* protocol version */
    cfgwritel(Fn1, ctrl.sdregs + Intmask, FrameInt | MailboxInt | Fcchange);
    sdioset(Fn0, Ioenable, 1 << Fn2);
    for (i = 0; !(sdiord(Fn0, Ioready) & 1 << Fn2); i++) {
        if (i == 100) {
            printf("Error: ether4330: can't enable SDIO function 2 - ioready %x\n", sdiord(Fn0, Ioready));
            return;
        }
        MicroDelay(100);
    }
    sdiowr(Fn0, Intenable, (1 << Fn1) | (1 << Fn2) | 1);
}

static void fwload() {
    uint8_t buf[4];
    uint32_t i, n;

    i = 0;
    while (firmware[i].chipid != ctrl.chipid || firmware[i].chiprev != ctrl.chiprev) {
        if (++i == nelem(firmware)) {
            printf("ERROR ether4330: no firmware for chipid %x (%d) chiprev %d\n", ctrl.chipid, ctrl.chipid,
                   ctrl.chiprev);
            return;
        }
    }
    ctrl.regufile = firmware[i].regufile;
    cfgw(Clkcsr, ReqALP);
    while ((cfgr(Clkcsr) & ALPavail) == 0) {
        MicroDelay(10);
    }

    memset(buf, 0, 4);
    sbmem(1, buf, 4, ctrl.rambase + ctrl.socramsize - 4);

    printf("started firmware load...");
    upload(firmware[i].fwfile, 0);
    printf("config load...");
    n = upload(firmware[i].cfgfile, 1);
    n /= 4;
    n = (n & 0xFFFF) | (~n << 16);
    put4(buf, n);
    sbmem(1, buf, 4, ctrl.rambase + ctrl.socramsize - 4);
    if (ctrl.armcore == ARMcr4) {
        sbwindow(ctrl.sdregs);
        cfgwritel(Fn1, ctrl.sdregs + Intstatus, ~0);
        if (ctrl.resetvec.i != 0) {
            // printf("%ux\n", resetvec.i);
            sbmem(1, ctrl.resetvec.c, sizeof(ctrl.resetvec.c), 0);
        }
        sbreset(ctrl.armctl, Cr4Cpuhalt, 0);
    } else {
        sbreset(ctrl.armctl, 0, 0);
    }
}

#define nil (void*) 0
static void corescan(uint32_t r) {
    uint8_t* buf;
    uint32_t i, coreid, corerev;
    uint32_t addr;

    buf = mem_allocate(Corescansz);
    if (buf == nil) {
        printf(" Could not allocate memory of size : %d \n", Corescansz);
        return;
    }

    sbmem(0, (void*) buf, Corescansz, r);
    coreid  = 0;
    corerev = 0;
    for (i = 0; i < Corescansz; i += 4) {
        switch (buf[i] & 0xF) {
        case 0xF: /* end */
                  // printf(" Switch 0xf \n");
            mem_deallocate(buf);
            return;
        case 0x1: /* core info */
            if ((buf[i + 4] & 0xF) != 0x1) {
                break;
            }
            coreid = (buf[i + 1] | buf[i + 2] << 8) & 0xFFF;
            i += 4;
            corerev = buf[i + 3];
            break;
        case 0x05: /* address */
            addr = buf[i + 1] << 8 | buf[i + 2] << 16 | buf[i + 3] << 24;
            addr &= ~0xFFF;
            // printf("core %x %s 0x%x\n", coreid, buf[i]&0xC0? "ctl" : "mem", addr);
            switch (coreid) {
            case 0x800:
                if ((buf[i] & 0xC0) == 0) {
                    // printf(" Switch chipcommon \n");
                    ctrl.chipcommon = addr;
                }
                break;
            case ARMcm3:
            case ARM7tdmi:
            case ARMcr4:
                ctrl.armcore = coreid;
                if (buf[i] & 0xC0) {
                    if (ctrl.armctl == 0) {
                        ctrl.armctl = addr;
                        //  printf(" Switch armctl \n");
                    }
                } else {
                    if (ctrl.armregs == 0) {
                        ctrl.armregs = addr;
                        //  printf(" Switch armregs \n");
                    }
                }
                break;
            case 0x80E:
                if (buf[i] & 0xC0) {
                    ctrl.socramctl = addr;
                } else if (ctrl.socramregs == 0) {
                    ctrl.socramregs = addr;
                    // printf(" Switch socramregs \n");
                }
                ctrl.socramrev = corerev;
                break;
            case 0x829:
                if ((buf[i] & 0xC0) == 0) {
                    ctrl.sdregs = addr;
                    // printf(" Switch sdregs \n");
                }
                ctrl.sdiorev = corerev;
                break;
            case 0x812:
                if (buf[i] & 0xC0) {
                    ctrl.d11ctl = addr;
                    //  printf(" Switch d11ctl \n");
                }
                break;
            }
        }
    }
    mem_deallocate(buf);
}

static void sbdisable(uint32_t regs, uint32_t pre, uint32_t ioctl) {
    sbwindow(regs);
    if ((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0) {
        cfgwritel(Fn1, regs + Ioctrl, 3 | ioctl);
        cfgreadl(Fn1, regs + Ioctrl);
        return;
    }
    cfgwritel(Fn1, regs + Ioctrl, 3 | pre);
    cfgreadl(Fn1, regs + Ioctrl);
    cfgwritel(Fn1, regs + Resetctrl, 1);
    MicroDelay(10);

    while ((cfgreadl(Fn1, regs + Resetctrl) & 1) == 0)
        ;
    cfgwritel(Fn1, regs + Ioctrl, 3 | ioctl);
    cfgreadl(Fn1, regs + Ioctrl);
}

static void sbreset(uint32_t regs, uint32_t pre, uint32_t ioctl) {
    sbdisable(regs, pre, ioctl);
    sbwindow(regs);

    // printf("sbreset %#p %#lux %#lux ->", regs, cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));

    while ((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0) {
        cfgwritel(Fn1, regs + Resetctrl, 0);
        MicroDelay(40);
    }
    cfgwritel(Fn1, regs + Ioctrl, 1 | ioctl);
    cfgreadl(Fn1, regs + Ioctrl);
    // printf("%#lux %#lux\n", cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));
}

static void ramscan() {
    uint32_t r, n, size;
    uint32_t banks, i;

    if (ctrl.armcore == ARMcr4) {
        r = ctrl.armregs;
        sbwindow(r);
        n = cfgreadl(Fn1, r + Cr4Cap);
        printf("cr4 banks %lux\n", n);
        banks = ((n >> 4) & 0xF) + (n & 0xF);
        size  = 0;
        for (i = 0; i < banks; i++) {
            cfgwritel(Fn1, r + Cr4Bankidx, i);
            n = cfgreadl(Fn1, r + Cr4Bankinfo);
            printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
            size += 8192 * ((n & 0x3F) + 1);
        }
        ctrl.socramsize = size;
        ctrl.rambase    = 0x198000;
        return;
    }
    if (ctrl.socramrev <= 7 || ctrl.socramrev == 12) {
        printf("ether4330: SOCRAM rev %d not supported\n", ctrl.socramrev);
        return;
    }
    sbreset(ctrl.socramctl, 0, 0);
    r = ctrl.socramregs;
    sbwindow(r);
    n = cfgreadl(Fn1, r + Coreinfo);
    printf("socramrev %d coreinfo %lux\n", ctrl.socramrev, n);
    banks = (n >> 4) & 0xF;
    size  = 0;
    for (i = 0; i < banks; i++) {
        cfgwritel(Fn1, r + Bankidx, i);
        n = cfgreadl(Fn1, r + Bankinfo);
        if (SBDEBUG)
            printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
        size += 8192 * ((n & 0x3F) + 1);
    }
    ctrl.socramsize = size;
    ctrl.rambase    = 0;
    if (ctrl.chipid == 43430) {
        cfgwritel(Fn1, r + Bankidx, 3);
        cfgwritel(Fn1, r + Bankpda, 0);
    }
}

static bool sbinit() {
    uint32_t r, chipid;

    sbwindow(Enumbase);
    r      = cfgreadl(Fn1, Enumbase);
    chipid = r & 0xFFFF;
    printf("Chip Id %x %d \n", chipid, chipid);

    switch (chipid) {
    case 0x4330:
    case 43362:
    case 43430:
    case 0x4345:
        ctrl.chiprev = (r >> 16) & 0xF;

        printf("Chip rev: %d \n", ctrl.chiprev);
        ctrl.chipid = chipid;
        break;
    default:
        printf("ether4330: chipid %#x (%d) not supported\n", chipid, chipid);
        return false;
    }
    r = cfgreadl(Fn1, Enumbase + 63 * 4);
    corescan(r);

    if (ctrl.armctl == 0 || ctrl.d11ctl == 0 ||
        (ctrl.armcore == ARMcm3 && (ctrl.socramctl == 0 || ctrl.socramregs == 0))) {
        printf("corescan didn't find essential cores\n");
        return false;
    }

    if (ctrl.armcore == ARMcr4) {
        printf("Reseting sonic backplane \n");
        sbreset(ctrl.armctl, Cr4Cpuhalt, Cr4Cpuhalt);
    } else {
        printf("Disabled sonic backplane \n");
        sbdisable(ctrl.armctl, 0, 0);
    }

    sbreset(ctrl.d11ctl, 8 | 4, 4);
    ramscan();
    printf("ARM 0x%x D11 0x%x SOCRAM 0x%x,0x%x %lu bytes @ 0x%x\n", ctrl.armctl, ctrl.d11ctl, ctrl.socramctl,
           ctrl.socramregs, ctrl.socramsize, ctrl.rambase);
    cfgw(Clkcsr, 0);
    MicroDelay(10);

    printf("chipclk: %x\n", cfgr(Clkcsr));
    cfgw(Clkcsr, Nohwreq | ReqHT);

    while ((cfgr(Clkcsr) & (HTavail | ALPavail)) == 0) {
        MicroDelay(10);
    }
    printf(" HT or ALP avail? %d clk:0x%x \n", (cfgr(Clkcsr) & (HTavail | ALPavail)), cfgr(Clkcsr));
    cfgw(Clkcsr, Nohwreq | ForceALP);
    MicroDelay(65);
    printf("chipclk: %x\n", cfgr(Clkcsr));
    cfgw(Pullups, 0);
    sbwindow(ctrl.chipcommon);
    cfgwritel(Fn1, ctrl.chipcommon + Gpiopullup, 0);
    cfgwritel(Fn1, ctrl.chipcommon + Gpiopulldown, 0);
    if (chipid != 0x4330 && chipid != 43362) {
        return true;
    }
    cfgwritel(Fn1, ctrl.chipcommon + Chipctladdr, 1);
    if (cfgreadl(Fn1, ctrl.chipcommon + Chipctladdr) != 1) {
        printf("ERROR: ether4330: can't set Chipctladdr\n");
        return false;
    } else {
        r = cfgreadl(Fn1, ctrl.chipcommon + Chipctldata);
        printf("chipcommon PMU (0x%x) 0x%x", cfgreadl(Fn1, ctrl.chipcommon + Chipctladdr), r);
        /* set SDIO drive strength >= 6mA */
        r &= ~0x3800;
        if (chipid == 0x4330) {
            r |= 3 << 11;
        } else {
            r |= 7 << 11;
        }
        cfgwritel(Fn1, ctrl.chipcommon + Chipctldata, r);
        printf("-> %lux (= %lux)\n", r, cfgreadl(Fn1, ctrl.chipcommon + Chipctldata));
    }

    //

    return true;
}

bool startWifi() {
    uint32_t binarysize = (uint32_t) &wifibinary_end - (uint32_t) &wifibinary_start;
    printf("Binary Size: %d \n", binarysize);
    bool is_success = true;
    // if(ctrl.chipid == 0) {
    is_success = sdioinit();
    is_success = sbinit();
    fwload();
    sbenable();
    // }
    return is_success;
}

static int sdiord(uint32_t fn, uint32_t addr) {
    uint32_t response = sdiocmd_locked(52, (0 << 31) | ((fn & 7) << 28) | ((addr & 0x1FFFF) << 9));
    // if(!is_success) {
    //     printf("Could not complete IO_RW_DIRECT: fn :%d addr: 0x%x \n", fn, addr);
    //     return 0;
    // }
    if (response & 0xCF00) {
        printf("ERROR: ether4330: sdiord(%x, %x) fail: %2.2x %2.2x\n", fn, addr, (response >> 8) & 0xFF,
               response & 0xFF);
    }
    return response & 0xFF;
}

static void sdiowr(int fn, int addr, int data) {
    uint32_t r;
    int retry;

    r = 0;
    for (retry = 0; retry < 10; retry++) {
        emmccmd(52, (1 << 31) | ((fn & 7) << 28) | ((addr & 0x1FFFF) << 9) | (data & 0xFF), &cmd_resp[0]);
        r = cmd_resp[0];
        if ((r & 0xCF00) == 0) {
            return;
        }
    }
    printf("ERROR: ether4330: sdiowr(%x, %x, %x) fail: %2.2ux %2.2ux\n", fn, addr, data, (r >> 8) & 0xFF, r & 0xFF);
}

static void sdioset(uint32_t fn, uint32_t addr, uint32_t bits) {
    sdiowr(fn, addr, sdiord(fn, addr) | bits);
    if ((sdiord(fn, addr) & bits) == 0) {
        printf("Failed to set bits : 0x%x value: 0x%x\n", bits, sdiord(fn, addr));
    }
}

static bool sdioinit() {
    bool is_success = false;

    for (uint32_t i = 48; i <= 53; i++) {
        select_alt_func(i, Alt0);
    }

    // FOllowing lines connect EMMC controller to SD CARD
    for (uint32_t i = 34; i <= 39; i++) {
        select_alt_func(i, Alt3);
        if (i == 34) {
            disable_pulling(i);
        } else {
            pullup_pin(i);
        }
    }

    emmcinit();
    emmcenable();
    is_success = emmccmd(0, 0, &cmd_resp[0]);
    if (is_success) {
        printf(" Sent go idle command \n");
    }

    emmccmd(5, 0, &cmd_resp[0]);
    ctrl.ocr = cmd_resp[0];

    printf("Found card OCR : 0x%x\n", ctrl.ocr);
    uint32_t i = 0;
    while ((ctrl.ocr & (1 << 31)) == 0) {
        if (++i > 5) {
            printf("ether4330: no response to sdio access: ocr = %x\n", ctrl.ocr);
            return false;
        }
        // send op condition 3V_3
        is_success = emmccmd(5, 3 << 20, &cmd_resp[0]);
        ctrl.ocr   = cmd_resp[0];
        MicroDelay(100);
    }
    printf("NEW OCR : 0x%x\n", ctrl.ocr);
    emmccmd(3, 0, &cmd_resp[0]);
    // if(is_success) {
    ctrl.rca = (cmd_resp[0] >> 16);
    printf("rca: 0x%x \n", ctrl.rca);
    emmccmd(7, ctrl.rca << 16, &cmd_resp[0]);
    // if(!is_success) {
    //     printf("Could not select card \n");
    //     return false;
    // } else
    // {
    // 	 printf(" Selected the card successfully.\n");
    // }
    // }

    sdioset(Fn0, Highspeed, 2);
    sdioset(Fn0, Busifc, 2); /* bus width 4 */
    sdiowr(Fn0, Fbr1 + Blksize, 64);
    sdiowr(Fn0, Fbr1 + Blksize + 1, 64 >> 8);
    sdiowr(Fn0, Fbr2 + Blksize, 512);
    sdiowr(Fn0, Fbr2 + Blksize + 1, 512 >> 8);
    sdioset(Fn0, Ioenable, 1 << Fn1);
    printf("IOENABLE FN1: %d \n ", (sdiord(Fn0, Ioready) & 1 << Fn1));
    sdioset(Fn0, Ioenable, 1 << Fn2);
    printf("IOENABLE FN1: %d \n ", (sdiord(Fn0, Ioready) & 1 << Fn2));
    printf("Fn0 Capability: %x \n ", sdiord(Fn0, Capability));
    sdiowr(Fn0, Intenable, 0);

    // uint32_t i = 0;
    for (i = 0; !(sdiord(Fn0, Ioready) & 1 << Fn1); i++) {
        if (i == 10) {
            printf("ether4330: can't enable SDIO function\n");
            return false;
        }
        MicroDelay(100);
    }

    printf(" +++++Enabled SDIO IO functionality \n");
    return is_success;
}