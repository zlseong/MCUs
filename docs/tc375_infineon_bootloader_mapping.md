# TC375 Infineon   vs   

## [DOCS]  

- [Infineon TC375 Safe Application Development](https://www.infineon.com/assets/row/public/documents/10/42/infineon-safe-application-development-for-aurix-tc375-safety-litekit-applicationnotes-en.pdf)
- [AURIX TC3xx SOTA Documentation](https://documentation.infineon.com/aurixtc3xx/docs/sih1702364171562)

---

## [BUILD] Infineon TC375   

### Hardware Boot Sequence:

```
Power On / Reset
   |
+--------------------------------------+
| 1. UCB (User Configuration Blocks)  |
|    - Hardware                    |
|    - Boot Mode                   |
|    - 0xAF400000 (DFLASH)            |
+------------+-------------------------+
             |
+--------------------------------------+
| 2. BMI (Boot Mode Index) Header     |
|    - Boot mode                   |
|    - SSW                     |
|    - 0x80000000 (PFLASH)            |
+------------+-------------------------+
             |
+--------------------------------------+
| 3. SSW (Startup Software)            |
|    - CPU                       |
|    - Clock                       |
|    - Application              |
|    - 0x80000000 ~ 0x80020000        |
+------------+-------------------------+
             |
+--------------------------------------+
| 4. Application                       |
|    -                    |
|    - 0x80020000 ~                   |
+--------------------------------------+
```

### Infineon TC375  :

```
PFLASH (Program Flash) - 6 MB:
+-------------------------------------+
| 0x80000000: BMI Header (256 B)      |
+-------------------------------------+
| 0x80000100: SSW (Startup SW)        |
|             (~128 KB)               |
+-------------------------------------+
| 0x80020000: Application             |
|             (~5.8 MB)               |
+-------------------------------------+

DFLASH (Data Flash) - 384 KB:
+-------------------------------------+
| 0xAF000000: Data storage            |
+-------------------------------------+
| 0xAF400000: UCB (Config Blocks)     |
+-------------------------------------+
```

---

## [UPDATE]   Infineon   

###  :

|   | Infineon  |  |  |  |
|-----------|--------------|------|------|------|
| **Stage 1** | **SSW (Startup Software)** | 0x80000000 | 64 KB |  , Stage 2  |
| **Stage 2A** | **Application Bootloader A** | 0x80011000 | 188 KB |    |
| **Stage 2B** | **Application Bootloader B** | 0x80041000 | 188 KB |   |
| **App A** | **User Application A** | 0x80071000 | 2.4 MB |   |
| **App B** | **User Application B** | 0x802F2000 | 2.4 MB |   |

---

## [TABLE]   

### Infineon  :

```
TC375 PFLASH:
+------------------------------------+
| BMI Header @ 0x80000000            | <- Hardware reads
+------------------------------------+
| SSW (Startup Software)             | <- Infineon 
| @ 0x80000100                       |
| Size: ~128 KB                      |
+------------------------------------+
| Application                        |
| @ 0x80020000                       |
| Size: ~5.8 MB                      |
+------------------------------------+
```

###   (Dual Bank ):

```
TC375 PFLASH:
+------------------------------------+
| BMI Header @ 0x80000000            | <- Hardware reads
+------------------------------------+
| Stage 1 (SSW) @ 0x80000100         | <- Infineon SSW 
| Size: 64 KB                        |   (Boot selector)
+------------------------------------+
| Stage 2A (App Bootloader A)        | <- OTA  
| @ 0x80011000                       |
| Size: 188 KB                       |
+------------------------------------+
| Stage 2B (App Bootloader B)        | <- OTA  
| @ 0x80041000                       |
| Size: 188 KB                       |
+------------------------------------+
| App A (User Application A)         | <-   
| @ 0x80071000                       |
| Size: 2.4 MB                       |
+------------------------------------+
| App B (User Application B)         | <-  
| @ 0x802F2000                       |
| Size: 2.4 MB                       |
+------------------------------------+
```

---

## [TARGET]  

### Infineon   ->  :

| Infineon |   |  |
|----------|----------|------|
| **UCB (User Configuration Block)** | EEPROM Boot Config |    |
| **BMI (Boot Mode Index)** | Stage 1 Header |    |
| **SSW (Startup Software)** | **Stage 1 Bootloader** |    |
| **ABM (Alternate Boot Mode)** | Stage 2   |   |
| **Application** | Stage 2 + App |   |
| **SOTA (Software Over-The-Air)** | OTA Manager |   |
| **Bank Switching** | Dual Bank (A/B) |   |

---

## [NOTE]   (Infineon  )

###   :

```
tc375_bootloader/
+-- bmi/
|   +-- bmi_header.c          # BMI Header (256 bytes)
+-- ssw/
|   +-- ssw_main.c            # Startup Software (= Stage 1)
+-- bootloader/
|   +-- bootloader_main.c     # Application Bootloader (= Stage 2)
|   +-- bootloader_a_linker.ld
|   +-- bootloader_b_linker.ld
+-- common/
    +-- boot_common.h
```

###   :

```c
// boot_common.h - Infineon  

// BMI Header
#define BMI_HEADER_START    0x80000000
#define BMI_HEADER_SIZE     0x00000100  // 256 bytes

// SSW (Startup Software = Stage 1)
#define SSW_START           0x80000100
#define SSW_SIZE            0x00010000  // 64 KB

// Application Bootloader A (= Stage 2A)
#define BOOT_A_META         0x80010000
#define BOOT_A_START        0x80011000
#define BOOT_A_SIZE         0x0002F000  // 188 KB

// Application Bootloader B (= Stage 2B)
#define BOOT_B_META         0x80040000
#define BOOT_B_START        0x80041000
#define BOOT_B_SIZE         0x0002F000  // 188 KB

// User Application A
#define APP_A_META          0x80070000
#define APP_A_START         0x80071000
#define APP_A_SIZE          0x00280000  // 2.5 MB

// User Application B
#define APP_B_META          0x802F1000
#define APP_B_START         0x802F2000
#define APP_B_SIZE          0x00280000  // 2.5 MB

// UCB (User Configuration Blocks)
#define UCB_BMI             0xAF400000  // DFLASH
#define BOOT_CFG_EEPROM     0xAF000000  // Boot config storage
```

---

## [CONFIG]  

### BMI Header ( ):

```c
// bmi_header.c
#include <stdint.h>

typedef struct {
    uint32_t magic;           // 0xB359 (Infineon BMI magic)
    uint32_t boot_mode;       // Internal Flash Boot
    uint32_t ssw_address;     // SSW entry point
    uint32_t reserved[61];    // Padding to 256 bytes
} __attribute__((packed)) BmiHeader;

__attribute__((section(".bmi_header")))
const BmiHeader bmi = {
    .magic = 0xB359,
    .boot_mode = 0x00,        // Internal Flash
    .ssw_address = 0x80000100 // SSW start
};
```

### SSW (= Stage 1) :

```c
// ssw_main.c ( stage1_main.c)

/**
 * SSW - Startup Software
 * 
 * Infineon : SSW (Startup Software)
 *  : Stage 1 Bootloader
 * 
 * Role: Minimal, ROM-like, NEVER UPDATED
 * - Verify and select Application Bootloader (A or B)
 * - Jump to selected bootloader
 * 
 * Size: 64 KB
 * Location: 0x80000100 - 0x8000FFFF
 */

#include "../common/boot_common.h"

void ssw_init_hardware(void) {
    // Minimal CPU init
}

BootBank ssw_read_active_bootloader(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    return (cfg->bootloader_active == 0) ? BANK_A : BANK_B;
}

void ssw_jump_to_bootloader(uint32_t bootloader_addr) {
    uint32_t* vectors = (uint32_t*)bootloader_addr;
    uint32_t sp = vectors[0];
    uint32_t pc = vectors[1];
    
    __asm volatile("mov.a SP, %0" : : "d"(sp));
    
    void (*bootloader_entry)(void) = (void(*)(void))pc;
    bootloader_entry();
}

void __attribute__((section(".ssw_reset"))) ssw_main(void) {
    // Phase 1: Hardware init
    ssw_init_hardware();
    
    // Phase 2: Select bootloader
    BootBank active_boot = ssw_read_active_bootloader();
    uint32_t boot_addr = (active_boot == BANK_A) ? 
        BOOT_A_START : BOOT_B_START;
    
    // Phase 3: Verify bootloader
    if (!ssw_verify_bootloader(boot_addr)) {
        ssw_switch_to_fallback();
    }
    
    // Phase 4: Jump
    ssw_jump_to_bootloader(boot_addr);
}
```

### Application Bootloader (= Stage 2) :

```c
// bootloader_main.c ( stage2_main.c)

/**
 * Application Bootloader
 * 
 * Infineon : Application Bootloader
 *  : Stage 2 Bootloader
 * 
 * Role: Full-featured bootloader, CAN BE UPDATED via OTA
 * - Verify and select User Application (A or B)
 * - Full CRC and PQC signature verification
 * - Self-update capability
 * - Recovery mechanisms
 * 
 * Size: 188 KB (with libs)
 * Location A: 0x80011000 - 0x8003FFFF
 * Location B: 0x80041000 - 0x8006FFFF
 */

#include "../common/boot_common.h"

void bootloader_main(void) {
    // Phase 1: Hardware init
    bootloader_init_hardware();
    
    // Phase 2: Select application
    BootBank active_app = bootloader_read_active_app();
    
    // Phase 3: Verify application
    if (!bootloader_verify_application(active_app)) {
        bootloader_switch_to_fallback_app();
    }
    
    // Phase 4: Jump to application
    bootloader_jump_to_application(app_addr);
}
```

---

## [DOCS] Infineon SOTA (Software Over-The-Air) 

### Infineon SOTA :

```
AURIX TC3xx SOTA:
- Dual Bank Flash
- Bank A / Bank B
- Active Bank  
- Inactive Bank 
- Bank Switching 
```

###  :

```
 OTA Manager:
- Dual Bank  (A/B)
- Application Bootloader A/B
- User Application A/B
- Stage 2 (Bootloader) OTA 
- Application OTA 
```

** !** [OK]

---

## [TARGET]   

###  :

|  | Infineon  |   |  |  |
|------|--------------|----------|---------|------|
| **Level 0** | UCB | EEPROM Config | [X] |   |
| **Level 1** | BMI Header | BMI Header | [X] |   |
| **Level 2** | SSW | **Stage 1** | [X] |   |
| **Level 3** | App Bootloader | **Stage 2 A/B** | [OK] OTA |   |
| **Level 4** | Application | **App A/B** | [OK] OTA |   |

---

## [OK] 

###   Infineon   :

1. **SSW (Startup Software) = Stage 1**
   - Infineon SSW  
   -  ,  
   -    

2. **Application Bootloader = Stage 2**
   - Infineon SOTA  
   - Dual Bank (A/B) 
   - OTA  

3. **User Application = App A/B**
   - Infineon Dual Bank 
   - SOTA 
   - OTA  

4. **SOTA = OTA**
   - Infineon Software Over-The-Air
   -  OTA Manager
   -  

---

## [REFERENCE]  

- [AURIX TC3xx SOTA Documentation](https://documentation.infineon.com/aurixtc3xx/docs/sih1702364171562)
- Infineon Application Note: Safe Application Development for AURIX TC375
- AUTOSAR Bootloader Specification
- ISO 22842 (Vehicle ECU Software Update)

**  Infineon TC375     !** [SUCCESS]

---

## [UPDATE]   

###   :

|   | Infineon   |   |
|----------|------------------|----------|
| Stage 1 | SSW (Startup Software) | **SSW / Stage 1** |
| Stage 2 | Application Bootloader | **Bootloader / Stage 2** |
| App | User Application | **Application** |
| OTA | SOTA | **OTA / SOTA** |

**    !** [OK]

