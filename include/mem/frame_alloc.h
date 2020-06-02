#ifndef _FRAME_ALLOC_H
#define _FRAME_ALLOC_H

#include <kernel/list.h>
#include <stdint.h>

#define PAGE_SIZE 4096

void init_frame_manager();
void show_current_memory_states();
uint32_t get_num_of_free_frames();
void* alloc_frame(void);

void* alloc_contiguous_frames(uint32_t num_of_frames);
void* alloc_cont_aligned_frames(uint32_t num_of_frames, uint32_t align);

void dealloc_frame(void* ptr);
void dealloc_cont_frames(void* ptr, uint32_t num_of_frames);

#endif