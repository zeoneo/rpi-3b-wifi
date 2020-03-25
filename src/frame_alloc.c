#include <plibc/stdio.h>
#include <stdint.h>
#include <mem/frame_alloc.h>
#include <kernel/rpi-mailbox-interface.h>
#include <kernel/list.h>

// https://github.com/littleosbook/littleosbook/blob/master/page_frame_allocation.md

typedef struct frame_manager_state
{
    uint32_t is_initialized;
    uint32_t total_memory;
    uint32_t total_frames;
    uint32_t kernel_usage;
    uint32_t frame_manager_usage;
    uint32_t available_frames;
    uint32_t *frame_bits_array;
    uint32_t next_idx;
} frame_manager_state_t;

#define FRAME_BYTE_IDX(frame_idx) ( frame_idx / 32)   // Get the byte which has allocation info for current frame.
#define FRAME_BIT_OFFSET(frame_idx) (frame_idx % 32) // This gets the bit position

// External Symbols
extern uint8_t __kernel_end;
extern void bzero(void *, uint32_t);


// Internal private methods.
static void mark_as_used(uint32_t frame_idx);
static void mark_as_free(uint32_t frame_idx);
static uint32_t find_free_frame_in_rage(uint32_t start_frame_idx, uint32_t end_frame_idx);
static uint32_t find_free_frame();
static uint32_t get_mem_size();
static uint32_t find_contiguous_frames_in_ranges(uint32_t start_frame_idx, uint32_t end_frame_idx, uint32_t num_of_frames, uint32_t alignment);
static uint32_t find_contiguous_frames(uint32_t num_of_frames, uint32_t align);

// Implementation specifics
static frame_manager_state_t current_state = {0};


void init_frame_manager()
{
    uint32_t page_array_len, kernel_pages;

    // Get the total number of pages
    current_state.total_memory = get_mem_size();
    current_state.total_frames = current_state.total_memory / PAGE_SIZE;
    
    // Initially all the frames are avaiable
    current_state.available_frames = current_state.total_frames;


    // Allocate space for all those pages' metadata.  Start this block just after the kernel image is finished
    current_state.frame_bits_array = (uint32_t *)&__kernel_end;
    page_array_len = current_state.total_frames / 32;


    current_state.frame_manager_usage = page_array_len / PAGE_SIZE;

    if(page_array_len % PAGE_SIZE != 0) {
        current_state.frame_manager_usage += 1; // Add 1 more page to usage. We should allocate complete page.
    }

    // Clear the old data.
    bzero((void *)current_state.frame_bits_array, page_array_len);


    // Number of pages kernel spans currently.
    kernel_pages = ((uint32_t)&__kernel_end) / PAGE_SIZE;
    
    current_state.kernel_usage = kernel_pages;

    // mark frames used for frame manager as used. 
    kernel_pages = kernel_pages + current_state.frame_manager_usage;


    for (uint32_t frame_idx = 0; frame_idx < kernel_pages; frame_idx++)
    {
        mark_as_used(frame_idx);
    }
    current_state.available_frames = 20;
    current_state.next_idx = FRAME_BYTE_IDX(kernel_pages) + 1;
    current_state.is_initialized = 1;
}

uint32_t get_num_of_free_frames()
{
    return current_state.available_frames;
}

void show_current_memory_states() {
    printf("\n------------MEMORY-STATISTICS-----------------\n");
    printf("Total Memory in KBytes : %d \n", current_state.total_memory / 1024);
    printf("Total Frames : %d \n", current_state.total_frames);
    printf("Available Frames : %d \n", current_state.available_frames);
    printf("Kernel Usage : %d Memory Manager : %d \n", current_state.kernel_usage, current_state.frame_manager_usage);
    printf("Next Byte Index : %d \n", current_state.next_idx);
    printf(" ----------------------------------------------\n");
};

void * alloc_frame(void)
{   
    if(current_state.available_frames == 0) {
        return 0;
    }

    uint32_t free_frame = find_free_frame();
    if(free_frame == 0) {
        return 0; // We didn;t find a frame
    }

    uint32_t free_frame_idx = free_frame / PAGE_SIZE;
    bzero((void *)free_frame, PAGE_SIZE);
    mark_as_used(free_frame_idx);
    current_state.next_idx = FRAME_BYTE_IDX(free_frame_idx);
    return (void *) free_frame;
}

void * alloc_contiguous_frames(uint32_t num_of_frames) {
    return alloc_cont_aligned_frames(num_of_frames, PAGE_SIZE);
}

void * alloc_cont_aligned_frames(uint32_t num_of_frames, uint32_t align) {
    if(num_of_frames > current_state.available_frames) {
        return 0;
    }

    uint32_t frame_addr = find_contiguous_frames(num_of_frames, align);
    if(frame_addr == 0) {
        return 0; // No allocation
    }

    uint32_t free_frame_idx = 0;
    uint32_t base_address = 0;
    for(uint32_t i = 0; i < num_of_frames; i++) {
        base_address = ((uint32_t) frame_addr + (i * PAGE_SIZE));
        bzero((void *)base_address, PAGE_SIZE);
        free_frame_idx = base_address / PAGE_SIZE;
        mark_as_used(free_frame_idx);
    }
    current_state.next_idx = FRAME_BYTE_IDX(free_frame_idx);
    return (void *) frame_addr;
}

