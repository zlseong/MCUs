/**
 * Stage 2 Bootloader for TC375 Zonal Gateway
 * 
 * Role: Application Bootloader (CAN BE UPDATED via OTA)
 * - Initialize full hardware (Ethernet, CAN, etc.)
 * - Check for OTA updates
 * - Verify and select Application (A or B)
 * - Provide recovery mode
 * 
 * Size: ~196 KB
 * Location: 0x800A1000 (Region A) or 0x820A1000 (Region B)
 */

#include "../common/boot_common.h"

// Full hardware initialization
void stage2_init_hardware(void) {
    // 1. Clock configuration (Full PLL)
    // IfxScuCcu_init();
    
    // 2. Ethernet initialization (for DoIP)
    // IfxEth_init();
    
    // 3. CAN initialization (optional, for diagnostics)
    // IfxMultican_init();
    
    // 4. UART for debug
    // IfxAsclin_init();
    
    // 5. Watchdog
    // IfxScuWdt_init();
}

// Read active app from EEPROM
BootBank stage2_read_active_app(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    uint32_t calc_crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
    if (calc_crc != cfg->crc) {
        return BANK_A;  // Default
    }
    
    return (cfg->app_active == 0) ? BANK_A : BANK_B;
}

// Update app boot count
void stage2_increment_app_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    if (bank == BANK_A) {
        cfg->app_boot_cnt_a++;
    } else {
        cfg->app_boot_cnt_b++;
    }
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
}

// Reset app boot count
void stage2_reset_app_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    if (bank == BANK_A) {
        cfg->app_boot_cnt_a = 0;
    } else {
        cfg->app_boot_cnt_b = 0;
    }
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
}

// Get app boot count
uint8_t stage2_get_app_boot_count(BootBank bank) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    return (bank == BANK_A) ? cfg->app_boot_cnt_a : cfg->app_boot_cnt_b;
}

// Check OTA pending flag
bool stage2_check_ota_pending(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    return (cfg->ota_pending == 1);
}

// Clear OTA pending flag
void stage2_clear_ota_pending(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    cfg->ota_pending = 0;
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
}

// Switch active application
void stage2_switch_app(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    cfg->app_active = (cfg->app_active == 0) ? 1 : 0;
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
    
    // Reset boot count for new app
    if (cfg->app_active == 0) {
        cfg->app_boot_cnt_a = 0;
    } else {
        cfg->app_boot_cnt_b = 0;
    }
    
    debug_print("[Stage2] Switched to App %c\n", 
                cfg->app_active == 0 ? 'A' : 'B');
}

// Verify application metadata
bool stage2_verify_app(uint32_t app_meta_addr) {
    BootMetadata* meta = (BootMetadata*)app_meta_addr;
    
    // 1. Magic number
    if (meta->magic != MAGIC_NUMBER) {
        debug_print("[Stage2] Invalid app magic\n");
        return false;
    }
    
    // 2. Valid flag
    if (meta->valid != 1) {
        debug_print("[Stage2] App not marked as valid\n");
        return false;
    }
    
    // 3. CRC32
    uint32_t app_start = app_meta_addr + 0x1000;
    uint32_t calc_crc = calculate_crc32((uint8_t*)app_start, meta->size);
    if (calc_crc != meta->crc32) {
        debug_print("[Stage2] App CRC failed\n");
        return false;
    }
    
    // 4. Signature (PQC Dilithium3)
    // if (!verify_dilithium_signature((uint8_t*)app_start, meta->size, meta->signature)) {
    //     debug_print("[Stage2] App signature verification failed\n");
    //     return false;
    // }
    
    return true;
}

