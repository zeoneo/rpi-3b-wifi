#include <stddef.h>
#include <stdint.h>
#include <plibc/stdio.h>


#include <device/sd_card.h>
#include <device/wifi-io.h>
#include <device/plan9_ether4330.h>
#include <device/uart0.h>
#include <device/dma.h>
#include <fs/fat.h>
#include <kernel/rpi-armtimer.h>
#include <kernel/rpi-interrupts.h>
#include <kernel/systimer.h>
#include <mem/frame_alloc.h>
#include <mem/virtmem.h>
#include <mem/kernel_alloc.h>

void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags)
{
    // Declare as unused
    (void)r0;
    (void)r1;
    (void)atags;

    uart_init();

    printf("\n-----------------Kernel Started Dude........................\n");
    interrupts_init();

    timer_init();
    // This will flash activity led
    timer_set(5000);
    
    init_frame_manager();
    if(!kernel_alloc_init(0x100000 * 64)) { // 16 MB
        printf(" Failed to initialize kernel alloc.");
    } else {
        printf("Kernel allocation init success \n");
    }
    
    // show_dma_demo();


    // printf("<---------------PRINT ROOT DIR USING SDHOST ----------------->\n");
    // init_sdcard();
    // if(initialize_fat(mmc_read_blocks)) {
    // 	print_root_directory_info();
    // }
    
    // if (sdInitCard(false) == SD_OK) {
    // 	printf("<---------------PRINT ROOT DIR USING ARASAN EMMC ----------------->\n");
    //     printf("Failed to initialize SD card");
    // 	if(initialize_fat(sdcard_read)) {
    // 		printf("-------Successfully Initialized FAT----------\n");
    // 		print_root_directory_info();
    // 	} else {
    // 		printf("-------Failed to initialize FAT----------\n");
    // 	}
    // }

    etherbcmattach();
    // if () {
        // printf("<---------------Wifi Started Successfully----------------->\n");
    // }
    while (1)
    {
    }

}