#ifndef _BCM_RANDOM_GENERATOR_H
#define _BCM_RANDOM_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include<stdint.h>

void initialize_random_generator();
uint32_t get_random_int();

#ifdef __cplusplus
}
#endif

#endif
