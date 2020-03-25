#ifndef _KERNEL_ALLOC_H_
#define _KERNEL_ALLOC_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    int32_t kernel_alloc_init(uint32_t ulSize);
    void kernel_deallocate(void *pBlock);
    void *kernel_allocate(uint32_t size);

#ifdef __cplusplus
}
#endif

#endif