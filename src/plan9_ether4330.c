/*
 * Broadcom bcm4330 wifi (sdio interface)
 */

#include <device/bcm4343.h>
#include <device/etherif.h>
#include <device/gpio.h>
#include <device/plan9_emmc.h>
#include <device/plan9_ether4330.h>
#include <kernel/fork.h>
#include <kernel/lock.h>
#include <kernel/scheduler.h>
#include <kernel/systimer.h>
#include <mem/kernel_alloc.h>
#include <plibc/stdio.h>
#include <plibc/string.h>
#include <stdarg.h>

#define USED(x)                                                                                                        \
    if (x)                                                                                                             \
        ;                                                                                                              \
    else {                                                                                                             \
    }
#define nelem(x) (sizeof(x) / sizeof((x)[0]))

extern int sdiocardintr(int);

#define CACHELINESZ 64 /* temp */

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

    /* SDIOCommands */
    GO_IDLE_STATE      = 0,
    SEND_RELATIVE_ADDR = 3,
    IO_SEND_OP_COND    = 5,
    SELECT_CARD        = 7,
    VOLTAGE_SWITCH     = 11,
    IO_RW_DIRECT       = 52,
    IO_RW_EXTENDED     = 53,

    /* SELECT_CARD args */
    Rcashift = 16,

    /* SEND_OP_COND args */
    Hcs  = 1 << 30, /* host supports SDHC & SDXC */
    V3_3 = 3 << 20, /* 3.2-3.4 volts */
    V2_8 = 3 << 15, /* 2.7-2.9 volts */
    V2_0 = 1 << 8,  /* 2.0-2.1 volts */
    S18R = 1 << 24, /* switch to 1.8V request */

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
    Intstatus    = 0x20,
    Fcstate      = 1 << 4,
    Fcchange     = 1 << 5,
    FrameInt     = 1 << 6,
    MailboxInt   = 1 << 7,
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

// static Ctlr ctrl = {0};

enum {
    Wpa        = 1,
    Wep        = 2,
    Wpa2       = 3,
    WNameLen   = 32,
    WNKeys     = 4,
    WKeyLen    = 32,
    WMinKeyLen = 5,
    WMaxKeyLen = 13,
};

// typedef struct {
// 	long	ref;
// 	Block*	next;
// 	Block*	list;
// 	uint8_t*	rp;			/* first unconsumed byte */
// 	uint8_t*	wp;			/* first empty byte */
// 	uint8_t*	lim;			/* 1 past the end of the buffer */
// 	uint8_t*	base;			/* start of the buffer */
// 	void	(*free)(Block*);
// 	uint16_t	flag;
// 	uint16_t	checksum;		/* IP checksum of complete packet (minus media header) */
// 	uint32_t	magic;
// } Block;

typedef struct WKey WKey;
struct WKey {
    uint16_t len;
    char dat[WKeyLen];
};

#define HOWMANY(x, y) (((x) + ((y) -1)) / (y))
#define ROUNDUP(x, y) (HOWMANY((x), (y)) * (y)) /* ceiling */
#define ROUNDDN(x, y) (((x) / (y)) * (y))       /* floor */
#define ROUND(s, sz)  (((s) + (sz - 1)) & ~(sz - 1))

typedef struct {
    Ether* edev;
    int joinstatus;
    int cryptotype;
    int chanid;
    char essid[WNameLen + 1];
    WKey keys[WNKeys];
    Block* rsp;
    Block* scanb;
    int scansecs;
    int status;
    int chipid;
    int chiprev;
    int armcore;
    char* regufile;
    union {
        uint32_t i;
        uint8_t c[4];
    } resetvec;
    uint32_t chipcommon;
    uint32_t armctl;
    uint32_t armregs;
    uint32_t d11ctl;
    uint32_t socramregs;
    uint32_t socramctl;
    uint32_t sdregs;
    int sdiorev;
    int socramrev;
    uint32_t socramsize;
    uint32_t rambase;
    short reqid;
    uint8_t fcmask;
    uint8_t txwindow;
    uint8_t txseq;
    uint8_t rxseq;

    uint32_t joinr;
    uint32_t cmdr;

    mutex_t cmdlock;
    mutex_t pktlock;
    mutex_t tlock;
    mutex_t alock;
    Lock txwinlock;
    ether_event_handler_t* evhndlr;
    void* evcontext;
} Ctlr;

static mutex_t sdiolock;

