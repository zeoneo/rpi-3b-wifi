#include <mem/frame_alloc.h>
#include <mem/kernel_alloc.h>
#include <plibc/stdio.h>

// This is rsta2/circle implementation of mem manager. I hope to write my own
// but till then all credits to rsta2 github

#define PACKED      __attribute__((packed))
#define ALIGN(n)    __attribute__((aligned(n)))
#define BLOCK_ALIGN 16
#define ALIGN_MASK  (BLOCK_ALIGN - 1)

#define PAGE_MASK      (PAGE_SIZE - 1)
#define BLOCK_MAGIC    0xDEADBEEF
#define FREEPAGE_MAGIC 0XBCADBCAD

struct PACKED alloc_header {
    uint32_t magic;
    uint32_t size;
    struct alloc_header* next;
    uint32_t padding;
    // Above padding is added to make struct size 16 Bytes.
    // this field data[0] is called struct hack it's size is zero.
    // https://www.geeksforgeeks.org/struct-hack/
    uint8_t block_data_ptr[0];
};

typedef struct PACKED {
    uint32_t size;
    struct alloc_header* free_list;
} alloc_bucket;

typedef struct {
    uint32_t nMagic;
    struct free_page* next;
} free_page;

typedef struct {
    free_page* free_list;
} page_bucket;

static uint8_t* s_pNextBlock;
static uint8_t* s_pBlockLimit;
static uint32_t s_nBlockReserve = 0x40000;

static alloc_bucket s_BlockBucket[] = {{0x40, 0},    {0x400, 0},   {0x1000, 0},  {0x4000, 0},
                                       {0x10000, 0}, {0x40000, 0}, {0x80000, 0}, {0, 0}};

#define MEGABYTE     0x100000
#define PAGE_SIZE    4096 // page size used by us
#define PAGE_RESERVE (4 * MEGABYTE)

int32_t kernel_alloc_init(uint32_t ulSize) {
    uint32_t ulBase = (uint32_t) alloc_contiguous_frames(ulSize / PAGE_SIZE);
    if (ulBase == 0) {
        printf("Couldnot allocate continuous memory. 0x%x \n", ulBase);
        return -1;
    }
    uint32_t ulLimit = ulBase + ulSize;

    ulSize                       = ulLimit - ulBase;
    unsigned long ulBlockReserve = ulSize - PAGE_RESERVE;

    s_pNextBlock  = (uint8_t*) ulBase;
    s_pBlockLimit = (uint8_t*) (ulBase + ulBlockReserve);
    printf("Kalloc start: 0x%x end:0x%x \n", ulBase, ulLimit);
    return 0;
}

void* kernel_allocate(uint32_t size) {

    alloc_bucket* pBucket;
    for (pBucket = s_BlockBucket; pBucket->size > 0; pBucket++) {
        if (size <= pBucket->size) {
            size = pBucket->size;
            break;
        }
    }

    struct alloc_header* pBlockHeader;
    if (pBucket->size > 0 && (pBlockHeader = pBucket->free_list) != 0 && pBlockHeader->magic == BLOCK_MAGIC) {
        pBucket->free_list = pBlockHeader->next;
    } else {
        pBlockHeader = (struct alloc_header*) s_pNextBlock;

        uint8_t* pNextBlock = s_pNextBlock;
        pNextBlock += (sizeof(struct alloc_header) + size + BLOCK_ALIGN - 1) & ~ALIGN_MASK;

        if (pNextBlock <= s_pNextBlock // may have wrapped
            || pNextBlock > s_pBlockLimit - s_nBlockReserve) {
            s_nBlockReserve = 0;
            return 0;
        }

        s_pNextBlock = pNextBlock;

        pBlockHeader->magic = BLOCK_MAGIC;
        pBlockHeader->size  = (unsigned) size;
    }

    pBlockHeader->next = 0;

    void* pResult = pBlockHeader->block_data_ptr;
    return pResult;
}

void kernel_deallocate(void* pBlock) {
    if (pBlock == 0) {
        return;
    }

    struct alloc_header* pBlockHeader = (struct alloc_header*) ((unsigned long) pBlock - sizeof(struct alloc_header));
    if (pBlockHeader->magic != BLOCK_MAGIC) {
        return;
    }

    for (alloc_bucket* pBucket = s_BlockBucket; pBucket->size > 0; pBucket++) {
        if (pBlockHeader->size == pBucket->size) {
            pBlockHeader->next = pBucket->free_list;
            pBucket->free_list = pBlockHeader;

            return;
        }
    }
}