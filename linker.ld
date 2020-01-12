ENTRY(_start)
 
SECTIONS
{
    /* Starts at LOADER_ADDR. */
    . = 0x8000;
    __start = .;
    __text_start = .;
    .text :
    {
        KEEP(*(.text.boot))
        *(.text)
    }
    . = ALIGN(4096); /* align to page size */
    __text_end = .;
 
    __rodata_start = .;
    .rodata :
    {
        *(.rodata)
    }
    . = ALIGN(4096); /* align to page size */
    __rodata_end = .;
 
    __data_start = .;
    .data :
    {
        *(.data)
    }
    . = ALIGN(4096); /* align to page size */
    __data_end = .;
 
    __bss_start = .;
    .bss :
    {
        bss = .;
        *(.bss)
    }
    . = ALIGN(4096); /* align to page size */
    __bss_end = .;

    . = . + 131072; /* 128KB for svc stack */
    __svc_stack_base = .;

    . = . + 1024; /* 1024 Bytes for irq stack */
    __irq_stack_base = .;

    . = ALIGN(16384); /* align 16KB for page tables */
    __first_lvl_tbl_base = .;
    . = . + 4096; /* First level page table size */
    __first_lvl_tbl_end = .;

    . = ALIGN(1024); /* Align address here on 1KB boundary for 2nd level page table */
    /* 1KB for each 2nd lvl page table = 4 Bytes * 256 */
    /* How many 2nd lvl tables ? */
    __second_lvl_tbl_base = .;
    . = . + 1048576; /* 1MB space for 2nd lvl tables. */
    __second_lvl_tbl_end = .;

    /* DMA demo starts here */
        . = ALIGN(4096);
    dma_cb_page = .;
    . = . + 4096; /* 4 KB for DMA control blocks */

    dma_src_page_1 = . ;
    . = . + 65536; /* 64 Kb of data */

    dma_src_page_2 = . ;
    . = . + 65536; /* 64 Kb of data */

    dma_dest_page_1 = . ;
    . = . + 65536; /* 64 Kb of data */

    dma_dest_page_2 = . ;
    . = . + 65536; /* 64 Kb of data */

    __kernel_end = .;
}