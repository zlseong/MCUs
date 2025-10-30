/**
 * Stage 1 Bootloader - Primary Bootloader
 * 
 * Role: Minimal, ROM-like, NEVER UPDATED
 * - Verify and select Stage 2 bootloader (A or B)
 * - Jump to Stage 2
 * 
 * Size: 64 KB
 * Location: 0x80000000 - 0x8000FFFF
 */

#include "../common/boot_common.h"

// Minimal hardware init
void stage1_init_hardware(void) {
    // 1. CPU 기본 설정
    // IfxCpu_setCoreMode(&MODULE_CPU0, IfxCpu_CoreMode_run);
    
    // 2. Watchdog 일시 중지 (Stage 1 동안만)
    // IfxScuWdt_clearCpuEndinit();
    
    // 3. Clock (최소한)
    // Set PLL to stable frequency
}

// Read boot config from EEPROM
BootBank stage1_read_active_stage2(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    // CRC 검증
    uint32_t calc_crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
    if (calc_crc != cfg->crc) {
        // Corruption! Default to Stage 2A
        return BANK_A;
    }
    
    return (cfg->stage2_active == 0) ? BANK_A : BANK_B;
}

// Update boot count in EEPROM
void stage1_increment_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    if (bank == BANK_A) {
        cfg->stage2_boot_cnt_a++;
    } else {
        cfg->stage2_boot_cnt_b++;
    }
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
}

// Reset boot count
void stage1_reset_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    if (bank == BANK_A) {
        cfg->stage2_boot_cnt_a = 0;
    } else {
        cfg->stage2_boot_cnt_b = 0;
    }
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
}

uint8_t stage1_get_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    return (bank == BANK_A) ? cfg->stage2_boot_cnt_a : cfg->stage2_boot_cnt_b;
}

// Switch to fallback Stage 2
void stage1_switch_to_fallback(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    // Toggle Stage 2
    cfg->stage2_active = (cfg->stage2_active == 0) ? 1 : 0;
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
    
    // Reset boot count for fallback
    if (cfg->stage2_active == 0) {
        cfg->stage2_boot_cnt_a = 0;
    } else {
        cfg->stage2_boot_cnt_b = 0;
    }
    
    debug_print("[Stage1] Switched to Stage 2 fallback\n");
    system_reset();
}

// Jump to Stage 2
void stage1_jump_to_stage2(uint32_t stage2_addr) {
    debug_print("[Stage1] Jumping to Stage 2...\n");
    
    // 1. Vector Table 읽기
    uint32_t* vector_table = (uint32_t*)stage2_addr;
    uint32_t stack_pointer = vector_table[0];
    uint32_t reset_handler = vector_table[1];
    
    // 2. Stack Pointer 설정
    __asm volatile("mov.a SP, %0" : : "d"(stack_pointer));
    
    // 3. Stage 2 Entry 점프
    void (*stage2_entry)(void) = (void(*)(void))reset_handler;
    stage2_entry();
    
    // Never return
    while(1);
}

// ============================================================================
// Stage 1 Main Entry Point
// ============================================================================

void __attribute__((section(".boot_reset"))) stage1_main(void) {
    // Phase 1: 최소 하드웨어 초기화
    stage1_init_hardware();
    
    debug_print("========================================\n");
    debug_print(" TC375 Stage 1 Bootloader v1.0\n");
    debug_print("========================================\n");
    
    // Phase 2: Stage 2 선택
    BootBank active_stage2 = stage1_read_active_stage2();
    
    BootMetadata* meta_2a = (BootMetadata*)STAGE2A_META;
    BootMetadata* meta_2b = (BootMetadata*)STAGE2B_META;
    
    BootMetadata* active_meta = (active_stage2 == BANK_A) ? meta_2a : meta_2b;
    uint32_t active_addr = (active_stage2 == BANK_A) ? STAGE2A_START : STAGE2B_START;
    
    debug_print("[Stage1] Active Stage 2: %c\n", 
               active_stage2 == BANK_A ? 'A' : 'B');
    
    // Phase 3: Boot Count 체크 (Fail-safe)
    stage1_increment_boot_count(active_stage2);
    uint8_t boot_cnt = stage1_get_boot_count(active_stage2);
    
    if (boot_cnt >= MAX_BOOT_ATTEMPTS) {
        debug_print("[Stage1] Stage 2 boot failed %d times, switching...\n", boot_cnt);
        stage1_switch_to_fallback();
        // system_reset() called inside, never return
    }
    
    // Phase 4: Stage 2 검증 (간단하게)
    
    // Magic number
    if (active_meta->magic != MAGIC_NUMBER) {
        debug_print("[Stage1] Invalid Stage 2 magic\n");
        stage1_switch_to_fallback();
    }
    
    // CRC 검증
    uint32_t calc_crc = calculate_crc32((uint8_t*)active_addr, active_meta->size);
    if (calc_crc != active_meta->crc32) {
        debug_print("[Stage1] Stage 2 CRC failed\n");
        stage1_switch_to_fallback();
    }
    
    // Phase 5: Stage 2로 점프
    debug_print("[Stage1] Stage 2 verified, jumping...\n\n");
    stage1_reset_boot_count(active_stage2);  // 검증 성공
    
    stage1_jump_to_stage2(active_addr);
    
    // Never reach here
    while(1);
}

