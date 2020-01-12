#include <stdint.h>
#include <kernel/rpi-armtimer.h>
#include <kernel/rpi-interrupts.h>

static rpi_arm_timer_t *rpiArmTimer = (rpi_arm_timer_t *)RPI_ARMTIMER_BASE;

rpi_arm_timer_t *RPI_GetArmTimer(void)
{
    return rpiArmTimer;
}

void arm_timer_init(void)
{
    RPI_GetIrqController()->Enable_Basic_IRQs = RPI_BASIC_ARM_TIMER_IRQ;
    RPI_GetArmTimer()->Load = 0x400;
    RPI_GetArmTimer()->Control =
        RPI_ARMTIMER_CTRL_23BIT |
        RPI_ARMTIMER_CTRL_ENABLE |
        RPI_ARMTIMER_CTRL_INT_ENABLE |
        RPI_ARMTIMER_CTRL_PRESCALE_256;
}
