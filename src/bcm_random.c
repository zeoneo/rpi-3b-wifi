#include <device/bcm_random.h>
#include <kernel/rpi-base.h>

#define PERI_RNG_BASE    (PERIPHERAL_BASE + 0x104000)
#define PERI_RNG_CTRL    (PERI_RNG_BASE + 0x00)
#define PERI_RNG_STATUS  (PERI_RNG_BASE + 0x04)
#define PERI_RNG_DATA    (PERI_RNG_BASE + 0x08)
#define PERI_RNG_CTRL_EN 0x01

static int initialized = 0;

extern void PUT32(uint32_t addr, uint32_t value);
extern uint32_t GET32(uint32_t addr);


void initialize_random_generator() {

    if (!initialized) {
        initialized = 1;
        PUT32(PERI_RNG_STATUS, 0x40000);
        PUT32(PERI_RNG_CTRL, PERI_RNG_CTRL_EN);
    }
}

uint32_t get_random_int() {
    while ((GET32 (PERI_RNG_STATUS) >> 24) == 0)
	{
		// just wait
	}
	
	return GET32(PERI_RNG_DATA);
}