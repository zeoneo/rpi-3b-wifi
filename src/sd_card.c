#include<device/sd_card.h>
#include<device/sdhost.h>
#include<plibc/stdio.h>
#include <kernel/systimer.h>


int init_sdcard() {
    struct sdhost_state * sd_host = sdhostInit();
    if(!sd_host) {
        printf("Unable initialize sd host \n");
        return -1;
    }
    
    MicroDelay(1000);

    // uint32_t resp[4] = {0};

    struct mmc_cmd cmd;
    cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

    int x  = bcm2835_send_cmd(sd_host, &cmd, (void *)0);
    printf("Response :%x \n",x);
    return 0;
}

// int read_block(uint32_t addr, uint32_t length, uin8_t *buffer) {
//     return 0;
// }