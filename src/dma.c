#include <device/dma.h>
#include <kernel/rpi-interrupts.h>
#include <kernel/systimer.h>
#include <mem/kernel_alloc.h>
#include <plibc/stdio.h>

extern int dma_src_page_1;
extern int dma_dest_page_1;
extern int dma_dest_page_2;
extern int dma_cb_page;

#define CB_ALIGN      32
#define CB_ALIGN_MASK (CB_ALIGN - 1)
#define PERMAP_SHIFT  16

extern void PUT32(uint32_t addr, uint32_t value);

typedef struct {
    uint32_t info;
    uint32_t src;
    uint32_t dst;
    uint32_t length;
    uint32_t stride;
    uint32_t next;
    uint32_t pad[2];
} bcm2835_dma_cb;

#define DMACH(n) (0x100 * (n))

typedef struct {
    uint32_t
        CS; // Control and Status
            // 31    RESET; set to 1 to reset DMA
            // 30    ABORT; set to 1 to abort current DMA control block (next one will be loaded & continue)
            // 29    DISDEBUG; set to 1 and DMA won't be paused when debug signal is sent
            // 28    WAIT_FOR_OUTSTANDING_WRITES; set to 1 and DMA will wait until peripheral says all writes have gone
            // through before loading next CB 24-27 reserved 20-23 PANIC_PRIORITY; 0 is lowest priority 16-19 PRIORITY;
            // bus scheduling priority. 0 is lowest 9-15  reserved 8     ERROR; read as 1 when error is encountered.
            // error can be found in DEBUG register. 7     reserved 6     WAITING_FOR_OUTSTANDING_WRITES; read as 1 when
            // waiting for outstanding writes 5     DREQ_STOPS_DMA; read as 1 if DREQ is currently preventing DMA 4
            // PAUSED; read as 1 if DMA is paused 3     DREQ; copy of the data request signal from the peripheral, if
            // DREQ is enabled. reads as 1 if data is being requested, else 0 2     INT; set when current CB ends and
            // its INTEN=1. Write a 1 to this register to clear it 1     END; set when the transfer defined by current
            // CB is complete. Write 1 to clear. 0     ACTIVE; write 1 to activate DMA (load the CB before hand)
    uint32_t CONBLK_AD; // Control Block Address
    uint32_t TI;        // transfer information; see DmaControlBlock.TI for description
    uint32_t SOURCE_AD; // Source address
    uint32_t DEST_AD;   // Destination address
    uint32_t TXFR_LEN;  // transfer length.
    uint32_t STRIDE;    // 2D Mode Stride. Only used if TI.TDMODE = 1
    uint32_t NEXTCONBK; // Next control block. Must be 256-bit aligned (32 bytes; 8 words)
    uint32_t DEBUG;     // controls debug settings
} dma_channel_header;

#define BCM2835_DMA_MAX_DMA_CHAN_SUPPORTED 14
#define BCM2835_DMA_CHAN_NAME_SIZE         8
#define BCM2835_DMA_BULK_MASK              BIT(0)

#define BCM2835_DMA_CS        0x00
#define BCM2835_DMA_ADDR      0x04
#define BCM2835_DMA_TI        0x08
#define BCM2835_DMA_SOURCE_AD 0x0c
#define BCM2835_DMA_DEST_AD   0x10
#define BCM2835_DMA_LEN       0x14
#define BCM2835_DMA_STRIDE    0x18
#define BCM2835_DMA_NEXTCB    0x1c
#define BCM2835_DMA_DEBUG     0x20

/* DMA CS Control and Status bits */
#define BCM2835_DMA_ACTIVE   BIT(0) /* activate the DMA */
#define BCM2835_DMA_END      BIT(1) /* current CB has ended */
#define BCM2835_DMA_INT      BIT(2) /* interrupt status */
#define BCM2835_DMA_DREQ     BIT(3) /* DREQ state */
#define BCM2835_DMA_ISPAUSED BIT(4) /* Pause requested or not active */
#define BCM2835_DMA_ISHELD   BIT(5) /* Is held by DREQ flow control */
#define BCM2835_DMA_WAITING_FOR_WRITES                                                                                 \
    BIT(6) /* waiting for last                                                                                         \
            * AXI-write to ack                                                                                         \
            */
