/**
 * Stage 2 Bootloader - Secondary Bootloader
 * 
 * Role: Full-featured bootloader, CAN BE UPDATED via OTA
 * - Verify and select Application (A or B)
 * - Full CRC and PQC signature verification
 * - Self-update capability
 * - Recovery mechanisms
 * 
 * Size: 188 KB (with libs)
 * Location A: 0x80011000 - 0x8003FFFF
 * Location B: 0x80041000 - 0x8006FFFF
 */

#include "../common/boot_common.h"

// ============================================================================
// Stage 2 Hardware Initialization
// ============================================================================

void stage2_init_hardware(void) {
    // 1. Full clock initialization
    // initPLL();
    // setCpuFrequency(300000000);  // 300 MHz
    
    // 2. Watchdog configuration
    // IfxScuWdt_enableSafetyWatchdog(5000);  // 5 sec timeout
    
    // 3. UART for debugging (optional)
    // initUART0(115200);
    
    // 4. Flash controller
    // IfxFlash_init();
    
    debug_print("[Stage2] Hardware initialized\n");
}

// ============================================================================
// Application Bank Management
// ============================================================================

BootBank stage2_read_active_app(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    return (cfg->app_active == 0) ? BANK_A : BANK_B;
}

void stage2_increment_app_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    if (bank == BANK_A) {
        cfg->app_boot_cnt_a++;
    } else {
        cfg->app_boot_cnt_b++;
    }
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
}

void stage2_reset_app_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    if (bank == BANK_A) {
        cfg->app_boot_cnt_a = 0;
    } else {
        cfg->app_boot_cnt_b = 0;
    }
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
}

uint8_t stage2_get_app_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    return (bank == BANK_A) ? cfg->app_boot_cnt_a : cfg->app_boot_cnt_b;
}

void stage2_switch_to_fallback_app(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    // Toggle Application bank
    cfg->app_active = (cfg->app_active == 0) ? 1 : 0;
    
    // Reset boot count
    if (cfg->app_active == 0) {
        cfg->app_boot_cnt_a = 0;
    } else {
        cfg->app_boot_cnt_b = 0;
    }
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
    
    debug_print("[Stage2] Switched to App fallback\n");
    system_reset();
}

// ============================================================================
// Application Verification (Full)
// ============================================================================

bool stage2_verify_application(BootBank bank) {
    BootMetadata* meta = (bank == BANK_A) ? 
        (BootMetadata*)APP_A_META : (BootMetadata*)APP_B_META;
    
    uint32_t app_start = (bank == BANK_A) ? APP_A_START : APP_B_START;
    
    debug_print("[Stage2] Verifying App %c...\n", bank == BANK_A ? 'A' : 'B');
    
    // 1. Magic Number
    if (meta->magic != MAGIC_NUMBER) {
        debug_print("[Stage2]   Magic: FAIL\n");
        return false;
    }
    debug_print("[Stage2]   Magic: OK\n");
    
    // 2. Valid Flag
    if (meta->valid != 1) {
        debug_print("[Stage2]   Valid flag: FAIL\n");
        return false;
    }
    debug_print("[Stage2]   Valid: OK\n");
    
    // 3. CRC32 (Full firmware)
    uint32_t calc_crc = calculate_crc32((uint8_t*)app_start, meta->size);
    if (calc_crc != meta->crc32) {
        debug_print("[Stage2]   CRC: FAIL (calc=%08X, expect=%08X)\n", 
                   calc_crc, meta->crc32);
        return false;
    }
    debug_print("[Stage2]   CRC: OK\n");
    
    // 4. PQC Signature (Dilithium3)
    if (!verify_dilithium_signature((uint8_t*)app_start, meta->size, meta->signature)) {
        debug_print("[Stage2]   Signature: FAIL\n");
        return false;
    }
    debug_print("[Stage2]   Signature: OK\n");
    
    debug_print("[Stage2] Verification: PASSED\n");
    return true;
}

// ============================================================================
// Jump to Application
// ============================================================================

void stage2_jump_to_application(uint32_t app_addr) {
    debug_print("[Stage2] Jumping to Application at 0x%08X\n", app_addr);
    
    // 1. Vector Table
    uint32_t* vectors = (uint32_t*)app_addr;
    uint32_t sp = vectors[0];  // Stack Pointer
    uint32_t pc = vectors[1];  // Program Counter (Reset Handler)
    
    // 2. Watchdog 재활성화
    // IfxScuWdt_setCpuEndinit();
    
    // 3. Stack 설정
    __asm volatile("mov.a SP, %0" : : "d"(sp));
    
    // 4. Application 실행
    void (*app_main)(void) = (void(*)(void))pc;
    app_main();
    
    // Never return
    while(1);
}

// ============================================================================
// Stage 2 Main Entry Point
// ============================================================================

void stage2_main(void) {
    // Phase 1: 하드웨어 초기화
    stage2_init_hardware();
    
    debug_print("========================================\n");
    debug_print(" TC375 Stage 2 Bootloader v1.0\n");
    debug_print("========================================\n");
    
    // Phase 2: Application 선택
    BootBank active_app = stage2_read_active_app();
    
    BootMetadata* meta_a = (BootMetadata*)APP_A_META;
    BootMetadata* meta_b = (BootMetadata*)APP_B_META;
    
    BootMetadata* active_meta = (active_app == BANK_A) ? meta_a : meta_b;
    uint32_t active_addr = (active_app == BANK_A) ? APP_A_START : APP_B_START;
    
    debug_print("[Stage2] Active Application: %c\n", 
               active_app == BANK_A ? 'A' : 'B');
    debug_print("[Stage2] Version: %d\n", active_meta->version);
    
    // Phase 3: Boot Count 체크
    stage2_increment_app_boot_count(active_app);
    uint8_t boot_cnt = stage2_get_app_boot_count(active_app);
    
    if (boot_cnt >= MAX_BOOT_ATTEMPTS) {
        debug_print("[Stage2] App failed %d times, rollback!\n", boot_cnt);
        stage2_switch_to_fallback_app();
        // Never return (system_reset inside)
    }
    
    // Phase 4: Application 검증
    if (!stage2_verify_application(active_app)) {
        debug_print("[Stage2] Application verification FAILED\n");
        stage2_switch_to_fallback_app();
    }
    
    // Phase 5: Application 점프
    stage2_reset_app_boot_count(active_app);
    stage2_jump_to_application(active_addr);
    
    // Never reach here
    while(1);
}

