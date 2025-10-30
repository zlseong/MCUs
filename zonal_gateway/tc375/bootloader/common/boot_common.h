#ifndef BOOT_COMMON_H
#define BOOT_COMMON_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// TC375 Hardware Bank Switching Memory Map (Infineon Standard)
// ============================================================================

// TC375: 6 MB PFLASH = Region A (3 MB) + Region B (3 MB)
// Hardware automatically switches between Region A and B

// ============================================================================
// Region A (3 MB) @ 0x80000000 - Active/Cached
// ============================================================================

// BMI Header (Boot Mode Index) - Hardware fixed
#define REGION_A_BMI_START      0x80000000
#define REGION_A_BMI_SIZE       0x00000100  // 256 bytes

// SSW (Startup Software) - Immutable
#define REGION_A_SSW_START      0x80000100
#define REGION_A_SSW_SIZE       0x0000FF00  // ~64 KB

// Reserved Areas - DO NOT USE!
#define REGION_A_RESERVED_TP    0x80010000  // TP: 64 KB
#define REGION_A_RESERVED_HSM   0x80020000  // HSM: 512 KB

// Bootloader (Application Bootloader)
#define REGION_A_BOOT_META      0x800A0000  // Metadata (4 KB)
#define REGION_A_BOOT_START     0x800A1000  // Code start
#define REGION_A_BOOT_SIZE      0x00031000  // 196 KB
#define REGION_A_BOOT_END       0x800D1FFF

// Application (User Application)
#define REGION_A_APP_META       0x800D2000  // Metadata (4 KB)
#define REGION_A_APP_START      0x800D3000  // Code start
#define REGION_A_APP_SIZE       0x0112D000  // ~2.1 MB
#define REGION_A_APP_END        0x81FFFFFF

#define REGION_A_START          0x80000000
#define REGION_A_SIZE           0x02000000  // 3 MB
#define REGION_A_END            0x81FFFFFF

// ============================================================================
// Region B (3 MB) @ 0x82000000 - Inactive/Backup
// ============================================================================

// BMI Header
#define REGION_B_BMI_START      0x82000000
#define REGION_B_BMI_SIZE       0x00000100  // 256 bytes

// SSW (Startup Software)
#define REGION_B_SSW_START      0x82000100
#define REGION_B_SSW_SIZE       0x0000FF00  // ~64 KB

// Reserved Areas
#define REGION_B_RESERVED_TP    0x82010000  // TP: 64 KB
#define REGION_B_RESERVED_HSM   0x82020000  // HSM: 512 KB

// Bootloader (Application Bootloader)
#define REGION_B_BOOT_META      0x820A0000  // Metadata (4 KB)
#define REGION_B_BOOT_START     0x820A1000  // Code start
#define REGION_B_BOOT_SIZE      0x00031000  // 196 KB
#define REGION_B_BOOT_END       0x820D1FFF

// Application (User Application)
#define REGION_B_APP_META       0x820D2000  // Metadata (4 KB)
#define REGION_B_APP_START      0x820D3000  // Code start
#define REGION_B_APP_SIZE       0x0112D000  // ~2.1 MB
#define REGION_B_APP_END        0x83FFFFFF

#define REGION_B_START          0x82000000
#define REGION_B_SIZE           0x02000000  // 3 MB
#define REGION_B_END            0x83FFFFFF

// ============================================================================
// Compatibility Aliases (for backward compatibility)
// ============================================================================

// Legacy names (will be deprecated)
#define STAGE2A_START           REGION_A_BOOT_START
#define STAGE2B_START           REGION_B_BOOT_START
#define STAGE2A_META            REGION_A_BOOT_META
#define STAGE2B_META            REGION_B_BOOT_META
#define APP_A_START             REGION_A_APP_START
#define APP_B_START             REGION_B_APP_START
#define APP_A_META              REGION_A_APP_META
#define APP_B_META              REGION_B_APP_META

// ============================================================================
// PFLASH Total
// ============================================================================

#define PFLASH_START            0x80000000
#define PFLASH_SIZE             0x04000000  // 6 MB (Region A + B)
#define PFLASH_END              0x84000000

// ============================================================================
// DFLASH Configuration Storage
// ============================================================================

#define DFLASH_START            0xAF000000
#define DFLASH_SIZE             0x00060000  // 384 KB

#define BOOT_CFG_EEPROM         0xAF000000  // Boot config (4 KB)
#define APP_DATA_START          0xAF001000  // App data (60 KB)
#define OTA_BUFFER_START        0xAF010000  // OTA buffer (64 KB)
#define UCB_START               0xAF400000  // Infineon UCB (reserved)

// ============================================================================
// Boot Configuration (DFLASH)
// ============================================================================

typedef enum {
    BANK_A = 0,      // Region A
    BANK_B = 1,      // Region B
    BANK_INVALID = 0xFF
} BootBank;

typedef struct {
    uint8_t  stage2_active;      // 0=Stage2A, 1=Stage2B
    uint8_t  stage2_boot_cnt_a;  // Stage2A boot attempts
    uint8_t  stage2_boot_cnt_b;  // Stage2B boot attempts
    uint8_t  app_active;         // 0=AppA, 1=AppB
    uint8_t  app_boot_cnt_a;     // AppA boot attempts
    uint8_t  app_boot_cnt_b;     // AppB boot attempts
    uint8_t  ota_pending;        // OTA update pending flag
    uint8_t  reserved[9];
    uint32_t crc;                // Config CRC32
} __attribute__((packed)) BootConfig;

// ============================================================================
// Metadata Structure (512 bytes)
// ============================================================================

typedef struct {
    uint32_t magic;           // 0xA5A5A5A5
    uint32_t version;         // Firmware version
    uint32_t size;            // Firmware size
    uint32_t crc32;           // CRC32
    uint8_t  signature[256];  // PQC Dilithium3
    uint32_t build_timestamp;
    uint32_t boot_count;
    uint8_t  valid;           // 0=Invalid, 1=Valid, 2=Testing
    uint8_t  reserved[243];
} __attribute__((packed)) BootMetadata;

// Configuration
#define MAX_BOOT_ATTEMPTS   3
#define MAGIC_NUMBER        0xA5A5A5A5

// Function Prototypes
uint32_t calculate_crc32(const uint8_t* data, uint32_t length);
bool verify_dilithium_signature(const uint8_t* data, uint32_t length, const uint8_t* sig);
void debug_print(const char* msg, ...);
void system_reset(void);

#endif // BOOT_COMMON_H

