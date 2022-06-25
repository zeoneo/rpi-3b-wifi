#ifndef _PLAN9_EMMC_H_
#define _PLAN9_EMMC_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

int emmcinit(void);
int emmcinquiry(char* inquiry, int inqlen);
void emmcenable(void);
int emmccmd(uint32_t cmd, uint32_t arg, uint32_t* resp);
void emmciosetup(int write, void* buf, int bsize, int bcount);
void emmcio(int write, uint8_t* buf, int len);
int sdiocardintr(int wait);

#ifdef __cplusplus
}
#endif

#endif