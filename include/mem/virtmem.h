#ifndef _VIRT_MEM_H
#define _VIRT_MEM_H
#include <stdint.h>

extern void PUT32(uint32_t addr, uint32_t value);
extern void start_mmu(uint32_t, uint32_t);
extern void stop_mmu(void);
extern void invalidate_tlbs(void);
extern void invalidate_caches(void);

void initialize_virtual_memory(void);
uint32_t mmu_section(uint32_t vadd, uint32_t padd, uint32_t flags);

#endif