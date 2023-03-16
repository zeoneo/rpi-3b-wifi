#include <device/bcm_random.h>
#include <device/sd_card.h>
#include <device/wifi-io.h>
#include <net/link_layer.h>
#include <plibc/cstring.h>
#include <plibc/stdio.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __enable_exp_
#include <device/bcm4343.h>
#include <device/plan9_ether4330.h>
#endif

#include <device/dma.h>
#include <device/uart0.h>
#include <fs/fat.h>
#include <kernel/fork.h>
#include <kernel/list.h>
#include <kernel/lock.h>
#include <kernel/queue.h>
#include <kernel/rpi-armtimer.h>
#include <kernel/rpi-interrupts.h>
#include <kernel/scheduler.h>
#include <kernel/systimer.h>
#include <mem/frame_alloc.h>
#include <mem/kernel_alloc.h>
#include <mem/virtmem.h>

#ifdef __enable_exp_
static bcm4343_net_device* net_device;
void ether_event_handler(ether_event_type_t type, const ether_event_params_t* params, void* context) {
    printf("Bcm event handler called %x %x %x\n ", &type, params, context);
}

#endif

extern int wpa_supplicant_main(void);
extern int prakash_main(void);

uint32_t x = 0;

extern task_struct_t* current;

spin_lock_t spinlock;
mutex_t mutex;

#define DELAY 100000

void process1(char* array) {
    printf("Name: %s \n", current->name);
    // while (1) {
    // printf("Process 1 \n");
    // printf("Trying to lock pid %d \n", current->pid);
    // spinlock_acquire(&spinlock);
    mutex_acquire(&mutex);
    // printf("In critical Section pid: :%d \n", current->pid);

    int count = 0;
    while (count < 20) {
        for (int i = 0; i < 5; i++) {
            uart_putc(array[i]);
            MicroDelay(10000);
            x++;
        }
        count++;
    }

    // printf("In POSt critical Section 1 \n");

    // spinlock_release(&spinlock);
    mutex_release(&mutex);
    terminate_this_task();
    // }
}

void process2(char* array) {
    printf("Name: %s \n", current->name);
    // spinlock_acquire(&spinlock);
    mutex_acquire(&mutex);
    // printf("Process 2 \n");
    // printf("Trying to lock pid:%d \n", current->pid);
    // printf("In critical Section pid: :%d \n", current->pid);
    int count = 0;
    while (count < 5) {
        for (int i = 0; i < 5; i++) {
            uart_putc(array[i]);
            MicroDelay(50000);
            x++;
        }
        count++;
    }
    // printf("In POSt critical Section 2 \n");
    // spinlock_release(&spinlock);
    mutex_release(&mutex);
    terminate_this_task();
}

void process3(char* array) {
    printf("Name: %s \n", current->name);
    // printf("Trying to lock pid: :%d \n", current->pid);
    // spinlock_acquire(&spinlock);
    mutex_acquire(&mutex);
    // printf("In critical Section pid: :%d \n", current->pid);
    while (1) {
        for (int i = 0; i < 5; i++) {
            uart_putc(array[i]);
            MicroDelay(DELAY);
        }
    }
    // spinlock_release(&spinlock);
    mutex_release(&mutex);
}

bool print_node(void* node) {
    l_node* linked_node = (l_node*) node;
    printf(" node: %u  ", *((uint32_t*) (linked_node->data)));
    return true;
}

void dump_wifi_status(void* arg) {
    if (arg != 0) {
        printf("Dumping wifi status: \n");
#ifdef __enable_exp_
        bcm4343_net_device* n_device = (bcm4343_net_device*) arg;
        dump_status(n_device);
#endif
    }
}

void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags) {
    // Declare as unused
    (void) r0;
    (void) r1;
    (void) atags;

    uart_init();

    printf("\n-----------------Kernel Started Dude........................\n");
    interrupts_init();

    timer_init();
    // This will flash activity led
    // timer_set(5000);
    initialize_random_generator();

    init_frame_manager();
    // show_current_memory_states();
    if (kernel_alloc_init(4096 * 200)) { // 16 MB
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
    // queue* q = new_queue(sizeof(task_struct_t*));
    // int pid  = create_task("proc1", (unsigned long) &process1, (uint32_t) "abcde");
    // if (pid == 0) {
    //     printf("error while starting process 1");
    //     return;
    // }
    // task_struct_t* task = get_task_by_pid(pid);
    // printf("taskname: %s task_addr:0x%x\n", task->name, task);
    // enqueue(q, &task);
    // pid = create_task("proc2", (unsigned long) &process2, (uint32_t) "#$@*!");
    // if (pid == 0) {
    //     printf("error while starting process 2");
    //     return;
    // }
    // task = get_task_by_pid(pid);
    // printf("taskname: %s task_addr:0x%x\n", task->name, task);
    // enqueue(q, &task);
    // pid = create_task("proc3", (unsigned long) &process3, (uint32_t) "12345");
    // if (pid == 0) {
    //     printf("error while starting process 3");
    //     return;
    // }
    // task = get_task_by_pid(pid);
    // printf("taskname: %s task_addr:0x%x\n", task->name, task);
    // enqueue(q, &task);

    // dequeue(q, &task);
    // printf("Task Name: %s \n", task->name);

    // dequeue(q, &task);
    // printf("Task Name: %s \n", task->name);

    // dequeue(q, &task);
    // printf("Task Name: %s \n", task->name);

    // spinlock_init(&spinlock);
    // mutex_init(&mutex);

#ifdef __enable_exp_

    net_device = allocate_bcm4343_device("/c/prakash/");
    // set_essid(net_device, "BlackCat");
    // set_auth(net_device, AuthModeWPA2, "RatKillerCatBlack567");

    // set_essid(net_device, "ZER1");
    // set_auth(net_device, AuthModeWPA2, "RatKiller");

    // set_essid(net_device, "ZER");
    // set_auth(net_device, AuthModeNone, "");

    printf("initialize: 0x%x \n", initialize);
    // prakash_main();
    printf("initialize_link_layer: 0x%x \n", initialize_link_layer);
    printf("wpa_supplicant_main: 0x%x \n", wpa_supplicant_main);

    initialize(net_device);
    initialize_link_layer(net_device);
    register_event_handler(net_device, ether_event_handler, (void*) 0);
    int pid2 = create_task("wifi", (unsigned long) &wpa_supplicant_main, 0);
    if (pid2 == 0) {
        printf("error while starting process 3");
        return;
    }
#endif
    schedule();
    // if () {
    // printf("<---------------Wifi Started Successfully----------------->\n");
    // }
    // char abc[] = "@#$%^&*()";
    while (1) {

        // printf("Dumping wifi status: \n");
        // #ifdef __enable_exp_
        //     dump_status(net_device);
        // #endif
        schedule(); // This is init process which will call schedule when intialization is completed.
    }
}