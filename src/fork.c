#include <kernel/fork.h>
#include <kernel/scheduler.h>
#include <mem/frame_alloc.h>
#include <plibc/stdio.h>
#include <plibc/string.h>

extern void ret_from_fork(void);

int create_task(char* name, uint32_t fn, uint32_t arg) {
    preempt_disable();
    struct task_struct* p;

    p = (task_struct_t*) alloc_contiguous_frames(10);
    if (!p) {
        return 0;
    }
    memcpy(p->name, name, 8);
    p->name[7] = '\0';

    p->state         = TaskStateReady;
    p->preempt_count = 0; // disable preemtion until schedule_tail

    p->cpu_context.lr = fn;
    p->cpu_context.r0 = arg;
    // p->cpu_context.pc = (unsigned) ret_from_fork;
    p->cpu_context.sp = (unsigned long) p + (9*THREAD_SIZE);
    int pid           = add_task(p);
    printf("NEw Task name: %s base: %x  sp: %x \n",p->name,p,  p->cpu_context.sp);
    preempt_enable();
    return pid;
}

void delete_task(task_struct_t* task_to_delete) {
    dealloc_frame(task_to_delete);
}