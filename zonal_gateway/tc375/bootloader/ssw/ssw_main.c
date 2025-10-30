/**
 * Stage 1 Bootloader - Primary Bootloader (SSW - Startup Software)
 * 
 * Role: Minimal, ROM-like, NEVER UPDATED
 * - Verify and select Stage 2 bootloader (A or B)
 * - Jump to Stage 2
 * 
 * Size: 64 KB
 * Location: 0x80000100 - 0x8000FFFF (Region A)
 * 
 * This is for Zonal Gateway (TC375)
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
    
    // Write back to EEPROM
    // Flash_Write(BOOT_CFG_EEPROM, cfg, sizeof(BootConfig));
}

// Check boot attempt limit
bool stage1_check_boot_limit(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    if (bank == BANK_A) {
        return cfg->stage2_boot_cnt_a < MAX_BOOT_ATTEMPTS;
    } else {
        return cfg->stage2_boot_cnt_b < MAX_BOOT_ATTEMPTS;
    }
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

// Verify Stage 2 metadata
bool stage1_verify_stage2(uint32_t stage2_addr) {
    BootMetadata* meta = (BootMetadata*)stage2_addr;
    
    // 1. Magic number check
    if (meta->magic != MAGIC_NUMBER) {
        return false;
    }
    
    // 2. Valid flag check
    if (meta->valid != 1) {
        return false;
    }
    
    // 3. CRC32 verification
    uint32_t* code_start = (uint32_t*)(stage2_addr + 0x1000);
    uint32_t calc_crc = calculate_crc32((uint8_t*)code_start, meta->size);
    if (calc_crc != meta->crc32) {
        return false;
    }
    
    // 4. Signature verification (PQC Dilithium)
    // if (!verify_dilithium_signature((uint8_t*)code_start, meta->size, meta->signature)) {
    //     return false;
    // }
    
    return true;
}

// Jump to Stage 2
void stage1_jump_to_stage2(uint32_t stage2_addr) {
    // Stage 2 code start address
    uint32_t entry_point = stage2_addr + 0x1000;
    
    // Function pointer
    void (*stage2_main)(void) = (void (*)(void))entry_point;
    
    // Jump!
    stage2_main();
    
    // Should never return
    while (1);
}

// Fallback: Try to boot from backup Stage 2
void stage1_fallback(void) {
    debug_print("[SSW] All Stage 2 options failed! Entering safe mode...\n");
    
    // Safe mode: Infinite loop with LED blink or diagnostic output
    while (1) {
        // LED blink or CAN diagnostic message
        for (volatile int i = 0; i < 1000000; i++);
    }
}

// Main function
void _start(void) __attribute__((section(".ssw_start")));

void _start(void) {
    // 1. Minimal hardware init
    stage1_init_hardware();
    
    debug_print("[SSW] Stage 1 Bootloader Started (Zonal Gateway)\n");
    debug_print("[SSW] TC375 Hardware Bank Switching\n");
    
    // 2. Read active Stage 2 from EEPROM
    BootBank active_bank = stage1_read_active_stage2();
    
    debug_print("[SSW] Active Stage 2: %s\n", 
                (active_bank == BANK_A) ? "Bank A" : "Bank B");
    
    // 3. Check boot attempt limit
    if (!stage1_check_boot_limit(active_bank)) {
        debug_print("[SSW] Boot limit exceeded for active bank! Switching...\n");
        active_bank = (active_bank == BANK_A) ? BANK_B : BANK_A;
        stage1_reset_boot_count(active_bank);
    }
    
    // 4. Increment boot count
    stage1_increment_boot_count(active_bank);
    
    // 5. Verify Stage 2
    uint32_t stage2_addr = (active_bank == BANK_A) ? REGION_A_BOOT_META : REGION_B_BOOT_META;
    
    if (stage1_verify_stage2(stage2_addr)) {
        debug_print("[SSW] Stage 2 verification OK. Jumping...\n");
        stage1_jump_to_stage2(stage2_addr);
    } else {
        debug_print("[SSW] Stage 2 verification FAILED!\n");
        
        // Try backup bank
        BootBank backup_bank = (active_bank == BANK_A) ? BANK_B : BANK_A;
        uint32_t backup_addr = (backup_bank == BANK_A) ? REGION_A_BOOT_META : REGION_B_BOOT_META;
        
        if (stage1_verify_stage2(backup_addr)) {
            debug_print("[SSW] Backup Stage 2 OK. Jumping...\n");
            stage1_jump_to_stage2(backup_addr);
        } else {
            // Both failed!
            stage1_fallback();
        }
    }
    
    // Should never reach here
    while (1);
}

