#ifndef DMA_H
#define DMA_H
#include <kernel/rpi-base.h>
#include <stdint.h>

// Taken all of the following code is taken from linux source
#define DMA_BASE PERIPHERAL_BASE + 0x7000

#ifndef BIT
#define BIT(nr)		(1 << (nr))
#endif

// For peripheral
// #define TIMER_BASE 0x3F003000
// #define DMA_BASE 0x3F007000
// #define CLOCK_BASE 0x3F101000
// #define GPIO_BASE 0x3F200000
// #define PWM_BASE 0x3F20C000
// #define GPIO_BASE_BUS 0x7E200000 //this is the physical bus address of the GPIO module. This is only used when other peripherals directly connected to the bus (like DMA) need to read/write the GPIOs
// #define PWM_BASE_BUS 0x7E20C000

// This is used when other peripheral which are directly connected to bus
// SPI with DMA, or GPIO with DMA can use this macro later.
#define BUS_TO_PHYS(x) ((x) & ~0xC0000000)

#define GPIO_REGISTER_BASE 0x200000
#define GPIO_SET_OFFSET 0x1C
#define GPIO_CLR_OFFSET 0x28
#define PHYSICAL_GPIO_BUS (0x7E000000 + GPIO_REGISTER_BASE)

typedef enum
{
    DEV_TO_MEM = 0, /* device to memory */
    MEM_TO_DEV = 1, /* memory to device */
    MEM_TO_MEM = 2, /* memory to memory */
} DMA_DIR;

int dma_start(int chan, int dev, DMA_DIR dir, void *src, void *dst, int len);
int dma_wait(int chan);

void show_dma_demo();
#endif