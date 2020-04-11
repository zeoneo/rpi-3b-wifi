#include <stddef.h>
#include <stdint.h>
#include <plibc/stdio.h>


#include <device/sd_card.h>
#include <device/wifi-io.h>

#ifdef __enable_exp_
    #include <device/plan9_ether4330.h>
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
    // int res = copy_process((unsigned long)&process1, (uint32_t)"12345");
	// if (res != 0) {
	// 	printf("error while starting process 1");
	// 	return;
	// }
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
	schedule();

    uint32_t list_nodes[] = { 12,134,345,456,788,67};

    list * lst = new_list(sizeof(uint32_t));

    printf("list size: %u \n", list_size(lst));

    for(int ui = 0 ; ui < 6; ui++) {
        list_push_back(lst, &list_nodes[ui]);
    }
    l_node *tail = list_tail(lst);
    list_for_each(lst, &print_node);
    printf("\n");

    for(int ui = 0 ; ui < 6; ui++) {
        list_push_front(lst, &list_nodes[ui]);
    }

    list_for_each(lst, &print_node);
    printf("\n");

    list_delete_node(lst, tail);
    list_for_each(lst, &print_node);
    printf("\n");

    for(int ui = 0 ; ui < 6; ui++) {
        list_delete_head(lst);
        list_delete_tail(lst);
        list_for_each(lst, &print_node);
        printf("\n");
    }
    list_delete_tail(lst);
    list_for_each(lst, &print_node);
    printf("\n");
    for(int ui = 0 ; ui < 6; ui++) {
        list_push_front(lst, &list_nodes[ui]);
    }
    list_for_each(lst, &print_node);
    printf("\n");
    destroy_list(lst);

    #ifdef __enable_exp_
        etherbcmattach();
    #endif
    // if () {
        // printf("<---------------Wifi Started Successfully----------------->\n");
    // }
    // char abc[] = "@#$%^&*()";
	while (1){
		schedule(); // This is init process which will call schedule when intialization is completed.
	}

}