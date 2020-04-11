#include<kernel/scheduler.h>
#include <kernel/systimer.h>
#include <plibc/stdio.h>

#define INIT_TASK \
/*cpu_context*/	{ {0}, \
/* state etc */	0,0,1, 0, 0 \
}

task_struct_t init_task = INIT_TASK;
task_struct_t *current = &(init_task);
task_struct_t * task[NR_TASKS] = {&(init_task), };
int nr_tasks = 1;

extern void task_switch(task_struct_t *old, task_struct_t *new);

void init_scheduler() {
	repeat_on_time_out(schedule, 8000);
	current->priority = 1;
}
void preempt_disable(void)
{
	current->preempt_count++;
}

void preempt_enable(void)
{
	current->preempt_count--;
}


void _schedule(void)
{
	preempt_disable();
	uint32_t next,c;
	struct task_struct * p;
	// printf(" nr_tasks: %d \n ", nr_tasks);
	while (1) {
		c = -1;
		next = 0;
		for (int i = 0; i < NR_TASKS; i++){
			p = task[i];
			if (p && p->state == TASK_RUNNING && p->counter > c) {
				// printf("at i")
				c = p->counter;
				next = i;
			}
		}
		if (c) {
			break;
		}
		for (int i = 0; i < NR_TASKS; i++) {
			p = task[i];
			if (p) {
				p->counter = (p->counter >> 1) + p->priority;
			}
		}
	}

	next = (current->pid + 1) % nr_tasks;

	// next = 1;
	// printf("Switching to next task \n");
	switch_to(task[next]);
	preempt_enable();
}

void schedule(void)
{
	// printf("calling scheduler current pid: %d \n", current->pid);
	// timer_set(10000U);
	current->counter = 0;
	_schedule();
}

void switch_to(struct task_struct * next) 
{
	if (current == next) {
		// printf(".");
		return;
	}
		
	struct task_struct * prev = current;
	current = next;
	// printf("FInally making a switch \n");
	task_switch(prev, next);
}