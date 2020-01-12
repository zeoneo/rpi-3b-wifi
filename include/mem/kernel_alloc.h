#ifndef _KERNEL_ALLOC_H_
#define _KERNEL_ALLOC_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    void mem_alloc_init(uint32_t ulBase, uint32_t ulSize);
    void mem_deallocate(void *pBlock);
    void *mem_allocate(uint32_t size);

#ifdef __cplusplus
}
#endif

#endif