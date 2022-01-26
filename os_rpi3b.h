/*
 * OS specific functions
 * Copyright (c) 2005-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef _os_rpi3b_h
#define _os_rpi3b_h

#ifdef __cplusplus
extern "C" {
#endif

#include <mem/kernel_alloc.h>
#include <plibc/string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int x;
} FILE;

struct in_addr {
    int x;
};

typedef signed long time_t;
typedef unsigned int size_t;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#include "os.h"

#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER    __LITTLE_ENDIAN
#define WPA_TYPES_DEFINED

#define __force

#define bswap_16 bswap16
#define bswap_32 bswap32

int abs(int i);

int isprint(int c);

char* strrchr(const char* s, int c);
char* strdup(const char* s);

int snprintf(char* str, size_t size, const char* format, ...);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int printf(const char* format, ...);
int vprintf(const char* format, va_list ap);

void qsort(void* base, size_t nmemb, size_t size, int (*compare)(const void*, const void*));

void abort(void);

void os_hexdump(const char* title, const void* p, size_t len);

// #define os_daemonize(s)			(0)

// #define os_daemonize_terminate(s)	((void) 0)

#ifdef __cplusplus
}
#endif

#endif