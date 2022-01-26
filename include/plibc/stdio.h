#ifndef _STDIO_H
#define _STDIO_H 1

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <sys/cdefs.h>

int printf(const char* __restrict, ...);
int vprintf(const char* __restrict, va_list);
int putchar(int);
int puts(const char*);
int vsprintf(char* buf, const char* fmt, va_list ap);
char* format_str(char* buf, char* fmt, ...);
#ifdef __cplusplus
}
#endif

#endif
