
#ifndef _SCHED_H_
#define _SCHED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define THREAD_CPU_CONTEXT  0   // offset of cpu_context in task_struct

#define THREAD_SIZE 4096
#define NR_TASKS    64

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#define TASK_RUNNING    0

extern struct task_struct *current;
extern struct task_struct * task[NR_TASKS];
extern int nr_tasks;

typedef struct __attribute__((__packed__, aligned(4))) {
    uint32_t    r0;
	// uint32_t    fpexc;
	// uint32_t    fpscr;
	uint32_t	r4;
	uint32_t	r5;
	uint32_t	r6;
	uint32_t	r7;
	uint32_t	r8;
	uint32_t	r9;
	uint32_t	r10;
	uint32_t	r11;
	uint32_t	r12;
	uint32_t	sp;
	uint32_t	lr;
	// uint64_t    d[16];
} cpu_context_t;

typedef struct __attribute__((__packed__, aligned(4))) task_struct {
    cpu_context_t cpu_context;
    uint32_t state;
    uint32_t counter;
    uint32_t priority;
    uint32_t preempt_count;
	uint32_t pid;
} task_struct_t;

void schedule(void);
void preempt_disable(void);
void preempt_enable(void);
void switch_to(struct task_struct* next);
void init_scheduler();

#ifdef __cplusplus
}
#endif

#endif // _TYPES_H