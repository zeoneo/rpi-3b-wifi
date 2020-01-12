#include <stdint.h>
#include <kernel/rpi-base.h>
#ifndef TIMER_H
#define TIMER_H

#define SYSTEM_TIMER_OFFSET 0x3000
#define SYSTEM_TIMER_BASE (SYSTEM_TIMER_OFFSET + PERIPHERAL_BASE)

void timer_init(void);

void timer_set(uint32_t usecs);
void MicroDelay(uint64_t delayInUs);

void udelay(uint32_t usecs);
uint32_t timer_getTickCount32(void);
uint64_t timer_getTickCount64(void);
uint64_t tick_difference (uint64_t us1, uint64_t us2);

typedef struct
{
    uint8_t timer0_matched : 1;
    uint8_t timer1_matched : 1;
    uint8_t timer2_matched : 1;
    uint8_t timer3_matched : 1;
    uint32_t reserved : 28;
} timer_control_reg_t;

typedef struct __attribute__((__packed__, aligned(4)))
{
    timer_control_reg_t control;
    uint32_t counter_low;
    uint32_t counter_high;
    uint32_t timer0;
    uint32_t timer1;
    uint32_t timer2;
    uint32_t timer3;
} timer_registers_t;

#endif
