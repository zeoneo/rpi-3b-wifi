#ifndef __SD_HOST1_H_
#define __SD_HOST1_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include<stdint.h>


uint32_t init_sdhost();
int sdhost1_cmd(uint32_t cmd, uint32_t arg, uint32_t *resp);
void sdhost1_iosetup(int write, void *buf, int bsize, int bcount);
void sdhost1_io(int write, uint8_t *buf, int len);

/**
 * End of all the declarations here.
 * */

#ifdef __cplusplus
}
#endif

#endif