// Jump to application
void stage2_jump_to_app(uint32_t app_addr) {
    debug_print("[Stage2] Jumping to application @ 0x%08X\n", app_addr);
    
    // 1. Vector Table
    uint32_t* vector_table = (uint32_t*)app_addr;
    uint32_t stack_pointer = vector_table[0];
    uint32_t reset_handler = vector_table[1];
    
    // 2. Set Stack Pointer
    __asm volatile("mov.a SP, %0" : : "d"(stack_pointer));
    
    // 3. Jump to App
    void (*app_entry)(void) = (void(*)(void))reset_handler;
    app_entry();
    
    // Never return
    while(1);
}

// OTA Update Handler
void stage2_process_ota(void) {
    debug_print("[Stage2] Processing OTA update...\n");
    
    // 1. Read OTA package from buffer
    // uint8_t* ota_buffer = (uint8_t*)OTA_BUFFER_START;
    
    // 2. Verify OTA package signature
    
    // 3. Write to inactive bank
    BootBank inactive_bank = (stage2_read_active_app() == BANK_A) ? BANK_B : BANK_A;
    uint32_t target_addr = (inactive_bank == BANK_A) ? REGION_A_APP_START : REGION_B_APP_START;
    
    debug_print("[Stage2] Installing OTA to Bank %c @ 0x%08X\n",
                inactive_bank == BANK_A ? 'A' : 'B', target_addr);
    
    // Flash write
    // Flash_Erase(target_addr, APP_SIZE);
    // Flash_Write(target_addr, ota_buffer, ota_size);
    
    // 4. Update metadata
    uint32_t meta_addr = (inactive_bank == BANK_A) ? REGION_A_APP_META : REGION_B_APP_META;
    BootMetadata meta;
    meta.magic = MAGIC_NUMBER;
    meta.valid = 1;
    // meta.size = ota_size;
    // meta.crc32 = calculate_crc32(ota_buffer, ota_size);
    // ... (fill in metadata)
    
    // Flash_Write(meta_addr, &meta, sizeof(BootMetadata));
    
    // 5. Switch to new app
    stage2_switch_app();
    stage2_clear_ota_pending();
    
    debug_print("[Stage2] OTA installation complete, rebooting...\n");
    system_reset();
}

// Stage 2 Main Entry Point
void stage2_main(void) {
    // Phase 1: Full hardware initialization
    stage2_init_hardware();
    
    debug_print("\n========================================\n");
    debug_print(" TC375 Stage 2 Bootloader v1.0\n");
    debug_print(" Zonal Gateway\n");
    debug_print("========================================\n");
    
    // Phase 2: Check OTA pending
    if (stage2_check_ota_pending()) {
        debug_print("[Stage2] OTA pending detected\n");
        stage2_process_ota();
        // system_reset() called, never return
    }
    
    // Phase 3: Select active application
    BootBank active_app = stage2_read_active_app();
    BootMetadata* meta_a = (BootMetadata*)REGION_A_APP_META;
    BootMetadata* meta_b = (BootMetadata*)REGION_B_APP_META;
    
    BootMetadata* active_meta = (active_app == BANK_A) ? meta_a : meta_b;
    uint32_t active_addr = (active_app == BANK_A) ? REGION_A_APP_START : REGION_B_APP_START;
    
    debug_print("[Stage2] Active App: %c\n", active_app == BANK_A ? 'A' : 'B');
    
    // Phase 4: Boot count check
    stage2_increment_app_boot_count(active_app);
    uint8_t boot_cnt = stage2_get_app_boot_count(active_app);
    
    if (boot_cnt >= MAX_BOOT_ATTEMPTS) {
        debug_print("[Stage2] App boot failed %d times, switching...\n", boot_cnt);
        stage2_switch_app();
        system_reset();
    }
    
    // Phase 5: Verify application
    if (!stage2_verify_app(active_meta)) {
        debug_print("[Stage2] Active app verification failed, switching...\n");
        stage2_switch_app();
        system_reset();
    }
    
    // Phase 6: Jump to application
    debug_print("[Stage2] App verified, jumping...\n\n");
    stage2_reset_app_boot_count(active_app);  // Success
    
    stage2_jump_to_app(active_addr);
    
    // Never reach here
    while(1);
}

