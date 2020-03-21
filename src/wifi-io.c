#include<device/wifi-io.h>
#include<device/emmc-sdio.h>
#include <kernel/systimer.h>
#include <plibc/stdio.h>

static uint32_t cmd_resp [4] = {0};
static uint32_t ocr = 0;
static uint32_t rca = 0;

enum {
    Fn0	    = 0,
	Fn1 	= 1,
	Fn2	    = 2,
	Fbr1	= 0x100,
	Fbr2	= 0x200,

    	/* CCCR */
	Ioenable	= 0x02,
	Ioready		= 0x03,
	Intenable	= 0x04,
	Intpend		= 0x05,
	Ioabort		= 0x06,
	Busifc		= 0x07,
	Capability	= 0x08,
	Blksize		= 0x10,
	Highspeed	= 0x13,
};

static int sdiord(uint32_t fn, uint32_t addr)
{
	bool is_success = sdio_send_command(IX_IO_RW_DIRECT, (0<<31)|((fn&7)<<28) | ((addr&0x1FFFF)<<9), &cmd_resp[0]);
    if(!is_success) {
        printf("Could not complete IO_RW_DIRECT: fn :%d addr: 0x%x \n", fn, addr);
        return 0;
    }
	if(cmd_resp[0] & 0xCF00){
		printf("ERROR: ether4330: sdiord(%x, %x) fail: %2.2x %2.2x\n", fn, addr, (cmd_resp[0]>>8)&0xFF, cmd_resp[0]&0xFF);
	}
	return cmd_resp[0] & 0xFF;
}

static void sdiowr(int fn, int addr, int data)
{
	uint32_t r;
	int retry;

	r = 0;
	for(retry = 0; retry < 10; retry++){
		sdio_send_command(IX_IO_RW_DIRECT, (1<<31)|((fn&7)<<28)|((addr&0x1FFFF)<<9)|(data&0xFF), &cmd_resp[0]);
        r = cmd_resp[0];
		if((r & 0xCF00) == 0) {
            return;
        }
	}
	printf("ERROR: ether4330: sdiowr(%x, %x, %x) fail: %2.2ux %2.2ux\n", fn, addr, data, (r>>8)&0xFF, r&0xFF);
}


static void sdioset(uint32_t fn, uint32_t addr, uint32_t bits)
{
	sdiowr(fn, addr, sdiord(fn, addr) | bits);
}

bool startWifi () {
    bool is_success = initSDIO();

    is_success = sdio_send_command(IX_GO_IDLE_STATE, 0, &cmd_resp[0]);
    if(is_success) {
        printf(" Sent go idle command \n");
    }

    is_success = sdio_send_command(IX_IO_SEND_OP_COND, 0 ,  &cmd_resp[0]);
    ocr = cmd_resp[0];
    if(is_success) {
        printf("Found card OCR : 0x%x\n", ocr);
        uint32_t i = 0;
        while((ocr & (1<<31)) == 0) {
            if(++i > 5){
                printf("ether4330: no response to sdio access: ocr = %x\n", ocr);
                return false;
            }
            // send op condition 3V_3
            is_success = sdio_send_command(IX_IO_SEND_OP_COND, 3<<20 ,  &cmd_resp[0]);
            ocr = cmd_resp[0];
            MicroDelay(100);
	    }
    } else {
        printf("Failed to get ocr. \n");
        return false;
    }
    printf("NEW OCR : 0x%x\n", ocr);
    is_success = sdio_send_command(IX_SEND_RELATIVE_ADDR, 0, &cmd_resp[0]);
    if(is_success) {
        rca = (cmd_resp[0] >> 16);
        printf("rca: 0x%x \n",rca);
        is_success = sdio_send_command(IX_SELECT_CARD, rca << 16, &cmd_resp[0]);
        if(!is_success) {
            printf("Could not select card \n");
            return false;
        }
        
    }

	sdioset(Fn0, Highspeed, 2);
	sdioset(Fn0, Busifc, 2);	/* bus width 4 */
	sdiowr(Fn0, Fbr1+Blksize, 64);
	sdiowr(Fn0, Fbr1+Blksize+1, 64>>8);
	sdiowr(Fn0, Fbr2+Blksize, 512);
	sdiowr(Fn0, Fbr2+Blksize+1, 512>>8);
	sdioset(Fn0, Ioenable, 1<<Fn1);
	sdiowr(Fn0, Intenable, 0);

    uint32_t i = 0;
    for(i = 0; !(sdiord(Fn0, Ioready) & 1<<Fn1); i++){
		if(i == 10){
			printf("ether4330: can't enable SDIO function\n");
			return false;
		}
		MicroDelay(100);
	}

    printf(" +++++Enabled SDIO IO functionality \n");
    return is_success;
}