// static int iodebug = 0;
enum {
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

INCLUDE_BINARY_FILE(wifibinary, "wifi_firmware/brcmfmac43430-sdio.bin", ".rodata.brcmfmac43430_sdio_bin");
INCLUDE_BINARY_FILE(wifitext, "wifi_firmware/brcmfmac43430-sdio.txt", ".rodata.brcmfmac43430_sdio_txt");

typedef struct Sdpcm Sdpcm;
typedef struct Cmd Cmd;
struct Sdpcm {
    uint8_t len[2];
    uint8_t lenck[2];
    uint8_t seq;
    uint8_t chanflg;
    uint8_t nextlen;
    uint8_t doffset;
    uint8_t fcmask;
    uint8_t window;
    uint8_t version;
    uint8_t pad;
};

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
    int chipid;
    int chiprev;
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

/*
 * Command interface between host and firmware
 */

static char* eventnames[] = {[0]   = "set ssid",
                             [1]   = "join",
                             [2]   = "start",
                             [3]   = "auth",
                             [4]   = "auth ind",
                             [5]   = "deauth",
                             [6]   = "deauth ind",
                             [7]   = "assoc",
                             [8]   = "assoc ind",
                             [9]   = "reassoc",
                             [10]  = "reassoc ind",
                             [11]  = "disassoc",
                             [12]  = "disassoc ind",
                             [13]  = "quiet start",
                             [14]  = "quiet end",
                             [15]  = "beacon rx",
                             [16]  = "link",
                             [17]  = "mic error",
                             [18]  = "ndis link",
                             [19]  = "roam",
                             [20]  = "txfail",
                             [21]  = "pmkid cache",
                             [22]  = "retrograde tsf",
                             [23]  = "prune",
                             [24]  = "autoauth",
                             [25]  = "eapol msg",
                             [26]  = "scan complete",
                             [27]  = "addts ind",
                             [28]  = "delts ind",
                             [29]  = "bcnsent ind",
                             [30]  = "bcnrx msg",
                             [31]  = "bcnlost msg",
                             [32]  = "roam prep",
                             [33]  = "pfn net found",
                             [34]  = "pfn net lost",
                             [35]  = "reset complete",
                             [36]  = "join start",
                             [37]  = "roam start",
                             [38]  = "assoc start",
                             [39]  = "ibss assoc",
                             [40]  = "radio",
                             [41]  = "psm watchdog",
                             [44]  = "probreq msg",
                             [45]  = "scan confirm ind",
                             [46]  = "psk sup",
                             [47]  = "country code changed",
                             [48]  = "exceeded medium time",
                             [49]  = "icv error",
                             [50]  = "unicast decode error",
                             [51]  = "multicast decode error",
                             [52]  = "trace",
                             [53]  = "bta hci event",
                             [54]  = "if",
                             [55]  = "p2p disc listen complete",
                             [56]  = "rssi",
                             [57]  = "pfn scan complete",
                             [58]  = "extlog msg",
                             [59]  = "action frame",
                             [60]  = "action frame complete",
                             [61]  = "pre assoc ind",
                             [62]  = "pre reassoc ind",
                             [63]  = "channel adopted",
                             [64]  = "ap started",
                             [65]  = "dfs ap stop",
                             [66]  = "dfs ap resume",
                             [67]  = "wai sta event",
                             [68]  = "wai msg",
                             [69]  = "escan result",
                             [70]  = "action frame off chan complete",
                             [71]  = "probresp msg",
                             [72]  = "p2p probreq msg",
                             [73]  = "dcs request",
                             [74]  = "fifo credit map",
                             [75]  = "action frame rx",
                             [76]  = "wake event",
                             [77]  = "rm complete",
                             [78]  = "htsfsync",
                             [79]  = "overlay req",
                             [80]  = "csa complete ind",
                             [81]  = "excess pm wake event",
                             [82]  = "pfn scan none",
                             [83]  = "pfn scan allgone",
                             [84]  = "gtk plumbed",
                             [85]  = "assoc ind ndis",
                             [86]  = "reassoc ind ndis",
                             [87]  = "assoc req ie",
                             [88]  = "assoc resp ie",
                             [89]  = "assoc recreated",
                             [90]  = "action frame rx ndis",
                             [91]  = "auth req",
                             [92]  = "tdls peer event",
                             [127] = "bcmc credit support"};

typedef struct {
    unsigned index;
    const char* cmd;
    unsigned maxargs;
} Cmdtab;

typedef struct {
    char buf[200];
    unsigned argc;
    char* f[10];
} Cmdbuf;

static Cmdtab cmds[] = {
    {CMauth, "auth", 2},     {CMchannel, "channel", 2}, {CMcrypt, "crypt", 2},   {CMessid, "essid", 2},
    {CMkey1, "key1", 2},     {CMkey2, "key1", 2},       {CMkey3, "key1", 2},     {CMkey4, "key1", 2},
    {CMrxkey, "rxkey", 3},   {CMrxkey0, "rxkey0", 3},   {CMrxkey1, "rxkey1", 3}, {CMrxkey2, "rxkey2", 3},
    {CMrxkey3, "rxkey3", 3}, {CMtxkey, "txkey", 3},     {CMdebug, "debug", 2},   {CMjoin, "join", 5},
};

// static QLock sdiolock;
// static int iodebug;
static void callevhndlr(Ctlr*, ether_event_type_t, const ether_event_params_t*);

static uint8_t* put2(uint8_t* p, short v) {
    p[0] = v;
    p[1] = v >> 8;
    return p + 2;
}

static uint8_t* put4(uint8_t* p, long v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
    return p + 4;
}

static uint16_t get2(uint8_t* p) {
    return p[0] | p[1] << 8;
}

static uint32_t get4(uint8_t* p) {
    return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
}

static void dump(char* s, void* a, int n) {
    int i;
    uint8_t* p;

    p = a;
    printf("%s:", s);
    for (i = 0; i < n; i++)
        printf("%c%2.2x", i & 15 ? ' ' : '\n', *p++);
    printf("\n");
}

/*
 * SDIO communication with dongle
 */
static uint32_t sdiocmd_locked(int cmd, uint32_t arg) {
    uint32_t resp[4];

    emmccmd(cmd, arg, resp);
    return resp[0];
}

static uint32_t sdiocmd(int cmd, uint32_t arg) {
    uint32_t r;

    mutex_acquire(&sdiolock);
    // if(waserror()){
    // 	if(SDIODEBUG) printf("sdiocmd error: cmd %d arg %lux\n", cmd, arg);
    // 	qunlock(&sdiolock);
    // 	nexterror();
    // }
    r = sdiocmd_locked(cmd, arg);
    mutex_release(&sdiolock);
    // poperror();
    return r;
}

static uint32_t trysdiocmd(int cmd, uint32_t arg) {
    uint32_t r;

    // if(waserror())
    // 	return 0;
    r = sdiocmd(cmd, arg);
    // poperror();
    return r;
}

static int sdiord(int fn, int addr) {
    int r;

    r = sdiocmd(IO_RW_DIRECT, (0 << 31) | ((fn & 7) << 28) | ((addr & 0x1FFFF) << 9));
    if (r & 0xCF00) {
        printf("ether4330: sdiord(%x, %x) fail: %2.2x %2.2x\n", fn, addr, (r >> 8) & 0xFF, r & 0xFF);
        // error(Eio);
        return 0;
    }
    // printf("r: %x \n", r);
    return r & 0xFF;
}

static void sdiowr(int fn, int addr, int data) {
    int r;
    int retry;

    r = 0;
    for (retry = 0; retry < 10; retry++) {
        r = sdiocmd(IO_RW_DIRECT, (1 << 31) | ((fn & 7) << 28) | ((addr & 0x1FFFF) << 9) | (data & 0xFF));
        if ((r & 0xCF00) == 0)
            return;
    }
    printf("ERROR ether4330: sdiowr(%x, %x, %x) fail: %2.2ux %2.2ux\n", fn, addr, data, (r >> 8) & 0xFF, r & 0xFF);
    // error(Eio);
}

static void sdiorwext(int fn, int write, void* a, int len, int addr, int incr) {
    int bsize, blk, bcount, m;

    bsize = fn == Fn2 ? 512 : 64;
    while (len > 0) {
        if (len >= 511 * bsize) {
            blk    = 1;
            bcount = 511;
            m      = bcount * bsize;
        } else if (len > bsize) {
            blk    = 1;
            bcount = len / bsize;
            m      = bcount * bsize;
        } else {
            blk    = 0;
            bcount = len;
            m      = bcount;
        }
        mutex_acquire(&sdiolock);
        // if(waserror()){
        // printf("ether4330: sdiorwext fail: %s\n", up->errstr);
        // qunlock(&sdiolock);
        // 	nexterror();
        // }
        if (blk)
            emmciosetup(write, a, bsize, bcount);
        else
            emmciosetup(write, a, bcount, 1);
        sdiocmd_locked(IO_RW_EXTENDED, write << 31 | (fn & 7) << 28 | blk << 27 | incr << 26 | (addr & 0x1FFFF) << 9 |
                                           (bcount & 0x1FF));
        emmcio(write, a, m);
        mutex_release(&sdiolock);
        // poperror();
        len -= m;
        a = (char*) a + m;
        if (incr)
            addr += m;
    }
}

static void sdioset(int fn, int addr, int bits) {
    sdiowr(fn, addr, sdiord(fn, addr) | bits);
}

static void sdioinit(void) {
    uint32_t ocr, rca;
    int i;

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
    sdiocmd(GO_IDLE_STATE, 0);
    ocr = trysdiocmd(IO_SEND_OP_COND, 0);
    i   = 0;
    while ((ocr & (1 << 31)) == 0) {
        if (++i > 5) {
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
    sdioset(Fn0, Busifc, 2); /* bus width 4 */
    sdiowr(Fn0, Fbr1 + Blksize, 64);
    sdiowr(Fn0, Fbr1 + Blksize + 1, 64 >> 8);
    sdiowr(Fn0, Fbr2 + Blksize, 512);
    sdiowr(Fn0, Fbr2 + Blksize + 1, 512 >> 8);
    sdioset(Fn0, Ioenable, 1 << Fn1);
    sdiowr(Fn0, Intenable, 0);
    for (i = 0; !(sdiord(Fn0, Ioready) & 1 << Fn1); i++) {
        if (i == 10) {
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

static void cfgw(uint32_t off, int val) {
    sdiowr(Fn1, off, val);
}

static int cfgr(uint32_t off) {
    return sdiord(Fn1, off);
}

static uint32_t cfgreadl(int fn, uint32_t off) {
    uint8_t cbuf[2 * CACHELINESZ];
    uint8_t* p;

    p = (uint8_t*) ROUND((uint32_t) &cbuf[0], CACHELINESZ);
    memset(p, 0, 4);
    sdiorwext(fn, 0, p, 4, off | Sb32bit, 1);
    if (SDIODEBUG)
        printf("cfgreadl %lux: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
    return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
}

static void cfgwritel(int fn, uint32_t off, uint32_t data) {
    uint8_t cbuf[2 * CACHELINESZ];
    uint8_t* p;
    // int retry;

    p = (uint8_t*) ROUND((uint32_t) &cbuf[0], CACHELINESZ); // TODO check if this is working correctly
    put4(p, data);
    if (SDIODEBUG)
        printf("cfgwritel %lux: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
    // retry = 0;
    // while(waserror()){
    // 	printf("ether4330: cfgwritel retry %lux %ux\n", off, data);
    // 	sdioabort(fn);
    // 	// if(++retry == 3)
    // 	// 	nexterror();
    // }
    sdiorwext(fn, 1, p, 4, off | Sb32bit, 1);
    // poperror();
}

static void sbwindow(uint32_t addr) {
    addr &= ~(Sbwsize - 1);
    cfgw(Sbaddr, addr >> 8);
    cfgw(Sbaddr + 1, addr >> 16);
    cfgw(Sbaddr + 2, addr >> 24);
}

static void sbrw(int fn, int write, uint8_t* buf, int len, uint32_t off) {
    int n;
    USED(fn);

    // if(waserror()){
    // 	printf("ether4330: sbrw err off %lux len %ud\n", off, len);
    // 	nexterror();
    // }
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
    // poperror();
}

static void sbmem(int write, uint8_t* buf, int len, uint32_t off) {
    int32_t n;

    n = ROUNDUP(off, Sbwsize) - off;
    if (n == 0)
        n = Sbwsize;
    while (len > 0) {
        if (n > len)
            n = len;
        sbwindow(off);
        sbrw(Fn1, write, buf, n, off & (Sbwsize - 1));
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

static void sbdisable(uint32_t regs, int pre, int ioctl) {
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

static void sbreset(uint32_t regs, int pre, int ioctl) {
    sbdisable(regs, pre, ioctl);
    sbwindow(regs);
    if (SBDEBUG)
        printf("sbreset %#p %#lux %#lux ->", regs, cfgreadl(Fn1, regs + Ioctrl), cfgreadl(Fn1, regs + Resetctrl));
    while ((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0) {
        cfgwritel(Fn1, regs + Resetctrl, 0);
        MicroDelay(40);
    }
    cfgwritel(Fn1, regs + Ioctrl, 1 | ioctl);
    cfgreadl(Fn1, regs + Ioctrl);
    if (SBDEBUG)
        printf("%#lux %#lux\n", cfgreadl(Fn1, regs + Ioctrl), cfgreadl(Fn1, regs + Resetctrl));
}

static void corescan(Ctlr* ctl, uint32_t r) {
    uint8_t* buf;
    int i, coreid, corerev;
    uint32_t addr;

    buf = kernel_allocate(Corescansz);
    if (buf == 0) {
        // error(Enomem);
        printf("Error no memory for buf \n");
        return;
    }

    sbmem(0, buf, Corescansz, r);
    coreid  = 0;
    corerev = 0;
    for (i = 0; i < Corescansz; i += 4) {
        switch (buf[i] & 0xF) {
        case 0xF: /* end */
            kernel_deallocate(buf);
            return;
        case 0x1: /* core info */
            if ((buf[i + 4] & 0xF) != 0x1)
                break;
            coreid = (buf[i + 1] | buf[i + 2] << 8) & 0xFFF;
            i += 4;
            corerev = buf[i + 3];
            break;
        case 0x05: /* address */
            addr = buf[i + 1] << 8 | buf[i + 2] << 16 | buf[i + 3] << 24;
            addr &= ~0xFFF;
            if (SBDEBUG)
                printf("core %x %s %#p\n", coreid, buf[i] & 0xC0 ? "ctl" : "mem", addr);
            switch (coreid) {
            case 0x800:
                if ((buf[i] & 0xC0) == 0)
                    ctl->chipcommon = addr;
                break;
            case ARMcm3:
            case ARM7tdmi:
            case ARMcr4:
                ctl->armcore = coreid;
                if (buf[i] & 0xC0) {
                    if (ctl->armctl == 0)
                        ctl->armctl = addr;
                } else {
                    if (ctl->armregs == 0)
                        ctl->armregs = addr;
                }
                break;
            case 0x80E:
                if (buf[i] & 0xC0)
                    ctl->socramctl = addr;
                else if (ctl->socramregs == 0)
                    ctl->socramregs = addr;
                ctl->socramrev = corerev;
                break;
            case 0x829:
                if ((buf[i] & 0xC0) == 0)
                    ctl->sdregs = addr;
                ctl->sdiorev = corerev;
                break;
            case 0x812:
                if (buf[i] & 0xC0)
                    ctl->d11ctl = addr;
                break;
            }
        }
    }
    kernel_deallocate(buf);
}

static void ramscan(Ctlr* ctl) {
    uint32_t r, n, size;
    int banks, i;

    if (ctl->armcore == ARMcr4) {
        r = ctl->armregs;
        sbwindow(r);
        n = cfgreadl(Fn1, r + Cr4Cap);
        if (SBDEBUG)
            printf("cr4 banks %lux\n", n);
        banks = ((n >> 4) & 0xF) + (n & 0xF);
        size  = 0;
        for (i = 0; i < banks; i++) {
            cfgwritel(Fn1, r + Cr4Bankidx, i);
            n = cfgreadl(Fn1, r + Cr4Bankinfo);
            if (SBDEBUG)
                printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
            size += 8192 * ((n & 0x3F) + 1);
        }
        ctl->socramsize = size;
        ctl->rambase    = 0x198000;
        return;
    }
    if (ctl->socramrev <= 7 || ctl->socramrev == 12) {
        printf("ERRROR ether4330: SOCRAM rev %d not supported\n", ctl->socramrev);
        // error(Eio);
        return;
    }
    sbreset(ctl->socramctl, 0, 0);
    r = ctl->socramregs;
    sbwindow(r);
    n = cfgreadl(Fn1, r + Coreinfo);
    if (SBDEBUG)
        printf("socramrev %d coreinfo %lux\n", ctl->socramrev, n);
    banks = (n >> 4) & 0xF;
    size  = 0;
    for (i = 0; i < banks; i++) {
        cfgwritel(Fn1, r + Bankidx, i);
        n = cfgreadl(Fn1, r + Bankinfo);
        if (SBDEBUG)
            printf("bank %d reg %lux size %lud\n", i, n, 8192 * ((n & 0x3F) + 1));
        size += 8192 * ((n & 0x3F) + 1);
    }
    ctl->socramsize = size;
    ctl->rambase    = 0;
    if (ctl->chipid == 43430) {
        cfgwritel(Fn1, r + Bankidx, 3);
        cfgwritel(Fn1, r + Bankpda, 0);
    }
}

static void sbinit(Ctlr* ctl) {
    uint32_t r;
    int chipid;
    char buf[16];

    sbwindow(Enumbase);
    r      = cfgreadl(Fn1, Enumbase);
    chipid = r & 0xFFFF;
    printf("Chip Id : 0x%x %d \n", chipid, chipid);
    printf("ether4330: chip %s rev %ld type %ld\n", buf, (r >> 16) & 0xF, (r >> 28) & 0xF);
    switch (chipid) {
    case 0x4330:
    case 43362:
    case 43430:
    case 0x4345:
        ctl->chipid  = chipid;
        ctl->chiprev = (r >> 16) & 0xF;
        break;
    default:
        printf("ERROR ether4330: chipid %#x (%d) not supported\n", chipid, chipid);
        // error(Eio);
        return;
    }
    r = cfgreadl(Fn1, Enumbase + 63 * 4);
    corescan(ctl, r);
    if (ctl->armctl == 0 || ctl->d11ctl == 0 ||
        (ctl->armcore == ARMcm3 && (ctl->socramctl == 0 || ctl->socramregs == 0)))
        printf("corescan didn't find essential cores\n");
    if (ctl->armcore == ARMcr4)
        sbreset(ctl->armctl, Cr4Cpuhalt, Cr4Cpuhalt);
    else
        sbdisable(ctl->armctl, 0, 0);
    sbreset(ctl->d11ctl, 8 | 4, 4);
    ramscan(ctl);
    if (SBDEBUG)
        printf("ARM %#p D11 %#p SOCRAM %#p,%#p %lud bytes @ %#p\n", ctl->armctl, ctl->d11ctl, ctl->socramctl,
               ctl->socramregs, ctl->socramsize, ctl->rambase);
    cfgw(Clkcsr, 0);
    MicroDelay(10);
    if (SBDEBUG)
        printf("chipclk: %x\n", cfgr(Clkcsr));
    cfgw(Clkcsr, Nohwreq | ReqALP);
    while ((cfgr(Clkcsr) & (HTavail | ALPavail)) == 0)
        MicroDelay(10);
    cfgw(Clkcsr, Nohwreq | ForceALP);
    MicroDelay(65);
    if (SBDEBUG)
        printf("chipclk: %x\n", cfgr(Clkcsr));
    cfgw(Pullups, 0);
    sbwindow(ctl->chipcommon);
    cfgwritel(Fn1, ctl->chipcommon + Gpiopullup, 0);
    cfgwritel(Fn1, ctl->chipcommon + Gpiopulldown, 0);
    if (ctl->chipid != 0x4330 && ctl->chipid != 43362)
        return;
    cfgwritel(Fn1, ctl->chipcommon + Chipctladdr, 1);
    if (cfgreadl(Fn1, ctl->chipcommon + Chipctladdr) != 1)
        printf("ether4330: can't set Chipctladdr\n");
    else {
        r = cfgreadl(Fn1, ctl->chipcommon + Chipctldata);
        if (SBDEBUG)
            printf("chipcommon PMU (%lux) %lux", cfgreadl(Fn1, ctl->chipcommon + Chipctladdr), r);
        /* set SDIO drive strength >= 6mA */
        r &= ~0x3800;
        if (ctl->chipid == 0x4330)
            r |= 3 << 11;
        else
            r |= 7 << 11;
        cfgwritel(Fn1, ctl->chipcommon + Chipctldata, r);
        if (SBDEBUG)
            printf("-> %lux (= %lux)\n", r, cfgreadl(Fn1, ctl->chipcommon + Chipctldata));
    }
}

static void sbenable(Ctlr* ctl) {
    int i;

    if (SBDEBUG)
        printf("enabling HT clock...");
    cfgw(Clkcsr, 0);
    MicroDelay(1000000);
    cfgw(Clkcsr, ReqHT);
    for (i = 0; (cfgr(Clkcsr) & HTavail) == 0; i++) {
        if (i == 50) {
            printf("ether4330: can't enable HT clock: csr %x\n", cfgr(Clkcsr));
            // error(Eio);
            return;
        }
        // tsleep(&up->sleep, return0, nil, 100);
        MicroDelay(100);
    }

    printf("HTCLOCK: 0x%x \n", cfgr(Clkcsr));

    cfgw(Clkcsr, cfgr(Clkcsr) | ForceHT);
    MicroDelay(1000 * 10);
    if (SBDEBUG)
        printf("chipclk: %x\n", cfgr(Clkcsr));
    sbwindow(ctl->sdregs);
    cfgwritel(Fn1, ctl->sdregs + Sbmboxdata, 4 << 16); /* protocol version */
    cfgwritel(Fn1, ctl->sdregs + Intmask, FrameInt | MailboxInt | Fcchange);
    sdioset(Fn0, Ioenable, 1 << Fn2);
    for (i = 0; !(sdiord(Fn0, Ioready) & 1 << Fn2); i++) {
        if (i == 10) {
            printf("ERROR ether4330: can't enable SDIO function 2 - ioready %x\n", sdiord(Fn0, Ioready));
            // error(Eio);
            return;
        }
        // tsleep(&up->sleep, return0, nil, 100);
        MicroDelay(100);
    }
    sdiowr(Fn0, Intenable, (1 << Fn1) | (1 << Fn2) | 1);
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
static int condense(uint8_t* buf, int n) {
    uint8_t *p, *ep, *lp, *op;
    int c, skipping;

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
            if (!skipping)
                *op++ = c;
            break;
        }
    }
    if (!skipping && op != lp)
        *op++ = '\0';
    *op++ = '\0';
    for (n = op - buf; n & 03; n++)
        *op++ = '\0';
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

static int upload(Ctlr* ctl, char* file, int isconfig) {
    FileInfo* file_info;
    uint8_t* buf  = NULL;
    uint8_t* cbuf = NULL;
    uint32_t off, n;

    file_info = findfirmware(file);

    if (file_info == NULL) {
        printf(" Could not load the file. \n");
        return -1;
    }

    buf = kernel_allocate(Uploadsz);

    if (buf == NULL) {
        printf("Could not allocate memory for buffer. \n");
        return -1;
    }

    if (Firmwarecmp) {
        cbuf = kernel_allocate(Uploadsz);
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
            off = ctl->socramsize - n - 4;
        } else if (off == 0) {
            memmove(ctl->resetvec.c, buf, sizeof(ctl->resetvec.c));
        }

        while (n & 3) {
            buf[n++] = 0;
        }
        // printf("Transferring bytes: %d \n", n);
        sbmem(1, buf, n, ctl->rambase + off);
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
            sbmem(0, cbuf, n, ctl->rambase + off);
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
    kernel_deallocate(buf);
    kernel_deallocate(cbuf);
    return n;
}

static void fwload(Ctlr* ctl) {
    uint8_t buf[4];
    uint32_t i, n;

    i = 0;
    while (firmware[i].chipid != ctl->chipid || firmware[i].chiprev != ctl->chiprev) {
        if (++i == nelem(firmware)) {
            printf("ether4330: no firmware for chipid %x (%d) chiprev %d\n", ctl->chipid, ctl->chipid, ctl->chiprev);
            printf("no firmware");
        }
    }
    ctl->regufile = firmware[i].regufile;
    cfgw(Clkcsr, ReqALP);
    while ((cfgr(Clkcsr) & ALPavail) == 0)
        MicroDelay(10);
    memset(buf, 0, 4);
    sbmem(1, buf, 4, ctl->rambase + ctl->socramsize - 4);
    if (FWDEBUG)
        printf("firmware load...");
    upload(ctl, firmware[i].fwfile, 0);
    if (FWDEBUG)
        printf("config load...");
    n = upload(ctl, firmware[i].cfgfile, 1);
    n /= 4;
    n = (n & 0xFFFF) | (~n << 16);
    put4(buf, n);
    sbmem(1, buf, 4, ctl->rambase + ctl->socramsize - 4);
    if (ctl->armcore == ARMcr4) {
        sbwindow(ctl->sdregs);
        cfgwritel(Fn1, ctl->sdregs + Intstatus, ~0);
        if (ctl->resetvec.i != 0) {
            if (SBDEBUG)
                printf("%ux\n", ctl->resetvec.i);
            sbmem(1, ctl->resetvec.c, sizeof(ctl->resetvec.c), 0);
        }
        sbreset(ctl->armctl, Cr4Cpuhalt, 0);
    } else
        sbreset(ctl->armctl, 0, 0);
}

static void packetrw(int write, uint8_t* buf, int len) {
    int n;
    // int retry;

    n = 2048;
    while (len > 0) {
        if (n > len) {
            n = ROUND(len, 4);
        }

        // retry = 0;
        // while(waserror()){
        // 	sdioabort(Fn2);
        // 	if(++retry == 3)
        // 		nexterror();
        // }
        sdiorwext(Fn2, write, buf, n, Enumbase, 0);
        // poperror();
        buf += n;
        len -= n;
    }
}

static Block* wlreadpkt(Ctlr* ctl) {
    Block* b;
    Sdpcm* p;
    uint32_t len, lenck;

    b = allocb(2048);
    p = (Sdpcm*) b->wp;
    mutex_acquire(&ctl->pktlock);
    // printf("ctl should be locked here.: %x \n", ctl);
    for (;;) {
        packetrw(0, b->wp, sizeof(*p));
        len = p->len[0] | p->len[1] << 8;
        if (len == 0) {
            // printf("Packet length 0 returning \n");
            freeb(b);
            b = 0;
            break;
        }
        lenck = p->lenck[0] | p->lenck[1] << 8;
        if (lenck != (len ^ 0xFFFF) || len < sizeof(*p) || len > 2048) {
            printf("ether4330: wlreadpkt error len %.4x lenck %.4x\n", len, lenck);
            cfgw(Framectl, Rfhalt);
            while (cfgr(Rfrmcnt + 1))
                ;
            while (cfgr(Rfrmcnt))
                ;
            continue;
        }
        if (len > sizeof(*p))
            packetrw(0, b->wp + sizeof(*p), len - sizeof(*p));
        b->wp += len;
        // printf("Breaking packet read process \n");
        break;
    }
    mutex_release(&ctl->pktlock);
    // printf("read packet successful \n");
    return b;
}
#define BLEN(b) ((b)->wp - (b)->rp)

static void txstart(Ether* edev) {
    Ctlr* ctl;
    Sdpcm* p;
    Block* b;
    int len, off;

    ctl = edev->ctlr;
    mutex_acquire(&ctl->tlock);
    // if(!canqlock(&ctl->tlock))
    // 	return;
    // if(waserror()){
    // 	qunlock(&ctl->tlock);
    // 	return;
    // }
    for (;;) {
        lock(&ctl->txwinlock);
        if (ctl->txseq == ctl->txwindow) {
            printf("f \n");
            unlock(&ctl->txwinlock);
            break;
        }
        if (ctl->fcmask & 1 << 2) {
            printf("x \n");
            unlock(&ctl->txwinlock);
            break;
        }
        unlock(&ctl->txwinlock);
        b = qget(edev->oq);
        if (b == 0) {
            break;
        }

        off = ((uint32_t)((uint32_t*) b->rp) & 3) + sizeof(Sdpcm);
        b   = padblock(b, off + 4);
        len = BLEN(b);
        p   = (Sdpcm*) b->rp;
        memset(p, 0, off); /* TODO: refactor dup code */
        put2(p->len, len);
        put2(p->lenck, ~len);
        p->chanflg = 2;
        p->seq     = ctl->txseq;
        p->doffset = off;
        put4(b->rp + off, 0x20); /* BDC header */
        // if(iodebug)
        dump("send", b->rp, len);
        mutex_acquire(&ctl->pktlock);
        // if(waserror()){
        // 	if(iodebug) print("halt frame %x %x\n", cfgr(Wfrmcnt+1), cfgr(Wfrmcnt+1));
        // 	cfgw(Framectl, Wfhalt);
        // 	while(cfgr(Wfrmcnt+1))
        // 		;
        // 	while(cfgr(Wfrmcnt))
        // 		;
        // 	qunlock(&ctl->pktlock);
        // 	nexterror();
        // }
        packetrw(1, b->rp, len);
        ctl->txseq++;
        // poperror();
        mutex_release(&ctl->pktlock);
        freeb(b);
    }
    // poperror();
    mutex_release(&ctl->tlock);
}

struct up_t {
    uint32_t sleep;
    const char* errstr;
    char genbuf[16];
};

static struct up_t* up = {0};

static void intwait(Ctlr* ctlr, int wait) {
    unsigned long ints, mbox;
    int i;

    // if (waserror())
    //     return;
    for (;;) {
        sdiocardintr(wait);
        sbwindow(ctlr->sdregs);
        i = sdiord(Fn0, Intpend);
        // print(" sdiord i:%x \n");
        if (i == 0) {
            // tsleep(&up->sleep, return0, 0, 10);
            continue;
        }
        ints = cfgreadl(Fn1, ctlr->sdregs + Intstatus);
        cfgwritel(Fn1, ctlr->sdregs + Intstatus, ints);
        // if (0)
        printf("INTS: (%x) %x -> %x\n", i, ints, cfgreadl(Fn1, ctlr->sdregs + Intstatus));
        if (ints & MailboxInt) {
            mbox = cfgreadl(Fn1, ctlr->sdregs + Hostmboxdata);
            cfgwritel(Fn1, ctlr->sdregs + Sbmbox, 2); /* ack */
            if (mbox & 0x8)
                printf("ether4330: firmware ready\n");
        }
        if (ints & FrameInt)
            break;
    }
    // poperror();
}

static void linkdown(Ctlr* ctl) {
    Ether* edev;
    Netfile* f;
    uint32_t i;
    edev = ctl->edev;
    if (edev == 0 || ctl->status != Connected)
        return;
    ctl->status = Disconnected;
    /* send eof to aux/wpa */
    for (i = 0; i < edev->nfile; i++) {
        f = edev->f[i];
        if (f == 0 || f->in == 0 || f->inuse == 0 || f->type != 0x888e) {
            continue;
        }
        qwrite(f->in, 0, 0);
    }
}

int snprint(char* str, uint32_t str_size, const char* fmt, ...) {
    va_list args;
    int i;
    USED(str_size);
    va_start(args, fmt);
    i = vsprintf(str, fmt, args);
    va_end(args);
    str[i] = '\0';
    return i;
}

static char* evstring(uint32_t event) {
    static char buf[12];

    if (event >= nelem(eventnames) || eventnames[event] == 0) {
        /* not reentrant but only called from one kproc */
        snprint(buf, sizeof buf, "%d", event);
        return buf;
    }
    return eventnames[event];
}

char* seprint(char* str, char* end, const char* fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(str, fmt, args);
    va_end(args);
    *(end - 1) = '\0';
    return &str[i + 1];
}

static uint8_t* gettlv(uint8_t* p, uint8_t* ep, int tag) {
    int len;

    while (p + 1 < ep) {
        len = p[1];
        if (p + 2 + len > ep)
            return NULL;
        if (p[0] == tag)
            return p;
        p += 2 + len;
    }
    return NULL;
}

static void addscan(Block* bp, uint8_t* p, int len) {
    char bssid[24];
    char *auth, *auth2;
    uint8_t *t, *et;
    int ielen;
    static uint8_t wpaie1[4] = {0x00, 0x50, 0xf2, 0x01};

    snprint(bssid, sizeof bssid, ";bssid=%s", p + 8);
    if (strstr((char*) bp->rp, bssid) != NULL) {
        printf("Returning from str str \n");
        return;
    }

    bp->wp = (uint8_t*) seprint((char*) bp->wp, (char*) bp->lim, "ssid=%.*s%s;signal=%d;noise=%d;chan=%d", p[18],
                                (char*) p + 19, bssid, (short) get2(p + 78), (signed char) p[80], get2(p + 72) & 0xF);

    auth = auth2 = "";
    if (get2(p + 16) & 0x10)
        auth = ";wep";
    ielen = get4(p + 0x78);
    if (ielen > 0) {
        t  = p + get4(p + 0x74);
        et = t + ielen;
        if (et > p + len)
            return;
        if (gettlv(t, et, 0x30) != NULL) {
            auth  = "";
            auth2 = ";wpa2";
        }
        while ((t = gettlv(t, et, 0xdd)) != NULL) {
            if (t[1] > 4 && memcmp(t + 2, wpaie1, 4) == 0) {
                auth = ";wpa";
                break;
            }
            t += 2 + t[1];
        }
    }
    bp->wp = (uint8_t*) seprint((char*) bp->wp, (char*) bp->lim, "%s%s\n", auth, auth2);
}

static void wlscanresult(Ether* edev, uint8_t* p, uint32_t len) {
    Ctlr* ctlr;
    Netfile **ep, *f, **fp;
    Block* bp;
    int nbss, i;

    ctlr = edev->ctlr;
    if (get4(p) > len)
        return;
    /* TODO: more syntax checking */
    bp = ctlr->scanb;
    if (bp == 0)
        ctlr->scanb = bp = allocb(8192);
    nbss = get2(p + 10);
    p += 12;
    len -= 12;
    // if(0)
    dump("SCAN", p, len);
    if (nbss) {
        addscan(bp, p, len);
        return;
    }
    i  = edev->scan;
    ep = &edev->f[Ntypes];
    for (fp = edev->f; fp < ep && i > 0; fp++) {
        f = *fp;
        if (f == 0 || f->scan == 0)
            continue;
        if (i == 1)
            qpass(f->in, bp);
        else
            qpass(f->in, copyblock(bp, BLEN(bp)));
        i--;
    }
    if (i)
        freeb(bp);
    ctlr->scanb = 0;
}

static void bcmevent(Ctlr* ctl, uint8_t* p, int len) {
    int flags;
    long event, status, reason;
    ether_event_params_t params;

    if (len < ETHERHDRSIZE + 10 + 46) {
        return;
    }

    memset(&params, 0, sizeof params);

    p += ETHERHDRSIZE + 10; /* skip bcm_ether header */
    len -= ETHERHDRSIZE + 10;

    // printf("p:%x p+2: %x p+6: %x p+8: %x p+12: %x \n", p, p + 2, p + 6, p + 8, p + 12);
    flags = nhgets(p + 2);
    // printf("p+2 passed \n");
    event = nhgets(p + 6);
    // printf("p+6 passed \n");
    status = nhgetl(p + 8);
    // printf("p+8 passed \n");
    reason = nhgetl(p + 12);
    // printf("p+12 passed \n");
    if (EVENTDEBUG)
        printf("ether4330: [%s] status %ld flags %#x reason %ld\n", evstring(event), status, flags, reason);
    switch (event) {
    case 19: /* E_ROAM */
        if (status == 0)
            break;
    /* fall through */
    case 0: /* E_SET_SSID */
        ctl->joinstatus = 1 + status;
        wakeup(&ctl->joinr);
        break;
    case 5: /* E_DEAUTH */
    case 6: /* E_DEAUTH_IND */
        linkdown(ctl);
        callevhndlr(ctl, ether_event_deauth, 0);
        break;
    case 16:           /* E_LINK */
        if (flags & 1) /* link up */
            break;
        /* fall through */
    case 12: /* E_DISASSOC_IND */
        linkdown(ctl);
        callevhndlr(ctl, ether_event_deauth, 0);
        break;
    case 17: /* E_MIC_ERROR */
        params.mic_error.group = !!(flags & 4);
        memcpy(params.mic_error.addr, p + 24, Eaddrlen);
        callevhndlr(ctl, ether_event_mic_error, &params);
    case 26: /* E_SCAN_COMPLETE */
        break;
    case 69: /* E_ESCAN_RESULT */
        wlscanresult(ctl->edev, p + 48, len - 48);
        break;
    default:
        if (status) {
            if (!EVENTDEBUG)
                printf("ether4330: [%s] error status %ld flags %#x reason %ld\n", evstring(event), status, flags,
                       reason);
            dump("event", p, len);
        }
    }
}

static void rproc(void* a) {
    Ether* edev;
    Ctlr* ctl;
    Block* b;
    Sdpcm* p;
    Cmd* q;
    int flowstart;
    int bdc;

    edev      = a;
    ctl       = edev->ctlr;
    flowstart = 0;
    // printf("%s: %d edev: %x \n", __FILE__, __LINE__, edev);
    for (;;) {
        // printf("In RPROC \n");
        if (flowstart) {
            printf("Flow start calling txstart \n");
            flowstart = 0;
            txstart(edev);
        }
        b = wlreadpkt(ctl);
        if (b == 0) {
            // printf("calling intwait \n");
            intwait(ctl, 1);
            // printf("intwait complete \n");
            continue;
        }
        p = (Sdpcm*) b->rp;
        if (p->window != ctl->txwindow || p->fcmask != ctl->fcmask) {
            lock(&ctl->txwinlock);
            if (p->window != ctl->txwindow) {
                if (ctl->txseq == ctl->txwindow)
                    flowstart = 1;
                ctl->txwindow = p->window;
            }
            if (p->fcmask != ctl->fcmask) {
                if ((p->fcmask & 1 << 2) == 0)
                    flowstart = 1;
                ctl->fcmask = p->fcmask;
            }
            unlock(&ctl->txwinlock);
        }
        switch (p->chanflg & 0xF) {
        case 0:
            // if (iodebug)
            //     dump("rsp", b->rp, BLEN(b));
            if ((uint32_t) BLEN(b) < sizeof(Sdpcm) + sizeof(Cmd))
                break;
            q = (Cmd*) (b->rp + sizeof(*p));
            if ((q->id[0] | q->id[1] << 8) != ctl->reqid)
                break;
            ctl->rsp = b;
            // wakeup(&ctl->cmdr);
            continue;
        case 1:
            // if(iodebug)
            // dump("event", b->rp, BLEN(b));
            if (BLEN(b) > p->doffset + 4) {
                bdc = 4 + (b->rp[p->doffset + 3] << 2);
                if (BLEN(b) > p->doffset + bdc) {
                    b->rp += p->doffset + bdc; /* skip BDC header */
                    bcmevent(ctl, b->rp, BLEN(b));
                    break;
                }
            }
            // if(iodebug && BLEN(b) != p->doffset)
            // printf("short event %ld %d\n", BLEN(b), p->doffset);
            break;
        case 2:
            // if(iodebug)
            // dump("packet", b->rp, BLEN(b));
            if (BLEN(b) > p->doffset + 4) {
                bdc = 4 + (b->rp[p->doffset + 3] << 2);
                if (BLEN(b) >= p->doffset + bdc + ETHERHDRSIZE) {
                    b->rp += p->doffset + bdc; /* skip BDC header */
                    etheriq(edev, b, 1);
                    continue;
                }
            }
            break;
        default:
            dump("ether4330: bad packet", b->rp, BLEN(b));
            break;
        }
        freeb(b);
    }
    terminate_this_task();
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int parsehex(char* buf, int buflen, char* a) {
    int i, k, n;

    k = 0;
    for (i = 0; k < buflen && *a; i++) {
        if (*a >= '0' && *a <= '9')
            n = *a++ - '0';
        else if (*a >= 'a' && *a <= 'f')
            n = *a++ - 'a' + 10;
        else if (*a >= 'A' && *a <= 'F')
            n = *a++ - 'A' + 10;
        else
            break;

        if (i & 1) {
            buf[k] |= n;
            k++;
        } else
            buf[k] = n << 4;
    }
    if (i & 1)
        return -1;
    return k;
}

static int cmddone(void* a) {
    return ((Ctlr*) a)->rsp != 0;
}

static void wlcmd(Ctlr* ctl, int write, int op, void* data, int dlen, void* res, int rlen) {
    Block* b;
    Sdpcm* p;
    Cmd* q;
    int len, tlen;

    if (write)
        tlen = dlen + rlen;
    else
        tlen = MAX(dlen, rlen);
    len = sizeof(Sdpcm) + sizeof(Cmd) + tlen;
    b   = allocb(len);
    if (b == 0) {
        printf("Error while allocating block");
        freeb(b);
        return;
    }
    mutex_acquire(&(ctl->cmdlock));
    // if(waserror()){
    // freeb(b);
    // qunlock(&ctl->cmdlock);
    // nexterror();
    // }
    memset(b->wp, 0, len);
    mutex_acquire(&ctl->pktlock);
    p = (Sdpcm*) b->wp;
    put2(p->len, len);
    put2(p->lenck, ~len);
    p->seq     = ctl->txseq;
    p->doffset = sizeof(Sdpcm);
    b->wp += sizeof(*p);

    q = (Cmd*) b->wp;
    put4(q->cmd, op);
    put4(q->len, tlen);
    put2(q->flags, write ? 2 : 0);
    put2(q->id, ++ctl->reqid);
    put4(q->status, 0);
    b->wp += sizeof(*q);

    if (dlen > 0)
        memmove(b->wp, data, dlen);
    if (write)
        memmove(b->wp + dlen, res, rlen);
    b->wp += tlen;

    // if(iodebug)
    // dump("cmd", b->rp, len);
    packetrw(1, b->rp, len);
    ctl->txseq++;
    mutex_release(&ctl->pktlock);
    freeb(b);
    b = 0;
    USED(b);
    sleep(&ctl->cmdr, cmddone, ctl);
    // MicroDelay(1000);
    b        = ctl->rsp;
    ctl->rsp = 0;
    if (b == 0) {
        printf("error in command nb == 0 \n");
    }
    // assert(b != nil);
    p = (Sdpcm*) b->rp;
    q = (Cmd*) (b->rp + p->doffset);
    if (q->status[0] | q->status[1] | q->status[2] | q->status[3]) {
        printf("ether4330: cmd %d error status %ld\n", op, get4(q->status));
        dump("ether4330: cmd error", b->rp, BLEN(b));
        printf("wlcmd error");
    }
    if (!write)
        memmove(res, q + 1, rlen);
    freeb(b);
    mutex_release(&ctl->cmdlock);
    // poperror();
}

static void wlsetvar(Ctlr* ctl, char* name, void* val, int len) {
    // if(VARDEBUG){
    // char buf[32];
    // snprint(buf, sizeof buf, "wlsetvar %s", name);
    // dump(buf, val, len);
    // }
    wlcmd(ctl, 1, SetVar, name, strlen(name) + 1, val, len);
}

static void wlsetint(Ctlr* ctl, char* name, int val) {
    uint8_t buf[4];

    put4(buf, val);
    wlsetvar(ctl, name, buf, 4);
}

static void wlcmdint(Ctlr* ctl, int op, int val) {
    uint8_t buf[4];

    put4(buf, val);
    wlcmd(ctl, 1, op, buf, 4, 0, 0);
}

static void wlscanstart(Ctlr* ctl) {
    /* version[4] action[2] sync_id[2] ssidlen[4] ssid[32] bssid[6] bss_type[1]
        scan_type[1] nprobes[4] active_time[4] passive_time[4] home_time[4]
        nchans[2] nssids[2] chans[nchans][2] ssids[nssids][32] */
    /* hack - this is only correct on a little-endian cpu */
    static uint8_t params[4 + 2 + 2 + 4 + 32 + 6 + 1 + 1 + 4 * 4 + 2 + 2 + 14 * 2 + 32 + 4] = {
        1,    0,    0,    0,    1,    0,    0x34, 0x12, 0,    0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 2,
        0,    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        14,   0,    1,    0,    0x01, 0x2b, 0x02, 0x2b, 0x03, 0x2b, 0x04, 0x2b, 0x05, 0x2e, 0x06, 0x2e, 0x07,
        0x2e, 0x08, 0x2b, 0x09, 0x2b, 0x0a, 0x2b, 0x0b, 0x2b, 0x0c, 0x2b, 0x0d, 0x2b, 0x0e, 0x2b, 0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
        0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    };

    wlcmdint(ctl, 49, 0); /* PASSIVE_SCAN */
    wlsetvar(ctl, "escan", params, sizeof params);
}

static void lproc(void* a) {
    Ether* edev;
    Ctlr* ctlr;
    int secs;

    edev = a;
    ctlr = edev->ctlr;
    secs = 0;
    // printf("%s: %d edev: %x \n", __FILE__, __LINE__, edev);
    for (;;) {
        // printf("In LPROC \n");
        tsleep(&up->sleep, return0, 0, 1000);
        // MicroDelay(1000);
        if (ctlr->scansecs) {
            if (secs == 0) {
                // if(waserror())
                // 	ctlr->scansecs = 0;
                // else{
                printf("Stating scans: \n");
                wlscanstart(ctlr);
                printf("scan ended: \n");
                // poperror();
                // }
                secs = ctlr->scansecs;
            }
            --secs;
        } else {
            secs = 0;
        }
    }
    terminate_this_task();
}

static void wlgetvar(Ctlr* ctl, char* name, void* val, int len) {
    wlcmd(ctl, 0, GetVar, name, strlen(name) + 1, val, len);
}

static void wlinit(Ether* edev, Ctlr* ctlr) {
    uint8_t ea[Eaddrlen];
    uint8_t eventmask[16];
    char version[128];
    char* p;
    static uint8_t keepalive[12] = {1, 0, 11, 0, 0xd8, 0xd6, 0, 0, 0, 0, 0, 0};

    wlgetvar(ctlr, "cur_etheraddr", ea, Eaddrlen);
    memmove(edev->ea, ea, Eaddrlen);
    memmove(edev->addr, ea, Eaddrlen);
    printf("ether4330: addr %02X:%02X:%02X:%02X:%02X:%02X\n", ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);
    wlsetint(ctlr, "assoc_listen", 10);
    if (ctlr->chipid == 43430 || ctlr->chipid == 0x4345)
        wlcmdint(ctlr, 0x56, 0); /* powersave off */
    else
        wlcmdint(ctlr, 0x56, 2); /* powersave FAST */
    wlsetint(ctlr, "bus:txglom", 0);
    wlsetint(ctlr, "bcn_timeout", 10);
    wlsetint(ctlr, "assoc_retry_max", 3);
    if (ctlr->chipid == 0x4330) {
        wlsetint(ctlr, "btc_wire", 4);
        wlsetint(ctlr, "btc_mode", 1);
        wlsetvar(ctlr, "mkeep_alive", keepalive, 11);
    }
    memset(eventmask, 0xFF, sizeof eventmask);
#define ENABLE(n)  eventmask[n / 8] |= 1 << (n % 8)
#define DISABLE(n) eventmask[n / 8] &= ~(1 << (n % 8))
    DISABLE(40);  /* E_RADIO */
    DISABLE(44);  /* E_PROBREQ_MSG */
    DISABLE(54);  /* E_IF */
    DISABLE(71);  /* E_PROBRESP_MSG */
    DISABLE(20);  /* E_TXFAIL */
    DISABLE(124); /* ? */
    wlsetvar(ctlr, "event_msgs", eventmask, sizeof eventmask);
    wlcmdint(ctlr, 0xb9, 0x28);  /* SET_SCAN_CHANNEL_TIME */
    wlcmdint(ctlr, 0xbb, 0x28);  /* SET_SCAN_UNASSOC_TIME */
    wlcmdint(ctlr, 0x102, 0x82); /* SET_SCAN_PASSIVE_TIME */
    wlcmdint(ctlr, 2, 0);        /* UP */
    memset(version, 0, sizeof version);
    wlgetvar(ctlr, "ver", version, sizeof version - 1);
    if ((p = strchr(version, '\n')) != 0)
        *p = '\0';
    if (0)
        printf("ether4330: %s\n", version);
    wlsetint(ctlr, "roam_off", 1);
    wlcmdint(ctlr, 0x14, 1); /* SET_INFRA 1 */
    wlcmdint(ctlr, 10, 0);   /* SET_PROMISC */
    // wlcmdint(ctlr, 0x8e, 0);	/* SET_BAND 0 */
    // wlsetint(ctlr, "wsec", 1);
    wlcmdint(ctlr, 2, 1); /* UP */
    ctlr->keys[0].len = WMinKeyLen;
    // wlwepkey(ctlr, 0);
}

void etherbcmattach(Ether* edev) {
    Ctlr* ctlr1 = edev->ctlr;
    mutex_acquire(&ctlr1->alock);
    if (ctlr1->chipid == 0) {
        sdioinit();
        sbinit(ctlr1);
    }
    fwload(ctlr1);
    sbenable(ctlr1);
    printf("rproc : %x \n", (uint32_t) &rproc);
    printf("lproc : %x \n", (uint32_t) &lproc);
    create_task("wfread", (uint32_t) &rproc, (uint32_t) edev);  //"wifireader",
    create_task("wftimer", (uint32_t) &lproc, (uint32_t) edev); // wifitimer
    wlinit(edev, ctlr1);
    ctlr1->edev = edev;
    mutex_release(&ctlr1->alock);
}

static void etherbcmtransmit(Ether* edev) {
    Ctlr* ctlr;

    ctlr = edev->ctlr;
    if (ctlr == NULL) {
        return;
    }
    txstart(edev);
}

long readstr(uint32_t offset, void* buf, size_t len, const void* p) {
    const char* p1 = (const char*) p;

    size_t plen = strlen(p1);
    if (offset >= plen) {
        return 0;
    }

    p1 += offset;

    char* p2    = (char*) buf;
    long result = 0;

    while (*p1 != '\0' && len > 0) {
        *p2++ = *p1++;

        result++;
        len--;
    }

    return result;
}

#define READSTR 1000

static long etherbcmifstat(Ether* edev, void* a, long n, uint32_t offset) {
    USED(a);
    USED(offset);

    static char* cryptoname[4] = {
        [0] "off",
        [Wep] "wep",
        [Wpa] "wpa",
        [Wpa2] "wpa2",
    };
    /* these strings are known by aux/wpa */
    static char* connectstate[] = {
        [Disconnected] = "unassociated",
        [Connecting]   = "connecting",
        [Connected]    = "associated",
    };

    Ctlr* ctlr = edev->ctlr;
    if (ctlr == NULL) {
        return 0;
    }

    printf("WIFI STATUS: \n");
    printf("channel: %d\n", ctlr->chanid);
    printf("essid: %s\n", ctlr->essid);
    printf("crypt: %s\n", cryptoname[ctlr->cryptotype]);
    printf("oq: %d\n", qlen(edev->oq));
    printf("txwin: %d\n", ctlr->txwindow);
    printf("txseq: %d\n", ctlr->txseq);
    printf("status: %s\n", connectstate[ctlr->status]);

    return n;
}

Cmdbuf* parsecmd(const void* str, long n) // TODO: allow "param"
{
    Cmdbuf* pCmdbuf = (Cmdbuf*) kernel_allocate(sizeof(Cmdbuf));
    // assert (pCmdbuf != 0);
    USED(n);
    strncpy(pCmdbuf->buf, (const char*) str, sizeof pCmdbuf->buf - 1);
    pCmdbuf->buf[sizeof pCmdbuf->buf - 1] = '\0';

    pCmdbuf->argc = 0;

    char* pSavePtr;
    for (unsigned i = 0; i < nelem(pCmdbuf->f); i++) {
        char* p       = strtok_r(i == 0 ? pCmdbuf->buf : 0, " \t\n", &pSavePtr);
        pCmdbuf->f[i] = p;
        if (p == 0) {
            break;
        }

        pCmdbuf->argc++;
    }

    return pCmdbuf;
}

Cmdtab* lookupcmd(Cmdbuf* cb, Cmdtab* ct, size_t nelem) {
    if (cb->argc == 0) {
        // cmderror (cb, "Command expected");
        printf("Command expected pass null \n");
    }

    for (unsigned i = 0; i < nelem; i++, ct++) {
        if (strcasecmp(cb->f[0], ct->cmd) == 0) {
            return ct;
        }
    }

    printf("Invalid command \n");

    return 0;
}

static void setauth(Ctlr* ctlr, Cmdbuf* cb, char* a) {
    USED(cb);
    uint8_t wpaie[32];
    int i;

    i = parsehex((char*) wpaie, sizeof wpaie, a);
    printf(" i : %d a:%s \n", i, a);

    if (i < 2 || i != wpaie[1] + 2) {
        printf("bad wpa ie syntax \n");
    }

    if (wpaie[0] == 0xdd) {
        ctlr->cryptotype = Wpa;
    } else if (wpaie[0] == 0x30) {
        ctlr->cryptotype = Wpa2;
    } else {
        printf("bad wpa ie \n");
    }

    wlsetvar(ctlr, "wpaie", wpaie, i);
    if (ctlr->cryptotype == Wpa) {
        wlsetint(ctlr, "wpa_auth", 4 | 2); /* auth_psk | auth_unspecified */
        wlsetint(ctlr, "auth", 0);
        wlsetint(ctlr, "wsec", 2);     /* tkip */
        wlsetint(ctlr, "wpa_auth", 4); /* auth_psk */
    } else {
        wlsetint(ctlr, "wpa_auth", 0x80 | 0x40); /* auth_psk | auth_unspecified */
        wlsetint(ctlr, "auth", 0);
        wlsetint(ctlr, "wsec", 4);        /* aes */
        wlsetint(ctlr, "wpa_auth", 0x80); /* auth_psk */
    }
}

#define cistrcmp  strcasecmp
#define cistrncmp strncasecmp

static int setcrypt(Ctlr* ctlr, Cmdbuf* cb, char* a) {
    USED(cb);
    if (cistrcmp(a, "wep") == 0 || cistrcmp(a, "on") == 0)
        ctlr->cryptotype = Wep;
    else if (cistrcmp(a, "off") == 0 || cistrcmp(a, "none") == 0)
        ctlr->cryptotype = 0;
    else
        return 0;
    wlsetint(ctlr, "auth", ctlr->cryptotype);
    return 1;
}

static void wlwepkey(Ctlr* ctl, int i) {
    uint8_t params[164];
    uint8_t* p;

    memset(params, 0, sizeof params);
    p = params;
    p = put4(p, i); /* index */
    p = put4(p, ctl->keys[i].len);
    memmove(p, ctl->keys[i].dat, ctl->keys[i].len);
    p += 32 + 18 * 4; /* keydata, pad */
    if (ctl->keys[i].len == WMinKeyLen)
        p = put4(p, 1); /* algo = WEP1 */
    else
        p = put4(p, 3); /* algo = WEP128 */
    put4(p, 2);         /* flags = Primarykey */

    wlsetvar(ctl, "wsec_key", params, sizeof params);
}

uint64_t strtoull(const char* pString, char** ppEndPtr, int nBase) {
    uint64_t ullResult = 0;
    uint64_t ullPrevResult;
    int bMinus = 0;
    int bFirst = 1;

    if (ppEndPtr != 0) {
        *ppEndPtr = (char*) pString;
    }

    if (nBase != 0 && (nBase < 2 || nBase > 36)) {
        return ullResult;
    }

    int c;
    while ((c = *pString) == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v') {
        pString++;
    }

    if (*pString == '+' || *pString == '-') {
        if (*pString++ == '-') {
            bMinus = 1;
        }
    }

    if (*pString == '0') {
        pString++;

        if (*pString == 'x' || *pString == 'X') {
            if (nBase != 0 && nBase != 16) {
                return ullResult;
            }

            nBase = 16;

            pString++;
        } else {
            if (nBase == 0) {
                nBase = 8;
            }
        }
    } else {
        if (nBase == 0) {
            nBase = 10;
        }
    }

    while (1) {
        int c = *pString;

        if (c < '0') {
            break;
        }

        if ('a' <= c && c <= 'z') {
            c -= 'a' - 'A';
        }

        if (c >= 'A') {
            c -= 'A' - '9' - 1;
        }

        c -= '0';

        if (c >= nBase) {
            break;
        }

        ullPrevResult = ullResult;

        ullResult *= nBase;
        ullResult += c;

        if (ullResult < ullPrevResult) {
            ullResult = (uint32_t) -1;

            if (ppEndPtr != 0) {
                *ppEndPtr = (char*) pString;
            }

            return ullResult;
        }

        pString++;
        bFirst = 0;
    }

    if (ppEndPtr != 0) {
        *ppEndPtr = (char*) pString;
    }

    if (bFirst) {
        return ullResult;
    }

    if (bMinus) {
        ullResult = -ullResult;
    }

    return ullResult;
}

static int wpaparsekey(WKey* key, uint64_t* ivp, char* a) {
    int len;
    char* e;

    if (cistrncmp(a, "tkip:", 5) == 0 || cistrncmp(a, "ccmp:", 5) == 0)
        a += 5;
    else
        return 1;
    len = parsehex(key->dat, sizeof(key->dat), a);
    if (len <= 0)
        return 1;
    key->len = len;
    a += 2 * len;
    if (*a++ != '@')
        return 1;
    *ivp = strtoull(a, &e, 16);
    if (e == a)
        return -1;
    return 0;
}

static int wepparsekey(WKey* key, char* a) {
    int i, k, len, n;
    char buf[WMaxKeyLen];

    len = strlen(a);
    if (len == WMinKeyLen || len == WMaxKeyLen) {
        memset(key->dat, 0, sizeof(key->dat));
        memmove(key->dat, a, len);
        key->len = len;

        return 0;
    } else if (len == WMinKeyLen * 2 || len == WMaxKeyLen * 2) {
        k = 0;
        for (i = 0; i < len; i++) {
            if (*a >= '0' && *a <= '9')
                n = *a++ - '0';
            else if (*a >= 'a' && *a <= 'f')
                n = *a++ - 'a' + 10;
            else if (*a >= 'A' && *a <= 'F')
                n = *a++ - 'A' + 10;
            else
                return -1;

            if (i & 1) {
                buf[k] |= n;
                k++;
            } else
                buf[k] = n << 4;
        }

        memset(key->dat, 0, sizeof(key->dat));
        memmove(key->dat, buf, k);
        key->len = k;

        return 0;
    }

    return -1;
}

static void wlwpakey(Ctlr* ctl, int id, uint64_t iv, uint8_t* ea) {
    uint8_t params[164];
    uint8_t* p;
    int pairwise;

    if (id == CMrxkey)
        return;
    pairwise = (id == CMrxkey || id == CMtxkey);
    memset(params, 0, sizeof params);
    p = params;
    if (pairwise)
        p = put4(p, 0);
    else
        p = put4(p, id - CMrxkey0); /* group key id */
    p = put4(p, ctl->keys[0].len);
    memmove((char*) p, ctl->keys[0].dat, ctl->keys[0].len);
    p += 32 + 18 * 4; /* keydata, pad */
    if (ctl->cryptotype == Wpa)
        p = put4(p, 2); /* algo = TKIP */
    else
        p = put4(p, 4); /* algo = AES_CCM */
    if (pairwise)
        p = put4(p, 0);
    else
        p = put4(p, 2); /* flags = Primarykey */
    p += 3 * 4;
    p = put4(p, 0); // pairwise);		/* iv initialised */
    p += 4;
    p = put4(p, iv >> 16);    /* iv high */
    p = put2(p, iv & 0xFFFF); /* iv low */
    p += 2 + 2 * 4;           /* align, pad */
    if (pairwise)
        memmove(p, ea, Eaddrlen);

    wlsetvar(ctl, "wsec_key", params, sizeof params);
}

static int joindone(void* a) {
    return ((Ctlr*) a)->joinstatus;
}

static int waitjoin(Ctlr* ctl) {
    int n;

    sleep(&ctl->joinr, joindone, ctl);
    // MicroDelay(4000000);
    n               = ctl->joinstatus;
    ctl->joinstatus = 0;
    return n - 1;
}

static void wljoin(Ctlr* ctl, char* ssid, int chan) {
    uint8_t params[72];
    uint8_t* p;
    int n;
    printf("Sending wl join to essid: %s \n", ssid);
    if (chan != 0)
        chan |= 0x2b00; /* 20Mhz channel width */
    p = params;
    n = strlen(ssid);
    n = MIN(n, 32);
    p = put4(p, n);
    memmove(p, ssid, n);
    memset(p + n, 0, 32 - n);
    p += 32;
    p = put4(p, 0xff); /* scan type */
    if (chan != 0) {
        p = put4(p, 2);   /* num probes */
        p = put4(p, 120); /* active time */
        p = put4(p, 390); /* passive time */
    } else {
        p = put4(p, -1); /* num probes */
        p = put4(p, -1); /* active time */
        p = put4(p, -1); /* passive time */
    }
    p = put4(p, -1);           /* home time */
    memset(p, 0xFF, Eaddrlen); /* bssid */
    p += Eaddrlen;
    p = put2(p, 0); /* pad */
    if (chan != 0) {
        p = put4(p, 1);    /* num chans */
        p = put2(p, chan); /* chan spec */
        p = put2(p, 0);    /* pad */
        // assert(p == params + sizeof(params));
        if (p != params + sizeof(params)) {
            printf("Param and pointer error . \n");
        }
    } else {
        p = put4(p, 0); /* num chans */
        // assert(p == params + sizeof(params) - 4);
        if (p != params + sizeof(params) - 4) {
            printf("Pointer error: \n");
        }
    }

    printf("Wifi sending join command \n");
    wlsetvar(ctl, "join", params, chan ? sizeof params : sizeof params - 4);
    printf("Wifi sending join command completed \n");
    ctl->status = Connecting;
    printf("Called waitjoin \n");
    int res1 = waitjoin(ctl);
    printf("Called waitjoin completed resp: %x \n", res1);
    switch (res1) {
    case 0:
        ctl->status = Connected;
        printf("Connected to %s \n", ctl->essid);
        break;
    case 3:
        ctl->status = Disconnected;
        printf("wifi join: network not found");
        break;
    case 1:
        ctl->status = Disconnected;
        printf("wifi join: failed");
        break;
    default:
        ctl->status = Disconnected;
        printf("wifi join: error : %x \n", res1);
    }
}
static long etherbcmctl(Ether* edev, void* buf, long n) {
    Ctlr* ctlr;
    Cmdbuf* cb;
    Cmdtab* ct;
    uint8_t ea[Eaddrlen];
    uint64_t iv;
    int i;
    // printf("%s: %d edev: %x \n", __FILE__, __LINE__, edev);
    if ((ctlr = edev->ctlr) == NULL) {
        printf("Error ctrl null \n");
        return 0l;
    }
    USED(ctlr);

    cb = parsecmd(buf, n);
    // if(waserror()){
    // 	free(cb);
    // 	nexterror();
    // }
    ct = lookupcmd(cb, cmds, nelem(cmds));
    switch (ct->index) {
    case CMauth:
        setauth(ctlr, cb, cb->f[1]);
        if (ctlr->essid[0]) {
            wljoin(ctlr, ctlr->essid, ctlr->chanid);
        }
        printf("CMAUTH \n");
        break;
    case CMchannel:
        if ((i = atoi(cb->f[1])) < 0 || i > 16) {
            printf("bad channel number");
            return 0;
        }

        // wlcmdint(ctlr, 30, i);	/* SET_CHANNEL */
        ctlr->chanid = i;
        printf("CHMCHANNEL \n");
        break;
    case CMcrypt:
        if (setcrypt(ctlr, cb, cb->f[1])) {
            if (ctlr->essid[0]) {
                wljoin(ctlr, ctlr->essid, ctlr->chanid);
            }
        } else {
            printf("bad crypt type \n");
            return 0;
        }
        printf("CMCRYPT \n");
        break;
    case CMessid:
        if (cistrcmp(cb->f[1], "default") == 0)
            memset(ctlr->essid, 0, sizeof(ctlr->essid));
        else {
            strncpy(ctlr->essid, cb->f[1], sizeof(ctlr->essid) - 1);
            ctlr->essid[sizeof(ctlr->essid) - 1] = '\0';
        }
        // if(!waserror()){
        wljoin(ctlr, ctlr->essid, ctlr->chanid);
        // poperror();
        // }
        printf("CMESSID \n");
        break;
    case CMjoin:                         /* join essid channel wep|on|off|wpakey */
        if (strcmp(cb->f[1], "") != 0) { /* empty string for no change */
            if (cistrcmp(cb->f[1], "default") != 0) {
                strncpy(ctlr->essid, cb->f[1], sizeof(ctlr->essid) - 1);
                ctlr->essid[sizeof(ctlr->essid) - 1] = 0;
            } else {
                memset(ctlr->essid, 0, sizeof(ctlr->essid));
            }
        } else if (ctlr->essid[0] == 0) {
            printf("essid not set \n");
            return 0;
        }

        if ((i = atoi(cb->f[2])) >= 0 && i <= 16)
            ctlr->chanid = i;
        else {
            printf("bad channel number");
            return 0;
        }

        if (!setcrypt(ctlr, cb, cb->f[3]))
            setauth(ctlr, cb, cb->f[3]);
        if (ctlr->essid[0])
            wljoin(ctlr, ctlr->essid, ctlr->chanid);

        printf("CMJOIN \n");
        break;
    case CMkey1:
    case CMkey2:
    case CMkey3:
    case CMkey4:
        i = ct->index - CMkey1;
        if (wepparsekey(&ctlr->keys[i], cb->f[1])) {
            printf("bad WEP key syntax");
            return 0;
        }

        wlsetint(ctlr, "wsec", 1); /* wep enabled */
        wlwepkey(ctlr, i);
        printf("CMKEY1234 \n");
        break;
    case CMrxkey:
    case CMrxkey0:
    case CMrxkey1:
    case CMrxkey2:
    case CMrxkey3:
    case CMtxkey:
        if (parseether(ea, cb->f[1]) < 0) {
            printf("bad ether addr \n");
            return 0;
        }
        if (wpaparsekey(&ctlr->keys[0], &iv, cb->f[2])) {
            printf("bad wpa key \n");
            return 0;
        }

        wlwpakey(ctlr, ct->index, iv, ea);
        printf("CMRXKEY123 \n");
        break;
    case CMdebug:
        // iodebug = atoi(cb->f[1]);
        printf("iodebug %d \n", cb->f[1]);
        break;
    }
    // poperror();
    kernel_deallocate(cb);
    return n;
}

static void etherbcmscan(void* a, uint32_t secs) {
    Ether* edev;
    Ctlr* ctlr;

    edev           = a;
    ctlr           = edev->ctlr;
    ctlr->scansecs = secs;
}

static void etherbcmshutdown(Ether* edev) {
    USED(edev);
    sdiowr(Fn0, Ioabort, 1 << 3); /* reset */
}

static void etherbcmsetevhndlr(struct Ether* edev, ether_event_handler_t* hndlr, void* context) {
    Ctlr* ctlr;

    ctlr            = edev->ctlr;
    ctlr->evcontext = context;
    ctlr->evhndlr   = hndlr;
}

static void callevhndlr(Ctlr* ctlr, ether_event_type_t type, const ether_event_params_t* params) {
    if (ctlr->evhndlr != 0) {
        (*ctlr->evhndlr)(type, params, ctlr->evcontext);
    }
}

int etherbcmpnp(Ether* edev) {
    Ctlr* ctlr = kernel_allocate(sizeof(Ctlr));

    memset(ctlr, 0, sizeof(Ctlr));
    ctlr->chanid = Wifichan;
    mutex_init(&(ctlr->cmdlock));
    mutex_init(&(ctlr->pktlock));
    mutex_init(&(ctlr->tlock));
    mutex_init(&(ctlr->alock));
    mutex_init(&sdiolock);

    edev->ctlr       = ctlr;
    edev->attach     = etherbcmattach;
    edev->transmit   = etherbcmtransmit;
    edev->setevhndlr = etherbcmsetevhndlr;
    edev->ifstat     = etherbcmifstat;
    edev->ctl        = etherbcmctl;
    edev->scanbs     = etherbcmscan;
    edev->shutdown   = etherbcmshutdown;
    edev->arg        = edev;
    return 0;
}