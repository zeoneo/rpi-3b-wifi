#ifndef RPI_INTERRUPTS_H
#define RPI_INTERRUPTS_H

#include <stdint.h>
#include <device/uart0.h>
#include <kernel/rpi-interrupts.h>
#include "rpi-base.h"

#define RPI_INTERRUPT_CONTROLLER_BASE (PERIPHERAL_BASE + 0xB200)

typedef void (*interrupt_handler_f)(void);
typedef void (*interrupt_clearer_f)(void);

#define ARM_IRQ1_BASE 0
#define ARM_IRQ0_BASE 64

#define INTERRUPT_DMA0 (ARM_IRQ1_BASE + 16)
#define INTERRUPT_DMA1 (ARM_IRQ1_BASE + 17)
#define INTERRUPT_DMA2 (ARM_IRQ0_BASE + 13)
#define INTERRUPT_DMA3 (ARM_IRQ0_BASE + 14)
#define INTERRUPT_DMA4 (ARM_IRQ1_BASE + 20)
#define INTERRUPT_DMA5 (ARM_IRQ1_BASE + 21)
#define INTERRUPT_DMA6 (ARM_IRQ1_BASE + 22)
#define INTERRUPT_DMA7 (ARM_IRQ1_BASE + 23)
#define INTERRUPT_DMA8 (ARM_IRQ1_BASE + 24)
#define INTERRUPT_DMA9 (ARM_IRQ1_BASE + 25)
#define INTERRUPT_DMA10 (ARM_IRQ1_BASE + 26)
#define INTERRUPT_DMA11 (ARM_IRQ1_BASE + 27)
#define INTERRUPT_DMA12 (ARM_IRQ1_BASE + 28)

#define IRQ_BASIC 0x3F00B200
#define IRQ_PEND1 0x3F00B204
#define IRQ_PEND2 0x3F00B208
#define IRQ_FIQ_CONTROL 0x3F00B210
#define IRQ_ENABLE_BASIC 0x3F00B218
#define IRQ_DISABLE_BASIC 0x3F00B224

typedef enum
{
    RPI_BASIC_ARM_TIMER_IRQ = (1 << 0),
    RPI_BASIC_ARM_MAILBOX_IRQ = (1 << 1),
    RPI_BASIC_ARM_DOORBELL_0_IRQ = (1 << 2),
    RPI_BASIC_ARM_DOORBELL_1_IRQ = (1 << 3),
    RPI_BASIC_GPU_0_HALTED_IRQ = (1 << 4),
    RPI_BASIC_GPU_1_HALTED_IRQ = (1 << 5),
    RPI_BASIC_ACCESS_ERROR_1_IRQ = (1 << 6),
    RPI_BASIC_ACCESS_ERROR_0_IRQ = (1 << 7),
    IRQ_SDHOST = 56
} irq_number_t;

typedef struct __attribute__((packed, aligned(4)))
{
    volatile uint32_t IRQ_basic_pending;
    volatile uint32_t IRQ_pending_1;
    volatile uint32_t IRQ_pending_2;
    volatile uint32_t FIQ_control;
    volatile uint32_t Enable_IRQs_1;
    volatile uint32_t Enable_IRQs_2;
    volatile uint32_t Enable_Basic_IRQs;
    volatile uint32_t Disable_IRQs_1;
    volatile uint32_t Disable_IRQs_2;
    volatile uint32_t Disable_Basic_IRQs;
} rpi_irq_controller_t;

extern rpi_irq_controller_t *RPI_GetIrqController(void);
extern void _enable_interrupts();

__inline__ int32_t INTERRUPTS_ENABLED(void)
{
    int32_t res;
    __asm__ __volatile__("mrs %[res], CPSR"
                         : [res] "=r"(res)::);
    return ((res >> 7) & 1) == 0;
}

__inline__ void ENABLE_INTERRUPTS(void)
{
    if (!INTERRUPTS_ENABLED())
    {
        __asm__ __volatile__("cpsie i");
        // _enable_interrupts();
        // __asm__ __volatile__("cpsie i");
    }
}

__inline__ void DISABLE_INTERRUPTS(void)
{
    if (INTERRUPTS_ENABLED())
    {
        __asm__ __volatile__("cpsid i");
    }
}

void interrupts_init(void);
void register_irq_handler(irq_number_t irq_num, interrupt_handler_f handler, interrupt_clearer_f clearer);
void unregister_irq_handler(irq_number_t irq_num);

#endif
