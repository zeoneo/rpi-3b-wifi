#ifndef _STRING_H
#define _STRING_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void *s1, const void *s2, size_t n);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
size_t strlen(const char*);
char* strstr(char* str, char* substr);
char* strcat(char* pDest, const char* pSrc);
int strcmp(const char* pString1, const char* pString2);
int strcasecmp(const char* pString1, const char* pString2);
int strncasecmp(const char* pString1, const char* pString2, size_t nMaxLen);
char* strncpy(char* pDest, const char* pSrc, size_t nMaxLen);
char* strtok_r(char* pString, const char* pDelim, char** ppSavePtr);
char* strchr(const char* pString, int chChar);
int atoi(const char* pString);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char* pString, char** ppEndPtr, int nBase);
int strncmp(const char* pString1, const char* pString2, size_t nMaxLen);
char* strcpy(char* pDest, const char* pSrc);

#ifdef __cplusplus
}
#endif

#endif
