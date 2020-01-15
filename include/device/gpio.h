#ifndef _GPIO_H_
#define _GPIO_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef enum
    {
        Alt0 = 0x4,
        Alt1 = 0x5,
        Alt2 = 0x6,
        Alt3 = 0x7,
        Alt4 = 0x3,
        Alt5 = 0x2,
    } alt_func;

    void select_alt_func(uint32_t bcm_pin, alt_func alt_fun);

    void set_mode_output(uint32_t bcm_pin);
    void set_mode_input(uint32_t bcm_pin);

    void set_pin(uint32_t bcm_pin);
    void clear_pin(uint32_t bcm_pin);

    void disable_pulling(uint32_t bcm_pin);
    void pullup_pin(uint32_t bcm_pin);
    void pulldown_pin(uint32_t bcm_pin);

    void set_activity_led(uint32_t on_off);

    uint32_t read_digital_pin(uint32_t bcm_pin);

#ifdef __cplusplus
}
#endif

#endif