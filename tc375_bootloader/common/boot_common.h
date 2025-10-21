#ifndef BOOT_COMMON_H
#define BOOT_COMMON_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Common Definitions for Both Stage 1 and Stage 2
// ============================================================================

// Boot Banks
typedef enum {
    BANK_A = 0,
    BANK_B = 1,
    BANK_INVALID = 0xFF
} BootBank;

// Boot Metadata (공통 구조)
typedef struct {
    uint32_t magic;           // 0xA5A5A5A5 = Valid
    uint32_t version;         // Firmware version
    uint32_t size;            // Firmware size in bytes
    uint32_t crc32;           // CRC32 checksum
    uint8_t  signature[256];  // PQC Dilithium3 signature
    uint32_t build_timestamp; // Build time
    uint32_t boot_count;      // Boot attempt counter
    uint8_t  valid;           // 0=Invalid, 1=Valid, 2=Testing
    uint8_t  reserved[243];   // Padding to 512 bytes
} __attribute__((packed)) BootMetadata;

// Memory Map Constants
#define STAGE1_START        0x80000000
#define STAGE1_SIZE         0x00010000  // 64 KB

#define STAGE2A_META        0x80010000
#define STAGE2A_START       0x80011000
#define STAGE2A_SIZE        0x0002F000  // 188 KB

#define STAGE2B_META        0x80040000
#define STAGE2B_START       0x80041000
#define STAGE2B_SIZE        0x0002F000  // 188 KB

#define APP_A_META          0x80070000
#define APP_A_START         0x80071000
#define APP_A_SIZE          0x00280000  // 2.5 MB

#define APP_B_META          0x802F1000
#define APP_B_START         0x802F2000
#define APP_B_SIZE          0x00280000  // 2.5 MB

// EEPROM Boot Configuration
#define BOOT_CFG_EEPROM     0xAF000000

typedef struct {
    uint8_t  stage2_active;   // 0=2A, 1=2B
    uint8_t  stage2_boot_cnt_a;
    uint8_t  stage2_boot_cnt_b;
    uint8_t  app_active;      // 0=App A, 1=App B
    uint8_t  app_boot_cnt_a;
    uint8_t  app_boot_cnt_b;
    uint8_t  reserved[10];
    uint32_t crc;             // Config integrity
} __attribute__((packed)) BootConfig;

// Configuration
#define MAX_BOOT_ATTEMPTS   3
#define MAGIC_NUMBER        0xA5A5A5A5

// Function Prototypes (common)
uint32_t calculate_crc32(const uint8_t* data, uint32_t length);
bool verify_dilithium_signature(const uint8_t* data, uint32_t length, const uint8_t* signature);
void debug_print(const char* msg);
void system_reset(void);

#endif // BOOT_COMMON_H

