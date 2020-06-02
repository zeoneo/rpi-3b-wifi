#ifndef _STRING_H
#define _STRING_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int32_t, size_t);
size_t strlen(const char*);
char* strstr(char* str, char* substr);
int strcmp(const char* pString1, const char* pString2);
int strcasecmp(const char* pString1, const char* pString2);
int strncasecmp(const char* pString1, const char* pString2, size_t nMaxLen);
char* strncpy(char* pDest, const char* pSrc, size_t nMaxLen);
char* strtok_r(char* pString, const char* pDelim, char** ppSavePtr);
int atoi(const char* pString);

#ifdef __cplusplus
}
#endif

#endif
