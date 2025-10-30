/**
 * TC375 Stage 2A Bootloader Linker Script
 * 
 * Location: 0x80011000 - 0x8003FFFF (188 KB)
 * This is the PRIMARY Stage 2 bootloader
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(stage2_main)

MEMORY
{
    /* Stage 2A Bootloader Flash (188 KB) */
    STAGE2_FLASH (rx)  : ORIGIN = 0x80011000, LENGTH = 188K
    
    /* Stage 2 Bootloader RAM (128 KB) - Shared with 2B */
    STAGE2_RAM (rwx)   : ORIGIN = 0x70010000, LENGTH = 128K
}

SECTIONS
{
    /* Vector Table */
    .vectors : {
        KEEP(*(.vectors))
        . = ALIGN(4);
    } > STAGE2_FLASH
    
    /* Code */
    .text : {
        *(.text)
        *(.text*)
        . = ALIGN(4);
    } > STAGE2_FLASH
    
    /* Read-only Data */
    .rodata : {
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > STAGE2_FLASH
    
    /* Data */
    .data : {
        __data_start = .;
        *(.data)
        *(.data*)
        __data_end = .;
        . = ALIGN(4);
    } > STAGE2_RAM AT> STAGE2_FLASH
    
    __data_load_start = LOADADDR(.data);
    
    /* BSS */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(4);
    } > STAGE2_RAM
    
    /* Stack */
    .stack : {
        . = ALIGN(8);
        __stack_start = .;
        . = . + 0x4000;  /* 16 KB stack */
        __stack_end = .;
    } > STAGE2_RAM
}

/* Application locations (for Stage 2 to jump to) */
PROVIDE(APP_A_START = 0x80071000);
PROVIDE(APP_B_START = 0x802F2000);