#define BCM2835_DMA_ERR               BIT(8)
#define BCM2835_DMA_PRIORITY(x)       ((x & 15) << 16) /* AXI priority */
#define BCM2835_DMA_PANIC_PRIORITY(x) ((x & 15) << 20) /* panic priority */
/* current value of TI.BCM2835_DMA_WAIT_RESP */
#define BCM2835_DMA_WAIT_FOR_WRITES BIT(28)
#define BCM2835_DMA_DIS_DEBUG       BIT(29) /* disable debug pause signal */
#define BCM2835_DMA_ABORT           BIT(30) /* Stop current CB, go to next, WO */
#define BCM2835_DMA_RESET           BIT(31) /* WO, self clearing */

/* Transfer information bits - also bcm2835_cb.info field */
#define BCM2835_DMA_INT_EN          BIT(0)
#define BCM2835_DMA_TDMODE          BIT(1)  /* 2D-Mode */
#define BCM2835_DMA_WAIT_RESP       BIT(3)  /* wait for AXI-write to be acked */
#define BCM2835_DMA_D_INC           BIT(4)  /* Increment destination */
#define BCM2835_DMA_D_WIDTH         BIT(5)  /* 128bit writes if set */
#define BCM2835_DMA_D_DREQ          BIT(6)  /* enable DREQ for destination */
#define BCM2835_DMA_D_IGNORE        BIT(7)  /* ignore destination writes */
#define BCM2835_DMA_S_INC           BIT(8)  /* Increment Source */
#define BCM2835_DMA_S_WIDTH         BIT(9)  /* 128bit writes if set */
#define BCM2835_DMA_S_DREQ          BIT(10) /* enable SREQ for source */
#define BCM2835_DMA_S_IGNORE        BIT(11) /* ignore source reads - read 0 */
#define BCM2835_DMA_BURST_LENGTH(x) ((x & 15) << 12)
#define BCM2835_DMA_PER_MAP(x)      ((x & 31) << 16) /* REQ source */
#define BCM2835_DMA_WAIT(x)         ((x & 31) << 21) /* add DMA-wait cycles */
#define BCM2835_DMA_NO_WIDE_BURSTS  BIT(26)          /* no 2 beat write bursts */

/* debug register bits */
#define BCM2835_DMA_DEBUG_CLR_ERRORS               (7 << 0)
#define BCM2835_DMA_DEBUG_LAST_NOT_SET_ERR         BIT(0)
#define BCM2835_DMA_DEBUG_FIFO_ERR                 BIT(1)
#define BCM2835_DMA_DEBUG_READ_ERR                 BIT(2)
#define BCM2835_DMA_DEBUG_OUTSTANDING_WRITES_SHIFT 4
#define BCM2835_DMA_DEBUG_OUTSTANDING_WRITES_BITS  4
#define BCM2835_DMA_DEBUG_ID_SHIFT                 16
#define BCM2835_DMA_DEBUG_ID_BITS                  9
#define BCM2835_DMA_DEBUG_STATE_SHIFT              16
#define BCM2835_DMA_DEBUG_STATE_BITS               9
#define BCM2835_DMA_DEBUG_VERSION_SHIFT            25
#define BCM2835_DMA_DEBUG_VERSION_BITS             3
#define BCM2835_DMA_DEBUG_LITE                     BIT(28)

/* shared registers for all dma channels */
#define BCM2835_DMA_INT_STATUS 0xfe0
#define BCM2835_DMA_ENABLE     0xff0

#define BCM2835_DMA_DATA_TYPE_S8   1
#define BCM2835_DMA_DATA_TYPE_S16  2
#define BCM2835_DMA_DATA_TYPE_S32  4
#define BCM2835_DMA_DATA_TYPE_S128 16

/* Valid only for channels 0 - 14, 15 has its own base address */
#define BCM2835_DMA_CHAN(n)         ((n) << 8) /* Base address */
#define BCM2835_DMA_CHANIO(base, n) ((base) + BCM2835_DMA_CHAN(n))

