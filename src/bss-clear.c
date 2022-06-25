#include <stdint.h>
extern int32_t __bss_start;
extern int32_t __bss_end;

extern void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags);

void _clear_bss(uint32_t r0, uint32_t r1, uint32_t r2) {
    int32_t* bss     = &__bss_start;
    int32_t* bss_end = &__bss_end;

    while (bss < bss_end)
        *bss++ = 0;

    kernel_main(r0, r1, r2);
    while (1)
        ;
}
