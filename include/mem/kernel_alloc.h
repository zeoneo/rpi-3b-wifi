#ifndef _KERNEL_ALLOC_H_
#define _KERNEL_ALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef unsigned int size_t;

int32_t kernel_alloc_init(uint32_t ulSize);
void kernel_deallocate(void* pBlock);
void* kernel_allocate(size_t size);
void *calloc (size_t nBlocks, size_t nSize);
void *realloc (void *pBlock, size_t nSize);

// void *palloc (void);			// returns aligned page (AArch32: 4K, AArch64: 64K)
// void pfree (void *pPage);


#ifdef __cplusplus
}
#endif

#endif