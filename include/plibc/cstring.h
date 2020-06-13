#ifndef _C_STRING_H_
#define _C_STRING_H_ 1

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    char * buffer;
    char * m_pInPtr;
    uint32_t buffer_size;
} c_string_t;

c_string_t * get_new_cstring(const void *char_buf);
bool reinit_cstring (c_string_t * cstr, const void *char_buf);

c_string_t * deep_copy_cstring (c_string_t * cstr);

unsigned int get_length (c_string_t * cstr);
void append_to_cstring(c_string_t *cstr, const void *char_buf);
int compare_cstrings(c_string_t *cstr1, c_string_t* cstr2);
int find_in_cstrings(c_string_t *cstr, char c);
int replace_in_cstring (c_string_t *cstr, const char *pOld, const char *pNew);
void cstring_format (c_string_t *cstr, const char *pFormat, ...);
void cstring_formatv(c_string_t *cstr, const char *pFormat, va_list Args);

char * cstring_ntoa (char *pDest, unsigned long ulNumber, unsigned nBase, bool bUpcase);
char * cstring_lltoa (char *pDest, unsigned long long ullNumber, unsigned nBase, bool bUpcase);
char * cstring_ftoa (char *pDest, double fNumber, unsigned nPrecision);

void deallocate_cstring(c_string_t *cstr);


#ifdef __cplusplus
}
#endif

#endif
