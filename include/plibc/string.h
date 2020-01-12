#ifndef _STRING_H
#define _STRING_H 1

#include <sys/cdefs.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int32_t memcmp(const void *, const void *, size_t);
    void *memcpy(void *__restrict, const void *__restrict, size_t);
    void *memmove(void *, const void *, size_t);
    void *memset(void *, int32_t, size_t);
    size_t strlen(const int8_t *);
    
#ifdef __cplusplus
}
#endif

#endif
