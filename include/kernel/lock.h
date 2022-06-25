
#ifndef _LOCK_H_
#define _LOCK_H_

#include <kernel/queue.h>
#include <kernel/scheduler.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_OF_PROCESSORS 4

typedef struct {
    int lock;
    int pid;
    // queue* wait_queue;
} mutex_t;

void mutex_init(mutex_t* lock);
void mutex_acquire(mutex_t* lock);
void mutex_release(mutex_t* lock);

typedef struct {
    int lock;
    int pid;
} spin_lock_t;

void spinlock_init(spin_lock_t* lock);
void spinlock_acquire(spin_lock_t* lock);
void spinlock_release(spin_lock_t* lock);

typedef int Lock;

void lock(Lock* lock);
void unlock(Lock* lock);

#ifdef __cplusplus
}
#endif

#endif // _LOCK_H_