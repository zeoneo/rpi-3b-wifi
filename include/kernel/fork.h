
#ifndef _FORK_H_
#define _FORK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

int copy_process(uint32_t fn, uint32_t arg);

#ifdef __cplusplus
}
#endif

#endif // _FORK_H_