// WHEN L2 cache is enabled
// #define PHYS_TO_DMA(x) (x | 0X40000000)
// #define DMA_TO_PHYS(x) (x & ~(0X40000000))

// See: BCM2835-ARM-Peripherals.pdf

// BCM2836_VCBUS_0_ALIAS = $00000000;	0 Alias - L1 and L2 cached
// BCM2836_VCBUS_4_ALIAS = $40000000;	4 Alias - L2 cache coherent (non allocating)
// BCM2836_VCBUS_8_ALIAS = $80000000;	8 Alias - L2 cached (only)
// BCM2836_VCBUS_C_ALIAS = $C0000000; {C Alias - Direct uncached}	Suitable for RPi 2 Model B
// pass integer
#define PMEM_TO_DMA(x) (x | 0XC0000000)
#define DMA_TO_PMEM(x) (x & ~(0XC0000000))

// BCM2836 peripherals BCM2836_PERIPHERALS_*
// See: BCM2835-ARM-Peripherals.pdf

// BCM2836_PERIPHERALS_BASE = $3F000000;	Mapped to VC address 7E000000
// BCM2836_PERIPHERALS_SIZE = SIZE_16M;
// Pass integer
// Arm phys address to DMA address (also called bus address)
#define PIO_TO_DMA(x) (x - 0x3F000000 + 0X7E000000)

// DMA addr to arm phys address
#define DMA_TO_PIO(x) (x - 0X7E000000 + 0x3F000000)

// This will convert phys_io addr to bus io address
// 0x3F200000 - 0x3F000000 + 0X7E000000 = 0X7E200000

// We will only use 13 dma channels
static uint32_t dma_ints[13] = {INTERRUPT_DMA0,  INTERRUPT_DMA1,  INTERRUPT_DMA2, INTERRUPT_DMA3, INTERRUPT_DMA4,
                                INTERRUPT_DMA5,  INTERRUPT_DMA6,  INTERRUPT_DMA7, INTERRUPT_DMA8, INTERRUPT_DMA9,
                                INTERRUPT_DMA10, INTERRUPT_DMA11, INTERRUPT_DMA12};

typedef enum { DMA_NEED_INIT = 0, DMA_READY = 1, DMA_IN_PROGRESS = 2, DMA_END = 3 } dma_channel_status;

typedef struct {
    dma_channel_header* channel_header;
    bcm2835_dma_cb* cb;
    uint8_t* cb_alloc_ptr; // This is used to point original allocated ptr
    dma_channel_status channel_status;
} dma_ctrl;

static volatile dma_ctrl dma[13] = {0};

static void dma_irq_clearer(uint32_t chan) {
    volatile dma_ctrl* ctrl  = (void*) &dma[chan];
    ctrl->channel_header->CS = BCM2835_DMA_INT;
    ctrl->channel_status     = DMA_END;
}

static void dma_0_irq_clearer(void) {
    dma_irq_clearer(0);
}
static void dma_1_irq_clearer(void) {
    dma_irq_clearer(1);
}
static void dma_2_irq_clearer(void) {
    dma_irq_clearer(2);
}
static void dma_3_irq_clearer(void) {
    dma_irq_clearer(3);
}
static void dma_4_irq_clearer(void) {
    dma_irq_clearer(4);
}
static void dma_5_irq_clearer(void) {
    dma_irq_clearer(5);
}
static void dma_6_irq_clearer(void) {
    dma_irq_clearer(6);
}
static void dma_7_irq_clearer(void) {
    dma_irq_clearer(7);
}
static void dma_8_irq_clearer(void) {
    dma_irq_clearer(8);
}
static void dma_9_irq_clearer(void) {
    dma_irq_clearer(9);
}
static void dma_10_irq_clearer(void) {
    dma_irq_clearer(10);
}
static void dma_11_irq_clearer(void) {
    dma_irq_clearer(11);
}
static void dma_12_irq_clearer(void) {
    dma_irq_clearer(12);
}

typedef void (*clearer_callback_t)();

