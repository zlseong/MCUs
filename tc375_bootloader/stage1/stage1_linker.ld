/**
 * TC375 Stage 1 Bootloader Linker Script
 * 
 * Memory Layout:
 * - Flash: 0x80000000 - 0x8000FFFF (64 KB)
 * - RAM:   0x70000000 - 0x7000FFFF (64 KB)
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_reset)

MEMORY
{
    /* Stage 1 Bootloader Flash (64 KB) */
    STAGE1_FLASH (rx)  : ORIGIN = 0x80000000, LENGTH = 64K
    
    /* Stage 1 Bootloader RAM (64 KB) */
    STAGE1_RAM (rwx)   : ORIGIN = 0x70000000, LENGTH = 64K
}

SECTIONS
{
    /* Reset Vector - Must be at 0x80000000 */
    .boot_reset : {
        KEEP(*(.boot_reset))
        . = ALIGN(4);
    } > STAGE1_FLASH
    
    /* Code Section */
    .text : {
        *(.text)
        *(.text*)
        . = ALIGN(4);
    } > STAGE1_FLASH
    
    /* Read-only Data */
    .rodata : {
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > STAGE1_FLASH
    
    /* Initialized Data (loaded to RAM) */
    .data : {
        __data_start = .;
        *(.data)
        *(.data*)
        __data_end = .;
        . = ALIGN(4);
    } > STAGE1_RAM AT> STAGE1_FLASH
    
    __data_load_start = LOADADDR(.data);
    
    /* Uninitialized Data */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(4);
    } > STAGE1_RAM
    
    /* Stack */
    .stack : {
        . = ALIGN(8);
        __stack_start = .;
        . = . + 0x2000;  /* 8 KB stack */
        __stack_end = .;
    } > STAGE1_RAM
}

/* Symbols for Stage 2 locations */
PROVIDE(STAGE2A_START = 0x80011000);
PROVIDE(STAGE2B_START = 0x80041000);

