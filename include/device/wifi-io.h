#ifndef _WIFI_IO_H
#define _WIFI_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool startWifi();
uint32_t cfgreadl(int fn, uint32_t off);

/**
 * End of all the declarations here.
 * */

#ifdef __cplusplus
}
#endif

#endif