static clearer_callback_t dma_clearer[13] = {
    dma_0_irq_clearer,  dma_1_irq_clearer,  dma_2_irq_clearer, dma_3_irq_clearer, dma_4_irq_clearer,
    dma_5_irq_clearer,  dma_6_irq_clearer,  dma_7_irq_clearer, dma_8_irq_clearer, dma_9_irq_clearer,
    dma_10_irq_clearer, dma_11_irq_clearer, dma_12_irq_clearer};

static void dma_irq_handler(void) {
    // printf("\nDMA irq handler called. This should be called after clearer. \n");
}

void print_area(char* src_addr, int length) {
    uart_puts("\n ----------------- \n");
    for (int i = 0; i < length; i++) {
        uart_putc(*src_addr++);
    }
    uart_puts("\n ----------------- \n");
}

void copy(char* src_addr, char* dst_addr, int length) {
    while (length > 0) {
        *dst_addr = *src_addr;
        src_addr++;
        dst_addr++;
        length--;
    }
}

void writeBitmasked(volatile uint32_t* dest, uint32_t mask, uint32_t value) {
    uint32_t cur = *dest;
    uint32_t new = (cur & (~mask)) | (value & mask);
    *dest        = new;
    *dest        = new; // added safety for when crossing memory barriers.
}

void show_dma_demo() {

    char data_array[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'Z', 'I', 'J', 'K', '\0'};

    int data_length = sizeof(data_array) / sizeof(char);

    uart_puts("Before setting source address \n");
    char* src_ptr = (char*) &dma_src_page_1;

    print_area((char*) src_ptr, data_length);

    copy(&data_array[0], (char*) &dma_src_page_1, data_length);

    uart_puts("After setting source address \n");
    print_area(src_ptr, data_length);

    uart_puts("Before copying destination 1 lookslike \n");
    print_area((char*) &dma_dest_page_1, data_length);
    uart_puts("Before copying destination 2 lookslike \n");
    print_area((char*) &dma_dest_page_2, data_length);

    uint32_t dma_chan_num = 4;
    dma_start(dma_chan_num, 0, MEM_TO_MEM, &dma_src_page_1, &dma_dest_page_1, 65536);
    dma_wait(dma_chan_num);

    uart_puts("After DMA ops \n");
    uart_puts("After DMA destination 1 lookslike \n");
    print_area((char*) &dma_dest_page_1, data_length);

    dma_chan_num = 5;
    dma_start(dma_chan_num, 0, MEM_TO_MEM, &dma_dest_page_1, &dma_dest_page_2, 65536);
    dma_wait(dma_chan_num);

    uart_puts("After DMA ops \n");
    uart_puts("After DMA destination 2 lookslike \n");
    print_area((char*) &dma_dest_page_2, data_length);
    // uart_puts("After DMA destination lookslike \n");
    // print_area((char *)&dma_dest_page_2, data_length);
}

static void init_channel(volatile dma_ctrl* ctrl, uint32_t chan) {
    ctrl->channel_status = DMA_READY;
    // Initialize channel here
    uint32_t* dmaBaseMem = (void*) DMA_BASE;
    // Enable channel
    writeBitmasked(dmaBaseMem + BCM2835_DMA_ENABLE / 4, 1 << chan, 1 << chan);

    volatile dma_channel_header* channel_header = (dma_channel_header*) (dmaBaseMem + (DMACH(chan)) / 4);

    // dmaBaseMem is a uint32_t ptr, so divide by 4 before adding byte offset
    channel_header->CS = BCM2835_DMA_RESET;

    while (channel_header->CS & BCM2835_DMA_RESET)
        ;

    register_irq_handler(dma_ints[chan], dma_irq_handler, dma_clearer[chan]);

    // Control Block's needs 32 Byte alignment (256 Bits as mentioned in Manual)
    // So we allocate 64 Bytes because even if I allocate memory at 32 Bytes alignment
    // memory allocator header info like size, and magic comes first which will break the alignment.
    void* p              = kernel_allocate(2 * CB_ALIGN);
    ctrl->cb             = (bcm2835_dma_cb*) (((uint32_t) p + CB_ALIGN) & ~CB_ALIGN_MASK);
    ctrl->cb_alloc_ptr   = (uint8_t*) p; // If we ever want to return memory, call mem_deallocate using this.
    ctrl->channel_header = (void*) channel_header;
}

