#ifndef _GPU_MEM_UTIL_H
#define _GPU_MEM_UTIL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include<stdint.h>

void emit_uint8_t(uint8_t **list, uint8_t d);
void emit_uint16_t(uint8_t **list, uint16_t d);
void emit_uint32_t(uint8_t **list, uint32_t d);
void emit_float(uint8_t **list, float f);

#ifdef __cplusplus
}
#endif

#endif