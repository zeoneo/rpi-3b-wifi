#include <kernel/systimer.h>
#include <kernel/rpi-interrupts.h>
#include <device/uart0.h>
#include <plibc/stdio.h>
#include <device/gpio.h>

static timer_registers_t *timer_regs; // = (timer_registers_t *)SYSTEM_TIMER_BASE;

extern void dmb(void);
static void timer_irq_handler(void)
{
    // uart_puts("\n *** timer irq handler called");

    timer_set(345000);
    
}

static void timer_irq_clearer(void)
{
    timer_regs->control.timer1_matched = 1;
    // uart_puts("\n *** timer irq clearer called");
    set_activity_led(0);
    MicroDelay( 1000 * 1000);
    set_activity_led(1);
    
}

void timer_init(void)
{
    timer_regs = (timer_registers_t *)SYSTEM_TIMER_BASE;
    register_irq_handler(RPI_BASIC_ARM_TIMER_IRQ, timer_irq_handler, timer_irq_clearer);
}

void timer_set(uint32_t usecs)
{
    timer_regs->timer1 = timer_regs->counter_low + usecs;
}

__attribute__((optimize(0))) void udelay(uint32_t usecs)
{
    volatile uint32_t curr = timer_regs->counter_low;
    volatile uint32_t x = timer_regs->counter_low - curr;
    volatile uint32_t highCount = -1;
    volatile uint32_t lowCount = -1;
    while (x < usecs)
    {
        lowCount = timer_regs->counter_low;
        highCount = timer_regs->counter_high;
        printf("\n timer_regs->counter_low: %x", lowCount);
        printf("\n timer_regs->counter_high: %x", highCount);
        printf("\n 64 bit: %lx", 0x1234567812345678);
        x = timer_regs->counter_low - curr;
    }
}

uint32_t timer_getTickCount32(void)
{
    uint32_t count;

    count = timer_regs->counter_low;

    return count;
}

uint64_t timer_getTickCount64(void)
{
    volatile uint64_t resVal = -1;
    volatile uint32_t lowCount = -1;
    do
    {
        resVal = timer_regs->counter_high;                  // Read Arm system timer high count
        lowCount = timer_regs->counter_low;                 // Read Arm system timer low count
    } while (resVal != (uint64_t)timer_regs->counter_high); // Check hi counter hasn't rolled in that time
    resVal = (uint64_t)(resVal << 32 | lowCount);           // Join the 32 bit values to a full 64 bit
    return (resVal);                                        // Return the uint64_t timer tick count
}

uint64_t tick_difference (uint64_t us1, uint64_t us2) {
	if (us1 > us2) {												// If timer one is greater than two then timer rolled
		uint64_t td = UINT64_MAX - us1 + 1;							// Counts left to roll value
		return us2 + td;											// Add that to new count
	}
	return us2 - us1;												// Return difference between values
}

void MicroDelay(uint64_t delayInUs)
{
    uint64_t timer_tick = timer_getTickCount64();
    while (timer_getTickCount64() < (timer_tick + delayInUs))
        ;
}