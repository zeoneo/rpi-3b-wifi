#ifndef _SD_CARD_H_
#define _SD_CARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

int init_sdcard();
bool mmc_read_blocks(uint32_t startBlock, uint32_t numBlocks, uint8_t* dest);

/**
 * End of all the declarations here.
 * */

#ifdef __cplusplus
}
#endif

#endif