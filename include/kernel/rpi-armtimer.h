#ifndef RPI_ARMTIMER_H
#define RPI_ARMTIMER_H

#include <stdint.h>

#include "rpi-base.h"

#define RPI_ARMTIMER_BASE (PERIPHERAL_BASE + 0xB400)

#define RPI_ARMTIMER_CTRL_23BIT (1 << 1)

#define RPI_ARMTIMER_CTRL_PRESCALE_1 (0 << 2)
#define RPI_ARMTIMER_CTRL_PRESCALE_16 (1 << 2)
#define RPI_ARMTIMER_CTRL_PRESCALE_256 (2 << 2)

#define RPI_ARMTIMER_CTRL_INT_ENABLE (1 << 5)
#define RPI_ARMTIMER_CTRL_INT_DISABLE (0 << 5)

#define RPI_ARMTIMER_CTRL_ENABLE (1 << 7)
#define RPI_ARMTIMER_CTRL_DISABLE (0 << 7)

#define ARM_TIMER_LOD 0x3F00B400
#define ARM_TIMER_VAL 0x3F00B404
#define ARM_TIMER_CTL 0x3F00B408
#define ARM_TIMER_CLI 0x3F00B40C
#define ARM_TIMER_RIS 0x3F00B410
#define ARM_TIMER_MIS 0x3F00B414
#define ARM_TIMER_RLD 0x3F00B418
#define ARM_TIMER_DIV 0x3F00B41C
#define ARM_TIMER_CNT 0x3F00B420

typedef struct
{
    volatile uint32_t Load;
    volatile uint32_t Value;
    volatile uint32_t Control;
    volatile uint32_t IRQClear;
    volatile uint32_t RAWIRQ;
    volatile uint32_t MaskedIRQ;
    volatile uint32_t Reload;
    volatile uint32_t PreDivider;
    volatile uint32_t FreeRunningCounter;
} rpi_arm_timer_t;

extern rpi_arm_timer_t *RPI_GetArmTimer(void);
extern void arm_timer_init(void);

#endif
