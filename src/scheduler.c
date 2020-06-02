#include <kernel/scheduler.h>
#include <kernel/systimer.h>
#include <plibc/stdio.h>

// Implementation here is inspired and to some extent copied from R. Strange.
// I will write my own implementation later. For now I want to get wifi working

#define NR_TASKS 64
#define INIT_TASK                                                                                                      \
    /*cpu_context*/ { {0}, /* state etc */ 0, 0, 1, 0, 0, 0, "init" }

task_struct_t init_task        = INIT_TASK;
task_struct_t* current         = &(init_task);
task_struct_t* TASKS[NR_TASKS] = {
    &(init_task),
};

uint32_t nr_tasks = 1;

extern void task_switch(task_struct_t* old, task_struct_t* new);
extern void delete_task(task_struct_t* task_to_delete);

static void _schedule(void);

void preempt_disable(void) {
    current->preempt_count++;
}

void preempt_enable(void) {
    current->preempt_count--;
}

void schedule_tail(void) {
    printf("Schedule tail called");
    preempt_enable();
}

static void schedule_tick();

void init_scheduler() {
    // repeat_on_time_out(schedule_tick, 200000);
    repeat_on_time_out(schedule_tick, 100000);
    current->priority = 1;
}

static void schedule_tick() {
    --(current->counter);
    if (current->preempt_count > 0) {
        return;
    }
    current->counter = 0;
    _enable_interrupts();
    _schedule();
    // printf("__\n");
    DISABLE_INTERRUPTS();
}

task_struct_t* get_task_by_pid(int pid) {
    return TASKS[pid];
}

void schedule(void) {
    // printf("calling scheduler current pid: %d \n", current->pid);
    // timer_set(10000U);
    current->counter = 0;
    _schedule();
}

static void switch_to(struct task_struct* next) {
    if (current == next) {
        return;
    }
    struct task_struct* prev = current;
    current                  = next;
    // printf("__changing from: %d to %d ___\n", prev->pid, next->pid);
    task_switch(prev, next);
}

void block_this_task() {
    printf("trying to block task name: %s \n", current->name);
    if (current->pid == 0) {
        return;
    }

    current->state = TaskStateBlocked;
    schedule();
}

void wake_task(task_struct_t* pTask) {
    printf("trying to wake task name: %s \n", pTask->name);
    if (pTask->pid == 0) {
        return;
    }
    pTask->state = TaskStateReady;
}

int add_task(task_struct_t* pTask) {
    preempt_disable();
    if (pTask == 0) {
        return 0;
    }
    uint32_t i;
    for (i = 0; i < NR_TASKS; i++) {
        if (TASKS[i] == 0) {
            TASKS[i]        = pTask;
            TASKS[i]->pid   = i;
            pTask->priority = current->priority;
            pTask->counter  = current->priority;
            nr_tasks++;
            break;
        }
    }

    if (i >= NR_TASKS) {
        printf("System limit of tasks exceeded \n");
        return 0;
    }
    preempt_enable();
    return i;
}
static void remove_task(task_struct_t* pTask);

void terminate_this_task() {
    printf("Removing task: %s \n", current->name);
    remove_task(current);
    schedule();
}

static void remove_task(task_struct_t* pTask) {
    if (pTask->pid == 0) {
        return;
    }

    for (uint32_t i = 0; i < nr_tasks; i++) {
        if (TASKS[i] == pTask) {
            TASKS[i] = 0;
            if (i == nr_tasks - 1) {
                nr_tasks--;
            }
            delete_task(pTask);
            return;
        }
    }
}

// Is this buggy implementation
// What happens when all the tasks want to run indefinitely.
// they never get blocked, just preempted by schedular.
// TODO: Check the bug here.
static uint32_t get_next_task() {

    uint32_t nTask = current->pid < NR_TASKS ? current->pid : 0;

    uint32_t nTicks = timer_getTickCount32();

    for (unsigned i = 1; i <= NR_TASKS; i++) {
        if (++nTask >= nr_tasks) {
            nTask = 0;
        }

        task_struct_t* pTask = TASKS[nTask];
        if (pTask == 0) {
            continue;
        }

        switch (pTask->state) {
        case TaskStateReady:
            return nTask;

        case TaskStateBlocked:
            continue;

        case TaskStateSleeping:
            if ((int) (pTask->wake_ticks - nTicks) > 0) {
                continue;
            }
            pTask->state = TaskStateReady;
            return nTask;

        case TaskStateTerminated:
            // call the task terminal handler here.
            remove_task(pTask);
            // return NR_TASKS;
            break;

        default:
            printf(" Error in schedular alogirithm \n");
            break;
        }
    }

    return NR_TASKS;
}

void _schedule(void) {
    preempt_disable();

    uint32_t current_pid;
    while ((current_pid = get_next_task()) == NR_TASKS) // no TASKS is ready
    {
        // assert (m_nTasks > 0);
    }
    // printf("New pid to run: %d \n", current->pid);
    // assert (m_nCurrent < MAX_TASKS);
    task_struct_t* pNext = TASKS[current_pid];
    // assert(pNext != 0);
    if (pNext == 0 || current == pNext) {
        preempt_enable();
        // printf("Preempt enable. Returning same task \n");
        return;
    }

    // if (m_pTaskSwitchHandler != 0)
    // {
    // 	(*m_pTaskSwitchHandler) (m_pCurrent);
    // }
    // Call task switch handler callback here.
    // some task need to know when they are scheduled off the cpu.
    switch_to(pNext);
    preempt_enable();
    // printf("Preempt enable. Returning after switch to func \n");
}

void us_sleep(uint32_t nMicroSeconds) {
    if (nMicroSeconds > 0) {
        uint32_t nTicks = nMicroSeconds * (CLOCKHZ / 1000000);

        uint32_t nStartTicks = timer_getTickCount32();

        // No need to track task running pid.
        if (current->pid == 0) {
            return;
        }
        current->wake_ticks = (nStartTicks + nTicks);
        current->state      = TaskStateSleeping;
        schedule();
    }
}

// void sleep(uint32_t nSeconds) {
//     // be sure the clock does not run over taken as signed int
//     const unsigned nSleepMax = 1800; // normally 2147 but to be sure
//     while (nSeconds > nSleepMax) {
//         us_sleep(nSleepMax * 1000000);

//         nSeconds -= nSleepMax;
//     }
//     us_sleep(nSeconds * 1000000);
// }

void ms_sleep(uint32_t nMilliSeconds) {
    if (nMilliSeconds > 0) {
        us_sleep(nMilliSeconds * 1000);
    }
}

#define USED(x)                                                                                                        \
    if (x)                                                                                                             \
        ;                                                                                                              \
    else {                                                                                                             \
    }

void sleep(void* rendez, sleephandler_t* handler, void* param) {
    USED(rendez);
    do {
        schedule();
    } while ((*handler)(param) == 0);
}

void tsleep(void* rendez, sleephandler_t* handler, void* param, unsigned msecs) {
    USED(rendez);
    unsigned start = timer_getTickCount32();
    do {
        if ((timer_getTickCount32() - start) > ((msecs * 100) / 1000)) {
            break;
        }
        schedule();
    } while ((*handler)(param) == 0);
}

void wakeup(void* rendez) {
    USED(rendez);
}

int return0(void* param) {
    USED(param);
    return 0;
}