void dealloc_cont_frames(void *ptr, uint32_t num_of_frames) {
    uint32_t frame_address;
    for(int32_t i = num_of_frames -1; i >= 0; i--) {
        frame_address = (((uint32_t)ptr) + (i * PAGE_SIZE));
        dealloc_frame((void *) frame_address);
    }
}

void dealloc_frame(void *ptr)
{
    uint32_t frame_address = (uint32_t) ptr;
    uint32_t frame_idx = frame_address / PAGE_SIZE;
    if(frame_idx <= (current_state.kernel_usage + current_state.frame_manager_usage)) {
        return;// Someone is trying dealloc kernel :D
    }
    current_state.next_idx = FRAME_BYTE_IDX(frame_idx);
    mark_as_free(frame_idx);
}

static uint32_t get_mem_size()
{
    RPI_PropertyInit();
    RPI_PropertyAddTag(TAG_GET_ARM_MEMORY);
    RPI_PropertyProcess();

    rpi_mailbox_property_t *mp;
    mp = RPI_PropertyGet(TAG_GET_ARM_MEMORY);

    if (mp)
    {
        return (uint32_t)(mp->data.buffer_32[1]);
    }
    return 0;
}


static void mark_as_used(uint32_t frame_idx) {
    uint32_t byte_idx = FRAME_BYTE_IDX(frame_idx);
    uint32_t bit_offset = FRAME_BIT_OFFSET(frame_idx);
    current_state.frame_bits_array[byte_idx] |= ( 1 << bit_offset);
    current_state.available_frames--;
}

static void mark_as_free(uint32_t frame_idx) {
    uint32_t byte_idx = FRAME_BYTE_IDX(frame_idx);
    uint32_t bit_offset = FRAME_BIT_OFFSET(frame_idx);
    current_state.frame_bits_array[byte_idx] &= ~(1 << bit_offset); //clear the bit
    current_state.available_frames++;
}

static uint32_t find_free_frame() {
    // Search from next index to total frames
    uint32_t free_frame_address = find_free_frame_in_rage(current_state.next_idx, current_state.total_frames);
    if(free_frame_address == 0 && current_state.available_frames != 0) {

        // We know available frames count is non zero that means we have free pages from 0 to next->index.
        // This can happen if all the memory is allocated and later dellocated in random order.
        free_frame_address = find_free_frame_in_rage(current_state.kernel_usage + current_state.frame_manager_usage, current_state.next_idx - 1);
    }
    return free_frame_address;
}

static uint32_t find_free_frame_in_rage(uint32_t start_frame_idx, uint32_t end_frame_idx) {
    uint32_t offset;
    for (uint32_t frame_idx = start_frame_idx; frame_idx < end_frame_idx; frame_idx++)
    {
        if (current_state.frame_bits_array[frame_idx] != 0xFFFFFFFF) // Only check there is any free bit
        {
            // at least one bit is free here.
            for (offset = 0; offset < 32; offset++)
            {
                if(!(current_state.frame_bits_array[frame_idx] & (1 << offset))) {
                    return ((frame_idx * 32) + offset) * PAGE_SIZE;
                }
            }
        }
    }
    // failed to locate any free frame
    return 0;
}

static int32_t is_aligned(uint32_t mult_of_page_size, uint32_t aligment) {
    if(aligment == PAGE_SIZE || mult_of_page_size == 0) {
        return -1; // Always aligned;
    }

    uint32_t req_mult_of_page_size = aligment / PAGE_SIZE;
    return mult_of_page_size % req_mult_of_page_size  == 0 ? -1 : 0;

}

static uint32_t find_contiguous_frames(uint32_t num_of_frames, uint32_t alignment) {
    uint32_t frame_address = find_contiguous_frames_in_ranges(current_state.next_idx, current_state.total_frames, num_of_frames, alignment);
    if(frame_address == 0 && current_state.available_frames >= num_of_frames) {
        // there might be a chance that we get contiguous allocation.
        frame_address = find_contiguous_frames_in_ranges(current_state.kernel_usage + current_state.frame_manager_usage, current_state.next_idx - 1, num_of_frames, alignment);
    }
    return frame_address;
}

static uint32_t find_contiguous_frames_in_ranges(uint32_t start_frame_idx, uint32_t end_frame_idx, uint32_t num_of_frames, uint32_t alignment) {
    uint32_t offset, count;

    uint32_t start_address = 0;
    count = 0;
    for (uint32_t byte_idx = start_frame_idx; byte_idx < end_frame_idx; byte_idx++)
    {
        if (current_state.frame_bits_array[byte_idx] != 0xFFFFFFFF) // Only check there is any free bit
        {
            // at least one bit is free here.
            for (offset = 0; offset < 32; offset++)
            {
                uint32_t is_available = !(current_state.frame_bits_array[byte_idx] & (1 << offset));
                uint32_t mult_of_page_size = (byte_idx * 32 ) + offset;
                if(is_available) {
                    if(start_address == 0 && is_aligned(mult_of_page_size, alignment) ) {
                        start_address = ((byte_idx * 32) + offset) * PAGE_SIZE;
                    }
                    if(start_address != 0) {
                        count++;
                    }
                } else {
                    // this bit is not available to us. Start searching again.
                    if(start_address != 0) {
                        start_address = 0;
                        count = 0;
                    }
                }
                if(count == num_of_frames) {
                    return start_address;
                }
            }
        }
    }

    // Allocation failed
    return 0;
}