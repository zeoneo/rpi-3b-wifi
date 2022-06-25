#ifndef USB_MEM_H
#define USB_MEM_H

#include <stdint.h>

// #define NULL ((void*)0)

void* MemoryAllocate(uint32_t length);

void MemoryDeallocate(void* address);

void* MemoryReserve(uint32_t length, void* physicalAddress);

void MemoryCopy(void* destination, void* source, uint32_t length);

void PlatformLoad();

#endif