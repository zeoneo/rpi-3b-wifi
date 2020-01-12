#ifndef _PI_CONSOLE_H
#define _PI_CONSOLE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include<stdint.h>

void get_console_width_height_depth(uint32_t *width, uint32_t *height, uint32_t *depth, uint32_t *pitch);
uint32_t get_console_frame_buffer(uint32_t width, uint32_t height, uint32_t depth);
void console_puts(char *str);
#ifdef __cplusplus
}
#endif

#endif