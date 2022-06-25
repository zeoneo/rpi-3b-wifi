#include <kernel/lock.h>
#include <kernel/queue.h>
#include <kernel/rpi-interrupts.h>
#include <kernel/scheduler.h>
#include <mem/kernel_alloc.h>
#include <plibc/stdio.h>

extern int read_cpu_id();
extern task_struct_t* current;

// static int try_mutext_acquire(mutex_t* lock) {
//     volatile int* val = &(lock->lock);
//     if (*val == 0) {
//         *val      = 1;
//         lock->pid = current->pid;
//         return 1;
//     }
//     // printf("pid: %d lock:%d \n", current->pid, lock->lock);
//     return 0;
// }

void mutex_init(mutex_t* lock) {
    lock->lock = 0;
    lock->pid  = -1;
    // lock->wait_queue = new_queue(sizeof(task_struct_t*));
}

void mutex_acquire(mutex_t* lock) {
    volatile int* val = &(lock->lock);
    do {
        schedule();
    } while (*val == 1);
    *val = 1;
}

void mutex_release(mutex_t* lock) {
    volatile int* val = &(lock->lock);
    *val              = 0;
    schedule();
}

void spinlock_init(spin_lock_t* lock) {
    lock->lock = 0;
    lock->pid  = -1;
}

void spinlock_acquire(spin_lock_t* lock) {
    volatile int* val = &(lock->lock);
    while (*val == 1) {
        // printf("Pid: %d lock: %d lock-pid: %d \n", current->pid, val, lock->pid);
    }
    *val      = 1;
    lock->pid = current->pid;
    // printf("Lock acquired pid: %d lock: %d lock-pid: %d \n ", current->pid, lock->lock, lock->pid);
}

void spinlock_release(spin_lock_t* lock) {
    lock->lock = 0;
    lock->pid  = -1;
    // printf("\n releasing lock : %d lock-pid :%d \n\n", lock->lock, lock->pid);
}

void lock(Lock* lock) {
    *lock = 1;
    // this will prevent scheduling to occur. So on single process current process will run until
    // it calls unlock which enables interrupt -> timer interrupt will fire and cause schedule()
    DISABLE_INTERRUPTS();
}

void unlock(Lock* lock) {
    *lock = 0;
    ENABLE_INTERRUPTS();
}