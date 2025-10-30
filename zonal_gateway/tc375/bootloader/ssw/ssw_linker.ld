/**
 * SSW Linker Script for TC375 Zonal Gateway
 * Stage 1 Bootloader (Startup Software)
 * 
 * Location: 0x80000100 - 0x8000FFFF (Region A, 64KB)
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)

MEMORY
{
    /* SSW Region (Stage 1 Bootloader) */
    SSW (rx) : ORIGIN = 0x80000100, LENGTH = 64K - 0x100
    
    /* Data RAM */
    DSRAM (rw) : ORIGIN = 0x70000000, LENGTH = 192K
}

SECTIONS
{
    /* SSW Start Section */
    .ssw_start 0x80000100 :
    {
        KEEP(*(.ssw_start))
        *(.text._start)
    } > SSW
    
    /* Code Section */
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > SSW
    
    /* Data Section */
    .data :
    {
        *(.data)
        *(.data.*)
    } > DSRAM AT> SSW
    
    /* BSS Section */
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } > DSRAM
    
    /* Stack */
    .stack :
    {
        . = ALIGN(8);
        __stack_start = .;
        . = . + 4K;
        __stack_end = .;
    } > DSRAM
}

