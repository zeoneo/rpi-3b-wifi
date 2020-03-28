#include<kernel/fork.h>
#include<kernel/scheduler.h>
#include<mem/frame_alloc.h>
#include<plibc/stdio.h>

extern task_struct_t *current;
extern int nr_tasks;

int copy_process(uint32_t fn, uint32_t arg) {
    preempt_disable();
    struct task_struct *p;

    p = (task_struct_t *) alloc_frame();
    if (!p) {
        return 1;
    }

    p->priority = current->priority;
    p->state = TASK_RUNNING;
    p->counter = p->priority;
    p->preempt_count = 1; //disable preemtion until schedule_tail

    p->cpu_context.lr = fn;
    p->cpu_context.r0 = arg;
    p->cpu_context.sp = (unsigned long)p + THREAD_SIZE;
    printf("p:0x%x sp: 0x%x \n", p, p->cpu_context.sp);
    int pid = nr_tasks++;
    task[pid] = p;
    p->pid = pid;
    preempt_enable();
    return 0;
}
