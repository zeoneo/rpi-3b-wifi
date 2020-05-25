#include <stddef.h>
#include <stdint.h>
#include <plibc/stdio.h>


#include <device/sd_card.h>
#include <device/wifi-io.h>

#ifdef __enable_exp_
    #include <device/plan9_ether4330.h>
    #include <device/bcm4343.h>
#endif

#include <device/uart0.h>
#include <device/dma.h>
#include <fs/fat.h>
#include <kernel/rpi-armtimer.h>
#include <kernel/rpi-interrupts.h>
#include <kernel/systimer.h>
#include <kernel/scheduler.h>
#include <kernel/fork.h>
#include <kernel/list.h>

#include <mem/frame_alloc.h>
#include <mem/virtmem.h>
#include <mem/kernel_alloc.h>

#ifdef __enable_exp_
static bcm4343_net_device *net_device;
#endif

void process1(char *array)
{
	while (1){
		for (int i = 0; i < 5; i++){
			uart_putc(array[i]);
			MicroDelay(100000);
		}
	}
}

void process2(char *array)
{
	while (1){
		for (int i = 0; i < 5; i++){
			uart_putc(array[i]);
			MicroDelay(100000);
		}
	}
}

bool print_node(void * node) {
    l_node * linked_node = (l_node *) node;
    printf(" node: %u  ", *((uint32_t *)(linked_node->data)));
    return true;
}

void dump_wifi_status(void *arg) {
    if(arg != 0) {
        printf("Dumping wifi status: \n");
        #ifdef __enable_exp_
            bcm4343_net_device *n_device = (bcm4343_net_device *) arg;
            dump_status(n_device);
        #endif
    }
}

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
    // timer_set(5000);
    
    init_frame_manager();
    // show_current_memory_states();
    if(kernel_alloc_init(4096 * 100 )) { // 16 MB
        printf(" Failed to initialize kernel alloc.");
    } else {
        printf("Kernel allocation init success \n");
    }
    // show_current_memory_states();

    
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

    init_scheduler();
	// res = copy_process((unsigned long)&process2, (uint32_t)"abcde");
	// if (res != 0) {
	// 	printf("error while starting process 2");
	// 	return;
	// }
    // res = copy_process((unsigned long)&process2, (uint32_t)"uioprt");
	// if (res != 0) {
	// 	printf("error while starting process 2");
	// 	return;
	// }
    //  res = copy_process((unsigned long)&process2, (uint32_t)"9876543");
	// if (res != 0) {
	// 	printf("error while starting process 2");
	// 	return;
	// }
	// schedule();



    #ifdef __enable_exp_

        // etherbcmattach();
        net_device = allocate_bcm4343_device("/c/prakash/");
        set_essid(net_device, "ZERONONE");
        set_auth(net_device, AuthModeNone, "RatKillerCatBlack567");
        initialize(net_device);
        // int res = copy_process((unsigned long)&dump_wifi_status, (uint32_t)net_device);
        // if (res != 0) {
        //     printf("error while starting process 1");
        //     return;
        // }
    #endif
    // if () {
        // printf("<---------------Wifi Started Successfully----------------->\n");
    // }
    // char abc[] = "@#$%^&*()";
	while (1){

        // printf("Dumping wifi status: \n");
        // #ifdef __enable_exp_
        //     dump_status(net_device);
        // #endif
		schedule(); // This is init process which will call schedule when intialization is completed.
	}

}