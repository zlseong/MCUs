/**
 * OTA Handler for TC375 Dual-Bank System
 * 
 * Application이 실행 중일 때, 비활성 Region을 업데이트
 * 
 * Role:
 *   - Application A 실행 중 → Region B 업데이트
 *   - Application B 실행 중 → Region A 업데이트
 */

#include "ota_handler.h"
#include "boot_common.h"
#include "flash_driver.h"
#include <string.h>

// OTA 상태
static OTA_State ota_state = OTA_IDLE;
static uint32_t ota_received_bytes = 0;
static uint8_t ota_buffer[OTA_BUFFER_SIZE];

/**
 * @brief 현재 실행 중인 Region 감지
 * @return BANK_A or BANK_B
 */
static BootBank detect_current_region(void) {
    uint32_t pc;
    __asm__ volatile ("mov.a %0, a11" : "=d" (pc));  // Get Program Counter
    
    if (pc >= REGION_A_START && pc <= REGION_A_END) {
        return BANK_A;
    } else if (pc >= REGION_B_START && pc <= REGION_B_END) {
        return BANK_B;
    }
    
    return BANK_INVALID;
}

/**
 * @brief 대상 (비활성) Region 주소 계산
 */
static void get_target_region_addresses(
    uint32_t* boot_meta_addr,
    uint32_t* boot_start_addr,
    uint32_t* app_meta_addr,
    uint32_t* app_start_addr
) {
    BootBank current = detect_current_region();
    
    if (current == BANK_A) {
        // Region A 실행 중 → Region B 업데이트
        *boot_meta_addr = REGION_B_BOOT_META;
        *boot_start_addr = REGION_B_BOOT_START;
        *app_meta_addr = REGION_B_APP_META;
        *app_start_addr = REGION_B_APP_START;
    } else {
        // Region B 실행 중 → Region A 업데이트
        *boot_meta_addr = REGION_A_BOOT_META;
        *boot_start_addr = REGION_A_BOOT_START;
        *app_meta_addr = REGION_A_APP_META;
        *app_start_addr = REGION_A_APP_START;
    }
}

/**
 * @brief OTA 초기화
 */
void ota_init(void) {
    ota_state = OTA_IDLE;
    ota_received_bytes = 0;
    memset(ota_buffer, 0, sizeof(ota_buffer));
    
    BootBank current = detect_current_region();
    debug_print("[OTA] Initialized. Running from Region %s\n", 
                current == BANK_A ? "A" : "B");
}

/**
 * @brief OTA 시작
 */
bool ota_start(uint32_t expected_size) {
    if (ota_state != OTA_IDLE) {
        debug_print("[OTA] Error: OTA already in progress\n");
        return false;
    }
    
    BootBank current = detect_current_region();
    debug_print("[OTA] Starting OTA update...\n");
    debug_print("[OTA]   Current Region: %s\n", current == BANK_A ? "A" : "B");
    debug_print("[OTA]   Target Region:  %s\n", current == BANK_A ? "B" : "A");
    debug_print("[OTA]   Expected Size:  %u bytes\n", expected_size);
    
    ota_state = OTA_DOWNLOADING;
    ota_received_bytes = 0;
    
    return true;
}

/**
 * @brief OTA 데이터 수신
 */
bool ota_receive_chunk(const uint8_t* data, uint32_t size) {
    if (ota_state != OTA_DOWNLOADING) {
        debug_print("[OTA] Error: Not in downloading state\n");
        return false;
    }
    
    if (ota_received_bytes + size > OTA_BUFFER_SIZE) {
        debug_print("[OTA] Error: OTA buffer overflow\n");
        ota_state = OTA_FAILED;
        return false;
    }
    
    // 버퍼에 데이터 누적
    memcpy(ota_buffer + ota_received_bytes, data, size);
    ota_received_bytes += size;
    
    debug_print("[OTA] Received chunk: %u bytes (total: %u)\n", size, ota_received_bytes);
    
    return true;
}

/**
 * @brief Bootloader 업데이트 (대상 Region)
 */
static bool ota_update_bootloader(uint32_t boot_meta_addr, uint32_t boot_start_addr,
                                   const uint8_t* bootloader_data, uint32_t bootloader_size) {
    debug_print("[OTA] Updating Bootloader @ 0x%08X...\n", boot_start_addr);
    
    // 1. Bootloader 영역 Erase
    if (!Flash_Erase(boot_start_addr, REGION_A_BOOT_SIZE)) {
        debug_print("[OTA] Error: Bootloader erase failed\n");
        return false;
    }
    
    // 2. Bootloader Program
    if (!Flash_Write(boot_start_addr, bootloader_data, bootloader_size)) {
        debug_print("[OTA] Error: Bootloader program failed\n");
        return false;
    }
    
    // 3. Bootloader Metadata 작성
    BootMetadata boot_meta;
    boot_meta.magic = MAGIC_NUMBER;
    boot_meta.version = 0x00010000;  // From OTA package
    boot_meta.size = bootloader_size;
    boot_meta.crc32 = calculate_crc32(bootloader_data, bootloader_size);
    boot_meta.build_timestamp = 0;  // From OTA package
    boot_meta.boot_count = 0;
    boot_meta.valid = 1;  // Valid
    
    // Signature (Dilithium3) - From OTA package
    // memcpy(boot_meta.signature, dilithium_sig, 256);
    
    if (!Flash_Write(boot_meta_addr, (uint8_t*)&boot_meta, sizeof(boot_meta))) {
        debug_print("[OTA] Error: Bootloader metadata write failed\n");
        return false;
    }
    
    debug_print("[OTA] Bootloader updated successfully\n");
    return true;
}

