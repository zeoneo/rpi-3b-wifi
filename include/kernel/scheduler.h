
#ifndef _SCHED_H_
#define _SCHED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THREAD_CPU_CONTEXT 0 // offset of cpu_context in task_struct

#define THREAD_SIZE 4096

#define FIRST_TASK task[0]
#define LAST_TASK  task[NR_TASKS - 1]

enum { TaskStateReady, TaskStateBlocked, TaskStateSleeping, TaskStateTerminated, TaskStateUnknown };

typedef struct __attribute__((__packed__, aligned(4))) {
    uint32_t r0;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t sp; // r13
    uint32_t lr; // r14
    uint32_t fpexc;
    uint32_t fpscr;
    uint64_t d[16];
} cpu_context_t;

typedef struct __attribute__((__packed__, aligned(4))) task_struct {
    cpu_context_t cpu_context;
    uint32_t state;
    uint32_t counter;
    uint32_t priority;
    uint32_t preempt_count;
    uint32_t pid;
    uint32_t wake_ticks;
    uint8_t name[8];
} task_struct_t;

#define CLOCKHZ 1000000

void preempt_disable(void);
void preempt_enable(void);

void schedule(void);

int add_task(task_struct_t* pTask);
void wake_task(task_struct_t* pTask);

task_struct_t* get_task_by_pid(int pid);

void terminate_this_task();
void block_this_task();

void init_scheduler();

typedef int sleephandler_t(void* param);
void sleep(void* rendez, sleephandler_t* handler, void* param);
void tsleep(void* rendez, sleephandler_t* handler, void* param, unsigned msecs);
void wakeup(void* rendez);
int return0(void* param);

#ifdef __cplusplus
}
#endif

#endif // _TYPES_H