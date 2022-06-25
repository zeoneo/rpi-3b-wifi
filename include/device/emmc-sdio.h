#ifndef _EMMC_SDIO_H
#define _EMMC_SDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*--------------------------------------------------------------------------}
{						SD CARD COMMAND INDEX DEFINITIONS				    }
{--------------------------------------------------------------------------*/
#define IX_GO_IDLE_STATE         0
#define IX_IO_SEND_OP_COND       1
#define IX_SEND_RELATIVE_ADDR    2
#define IX_SELECT_CARD           3
#define IX_IO_RW_DIRECT          4
#define IX_IO_RW_DIRECT_EXTENDED 5

bool initSDIO();

bool sdio_send_command(int cmd_idx, uint32_t arg, uint32_t* resp);

void sdio_iosetup(bool write, void* buf, uint32_t bsize, uint32_t bcount);

void sdio_do_io(bool write, uint8_t* buf, uint32_t len);

/**
 * End of all the declarations here.
 * */

#ifdef __cplusplus
}
#endif

#endif