/**
 * @brief Application 업데이트 (대상 Region)
 */
static bool ota_update_application(uint32_t app_meta_addr, uint32_t app_start_addr,
                                    const uint8_t* app_data, uint32_t app_size) {
    debug_print("[OTA] Updating Application @ 0x%08X...\n", app_start_addr);
    
    // 1. Application 영역 Erase
    if (!Flash_Erase(app_start_addr, REGION_A_APP_SIZE)) {
        debug_print("[OTA] Error: Application erase failed\n");
        return false;
    }
    
    // 2. Application Program
    if (!Flash_Write(app_start_addr, app_data, app_size)) {
        debug_print("[OTA] Error: Application program failed\n");
        return false;
    }
    
    // 3. Application Metadata 작성
    BootMetadata app_meta;
    app_meta.magic = MAGIC_NUMBER;
    app_meta.version = 0x00020000;  // From OTA package
    app_meta.size = app_size;
    app_meta.crc32 = calculate_crc32(app_data, app_size);
    app_meta.build_timestamp = 0;  // From OTA package
    app_meta.boot_count = 0;
    app_meta.valid = 1;  // Valid
    
    // Signature (Dilithium3) - From OTA package
    // memcpy(app_meta.signature, dilithium_sig, 256);
    
    if (!Flash_Write(app_meta_addr, (uint8_t*)&app_meta, sizeof(app_meta))) {
        debug_print("[OTA] Error: Application metadata write failed\n");
        return false;
    }
    
    debug_print("[OTA] Application updated successfully\n");
    return true;
}

/**
 * @brief Boot Configuration 업데이트 (대상 Region 선택)
 */
static bool ota_switch_boot_config(void) {
    BootBank current = detect_current_region();
    BootBank target = (current == BANK_A) ? BANK_B : BANK_A;
    
    debug_print("[OTA] Switching boot config to Region %s\n", 
                target == BANK_A ? "A" : "B");
    
    BootConfig cfg;
    cfg.active_region = target;
    cfg.region_a_boot_cnt = 0;
    cfg.region_b_boot_cnt = 0;
    cfg.ota_pending = 0;  // OTA 완료
    memset(cfg.reserved, 0, sizeof(cfg.reserved));
    cfg.crc = calculate_crc32((uint8_t*)&cfg, sizeof(cfg) - 4);
    
    if (!Flash_Write(BOOT_CFG_EEPROM, (uint8_t*)&cfg, sizeof(cfg))) {
        debug_print("[OTA] Error: Boot config update failed\n");
        return false;
    }
    
    debug_print("[OTA] Boot config updated\n");
    return true;
}

/**
 * @brief OTA 설치 (Flash 작업 수행)
 * 
 * 이 함수가 핵심!
 * Application이 실행 중일 때, 비활성 Region을 업데이트합니다.
 */
bool ota_install(void) {
    if (ota_state != OTA_DOWNLOADING) {
        debug_print("[OTA] Error: Invalid state for installation\n");
        return false;
    }
    
    ota_state = OTA_INSTALLING;
    
    debug_print("[OTA] ========================================\n");
    debug_print("[OTA] Starting OTA Installation\n");
    debug_print("[OTA] ========================================\n");
    
    // 1. OTA 패키지 파싱 (간단한 예제)
    // 실제로는 더 복잡한 포맷 (헤더, 서명 등)
    uint32_t bootloader_offset = 0;
    uint32_t bootloader_size = 196 * 1024;  // 196 KB
    uint32_t app_offset = bootloader_size;
    uint32_t app_size = ota_received_bytes - bootloader_size;
    
    const uint8_t* bootloader_data = ota_buffer + bootloader_offset;
    const uint8_t* app_data = ota_buffer + app_offset;
    
    // 2. 대상 Region 주소 계산
    uint32_t boot_meta_addr, boot_start_addr, app_meta_addr, app_start_addr;
    get_target_region_addresses(&boot_meta_addr, &boot_start_addr, 
                                 &app_meta_addr, &app_start_addr);
    
    // 3. Bootloader 업데이트
    if (!ota_update_bootloader(boot_meta_addr, boot_start_addr, 
                                bootloader_data, bootloader_size)) {
        ota_state = OTA_FAILED;
        return false;
    }
    
    // 4. Application 업데이트
    if (!ota_update_application(app_meta_addr, app_start_addr, 
                                 app_data, app_size)) {
        ota_state = OTA_FAILED;
        return false;
    }
    
    // 5. Boot Config 업데이트 (대상 Region으로 전환)
    if (!ota_switch_boot_config()) {
        ota_state = OTA_FAILED;
        return false;
    }
    
    ota_state = OTA_COMPLETE;
    
    debug_print("[OTA] ========================================\n");
    debug_print("[OTA] OTA Installation Complete!\n");
    debug_print("[OTA] ========================================\n");
    debug_print("[OTA] Please reboot to activate new firmware\n");
    
    return true;
}

/**
 * @brief OTA 상태 조회
 */
OTA_State ota_get_state(void) {
    return ota_state;
}

/**
 * @brief OTA 진행률 조회
 */
uint32_t ota_get_progress(void) {
    return ota_received_bytes;
}

/**
 * @brief OTA 취소
 */
void ota_cancel(void) {
    debug_print("[OTA] OTA cancelled\n");
    ota_state = OTA_IDLE;
    ota_received_bytes = 0;
}

