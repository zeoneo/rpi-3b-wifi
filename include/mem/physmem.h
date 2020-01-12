#ifndef PHYS_MEM_H
#define PHYS_MEM_H

#include <stdint.h>
#include <kernel/list.h>

#define PAGE_SIZE 4096

typedef struct
{
    uint8_t allocated : 1;   // This page is allocated to something
    uint8_t kernel_page : 1; // This page is a part of the kernel
    uint32_t reserved : 30;
} page_flags_t;

typedef struct page
{
    uint32_t vaddr_mapped; // The virtual address that maps to this page
    page_flags_t flags;
    DEFINE_LINK(page);
} page_t;

void mem_init();
uint32_t get_mem_size();
uint32_t get_num_of_free_pages();
void *alloc_page(void);
void free_page(void *ptr);

#endif