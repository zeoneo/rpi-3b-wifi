
#ifndef _FORK_H_
#define _FORK_H_

#include <kernel/scheduler.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int create_task(char* name, uint32_t fn, uint32_t arg);
void delete_process(task_struct_t* task_to_delete);

#ifdef __cplusplus
}
#endif

#endif // _FORK_H_