int dma_start(int chan, int dev, DMA_DIR dir, void* src, void* dst, int len) {

    // printf("\t dma_start  src:0x%x dst: 0x%x len: %d \n", src, dst, len);
    volatile dma_ctrl* ctrl = &dma[chan];

    if (ctrl->channel_status == DMA_READY || ctrl->channel_status == DMA_IN_PROGRESS) {
        printf("Channel is busy. %d \n", dev);
        return -1;
    }

    if (ctrl->channel_status == DMA_NEED_INIT) {
        // printf(" DNA NEED INIT \n");
        init_channel(ctrl, chan);
    }

    uint32_t src_addr      = 0;
    uint32_t dest_addr     = 0;
    uint32_t transfer_info = 0;
    switch (dir) {
    case DEV_TO_MEM:
        transfer_info = BCM2835_DMA_S_DREQ | BCM2835_DMA_D_INC;
        src_addr      = (uint32_t) PIO_TO_DMA((uint32_t) src);
        dest_addr     = (uint32_t) PMEM_TO_DMA((uint32_t) dst);
        break;
    case MEM_TO_DEV:
        transfer_info = BCM2835_DMA_D_DREQ | BCM2835_DMA_S_INC;
        src_addr      = (uint32_t) PMEM_TO_DMA((uint32_t) src);
        dest_addr     = (uint32_t) PIO_TO_DMA((uint32_t) dst);
        break;
    case MEM_TO_MEM:
        transfer_info = BCM2835_DMA_S_INC | BCM2835_DMA_D_INC;
        src_addr      = (uint32_t) PMEM_TO_DMA((uint32_t) src);
        dest_addr     = (uint32_t) PMEM_TO_DMA((uint32_t) dst);
        break;
    default:
        break;
    }

    volatile bcm2835_dma_cb* cb = ctrl->cb;
    cb->info                    = transfer_info | dev << PERMAP_SHIFT | BCM2835_DMA_INT_EN;
    cb->src                     = src_addr;
    cb->dst                     = dest_addr;
    cb->length                  = len;
    cb->stride                  = 0x0;
    cb->next                    = 0x0; // last Control block

    ctrl->channel_header->CS = 0;
    MicroDelay(1);

    // clear debug error flags
    ctrl->channel_header->DEBUG =
        BCM2835_DMA_DEBUG_READ_ERR | BCM2835_DMA_DEBUG_FIFO_ERR | BCM2835_DMA_DEBUG_LAST_NOT_SET_ERR;

    // we have to point it to the PHYSICAL address of the control block (cb)
    ctrl->channel_header->CONBLK_AD = (uint32_t) PMEM_TO_DMA((uint32_t) cb);

    uint32_t* dmaBaseMem     = (void*) DMA_BASE;
    dmaBaseMem[0xfe0 >> 2]   = 0; // Interrupt status
    ctrl->channel_header->CS = BCM2835_DMA_INT;
    MicroDelay(2);

    ctrl->channel_header->CS = BCM2835_DMA_ACTIVE;
    // set active bit, but everything else is 0.

    MicroDelay(2);
    return 0;
}

int dma_wait(int chan) {
    volatile dma_ctrl* ctrl = &dma[chan];
    MicroDelay(10000);
    ctrl->channel_status                        = DMA_END;
    volatile dma_channel_header* channel_header = ctrl->channel_header;
    uint32_t status                             = channel_header->CS;

    if ((status & (BCM2835_DMA_ACTIVE | BCM2835_DMA_END | BCM2835_DMA_INT)) != BCM2835_DMA_END) {
        printf("DMA ERROR: Channel status: %x channel_header->DEBUG:%x \n", channel_header->CS, channel_header->DEBUG);
        channel_header->CS          = BCM2835_DMA_RESET;
        ctrl->channel_header->DEBUG = BCM2835_DMA_DEBUG_CLR_ERRORS;
        return -1;
    }
    channel_header->CS = BCM2835_DMA_END | BCM2835_DMA_INT;
    // printf("\t DMA Success: Channel: %d \n", chan);
    //     __asm__ volatile ("dsb sy" : : : "memory");
    // __asm__ volatile ("dmb sy" : : : "memory");
    return 0;
}