# TC375 Bootloader  

## [TARGET]  

```
Power On -> Reset Vector (0x00000000)
    |
Bootloader (0x00000000 - 0x0003FFFF)  <- 256 KB
    |
1.    (Clock, Watchdog)
2. Bank Metadata 
3. Bank  (A or B)
4. CRC 
5. Signature 
6. Boot Count 
7. Application  ------------> Bank A/B
                                      |
                              Application 
                              (UDS, OTA, TLS )
```

---

##  **  **

### ** :**

```c
tc375_bootloader/
+-- boot_main.c           //  
+-- flash_driver.c        // Flash /
+-- crypto_verify.c       // CRC/ 
+-- bank_manager.c        // A/B  
+-- boot_config.h         // 
+-- tc375_boot.ld         //    !
```

---

## [CONFIG] **1.   (  )**

### `tc375_boot.ld` -  

```ld
/* TC375 Bootloader Linker Script */

MEMORY
{
    /* Bootloader Flash  256KB */
    BOOT_FLASH (rx) : ORIGIN = 0x80000000, LENGTH = 256K
    
    /* Bootloader RAM */
    BOOT_RAM   (rw) : ORIGIN = 0x70000000, LENGTH = 64K
}

SECTIONS
{
    /* Reset Vector -     */
    .boot_start : {
        KEEP(*(.boot_reset))
        . = ALIGN(4);
    } > BOOT_FLASH
    
    /* Bootloader  */
    .text : {
        *(.text*)
    } > BOOT_FLASH
    
    /*   */
    .rodata : {
        *(.rodata*)
    } > BOOT_FLASH
    
    /*  */
    .data : {
        *(.data*)
    } > BOOT_RAM AT> BOOT_FLASH
    
    .bss : {
        *(.bss*)
    } > BOOT_RAM
}

/* Application Entry Points (  ) */
BANK_A_START = 0x80040000;  /* 256 KB  */
BANK_B_START = 0x802C2000;  /* 2.5 MB  */
```

### `tc375_app.ld` -  

```ld
/* TC375 Application Linker Script */

MEMORY
{
    /* Bank A: 256KB ~ 2.75MB */
    APP_FLASH_A (rx) : ORIGIN = 0x80040000, LENGTH = 2560K
    
    /* Bank B: 2.75MB ~ 5.25MB */
    APP_FLASH_B (rx) : ORIGIN = 0x802C2000, LENGTH = 2560K
    
    /* Application RAM ( ) */
    APP_RAM (rw) : ORIGIN = 0x70010000, LENGTH = 4M
}

SECTIONS
{
    /* Application Vector Table */
    .app_vectors : {
        KEEP(*(.app_vectors))
    } > APP_FLASH_A  /* or APP_FLASH_B */
    
    .text : { *(.text*) } > APP_FLASH_A
    .data : { *(.data*) } > APP_RAM
}
```

---

## [CODE] **2.   **

### `boot_main.c` -  

```c
#include <stdint.h>
#include "Ifx_Cfg.h"
#include "IfxCpu.h"
#include "IfxFlash.h"

// Bank  
#define META_A_ADDR  0x80040000
#define META_B_ADDR  0x802C1000

// Bank  
#define BANK_A_START 0x80041000
#define BANK_B_START 0x802C2000

// EEPROM   
#define BOOT_CFG_ADDR 0xAF000000

typedef enum {
    BANK_A = 0,
    BANK_B = 1
} BootBank;

typedef struct {
    uint32_t magic;           // 0xA5A5A5A5
    uint32_t version;
    uint32_t size;
    uint32_t crc32;
    uint8_t  signature[256]; // PQC signature
    uint32_t boot_count;
    uint8_t  valid;          // 0=Invalid, 1=Valid
} __attribute__((packed)) BankMetadata;

// ============================================================================
// Bootloader Main Entry
// ============================================================================

void __attribute__((section(".boot_reset"))) _reset(void) {
    // 1. CPU  ()
    IfxCpu_setCoreMode(&MODULE_CPU0, IfxCpu_CoreMode_run);
    
    // 2. Watchdog  ( )
    IfxScuWdt_clearCpuEndinit();
    
    // 3. Clock  ()
    initSystemClock();
    
    // 4.  
    bootloader_main();
}

void bootloader_main(void) {
    // ========================================
    // Phase 1: Bank  
    // ========================================
    
    BankMetadata* meta_a = (BankMetadata*)META_A_ADDR;
    BankMetadata* meta_b = (BankMetadata*)META_B_ADDR;
    
    // EEPROM  active bank 
    BootBank active_bank = readActiveBank();
    
    BankMetadata* active_meta = (active_bank == BANK_A) ? meta_a : meta_b;
    BankMetadata* backup_meta = (active_bank == BANK_A) ? meta_b : meta_a;
    
    // ========================================
    // Phase 2: Boot Count  (Fail-safe)
    // ========================================
    
    active_meta->boot_count++;
    
    if (active_meta->boot_count >= 3) {
        // 3   ->  !
        debug_print("[Boot] Too many failures, rollback!\n");
        
        if (backup_meta->valid == 1 && backup_meta->magic == 0xA5A5A5A5) {
            // Fallback bank 
            active_bank = (active_bank == BANK_A) ? BANK_B : BANK_A;
            writeActiveBank(active_bank);
            
            // Fallback metadata 
            backup_meta->boot_count = 0;
            
            // 
            systemReset();
        } else {
            // Catastrophic failure!
            enterRecoveryMode();  // USB DFU
            while(1);  // Never return
        }
    }
    
    // ========================================
    // Phase 3: Firmware 
    // ========================================
    
    // Magic number 
    if (active_meta->magic != 0xA5A5A5A5) {
        debug_print("[Boot] Invalid magic\n");
        tryFallback();
    }
    
    // CRC32 
    uint32_t calc_crc = calculateCRC32(
        active_bank == BANK_A ? BANK_A_START : BANK_B_START,
        active_meta->size
    );
    
    if (calc_crc != active_meta->crc32) {
        debug_print("[Boot] CRC failed: calc=%08X, expect=%08X\n", 
                   calc_crc, active_meta->crc32);
        tryFallback();
    }
    
    // PQC   (Dilithium3)
    if (!verifyDilithium3Signature(
            active_bank == BANK_A ? BANK_A_START : BANK_B_START,
            active_meta->size,
            active_meta->signature)) {
        debug_print("[Boot] Signature verification failed\n");
        tryFallback();
    }
    
    // ========================================
    // Phase 4: Application 
    // ========================================
    
    debug_print("[Boot] Booting Bank %c v%d\n",
               active_bank == BANK_A ? 'A' : 'B',
               active_meta->version);
    
    // Boot count  ( )
    active_meta->boot_count = 0;
    
    // Application  
    uint32_t app_start = (active_bank == BANK_A) ? BANK_A_START : BANK_B_START;
    
    // Application !
    jumpToApplication(app_start);
    
    //    
    while(1);
}

// ============================================================================
// Helper Functions
// ============================================================================

void jumpToApplication(uint32_t app_addr) {
    // 1. Application Vector Table 
    uint32_t* vector_table = (uint32_t*)app_addr;
    uint32_t stack_pointer = vector_table[0];  // SP
    uint32_t reset_handler = vector_table[1];  // PC
    
    // 2. Watchdog 
    IfxScuWdt_setCpuEndinit();
    
    // 3.   
    __asm volatile("mov.a SP, %0" : : "d"(stack_pointer));
    
    // 4. Application Reset Handler 
    void (*app_entry)(void) = (void(*)(void))reset_handler;
    app_entry();
    
    // Never return
}

void tryFallback(void) {
    BootBank current = readActiveBank();
    BootBank fallback = (current == BANK_A) ? BANK_B : BANK_A;
    
    BankMetadata* fallback_meta = (fallback == BANK_A) ? 
        (BankMetadata*)META_A_ADDR : (BankMetadata*)META_B_ADDR;
    
    // Fallback  
    if (fallback_meta->valid == 1 && fallback_meta->magic == 0xA5A5A5A5) {
        debug_print("[Boot] Switching to Bank %c\n", 
                   fallback == BANK_A ? 'A' : 'B');
        
        writeActiveBank(fallback);
        systemReset();  // 
    } else {
        //   !
        enterRecoveryMode();
    }
}

void enterRecoveryMode(void) {
    debug_print("[Boot] RECOVERY MODE - Connect USB for firmware upload\n");
    
    // USB DFU (Device Firmware Update)  
    //  USB    
    
    while(1) {
        handleUsbDfu();  // USB  
    }
}
```

---

##  **   **

### **  :**

```
+---------------------------------------------+
|  1. Bootloader (  )               |
|     - TC375                        |
|     -                            |
|     - Application  &                 |
+-----------------+---------------------------+
                  | 
                  v
+---------------------------------------------+
|  2. Application (tc375_simulator )      |
|                                             |
|     +-----------------+                    |
|     |  UDS Handler    | <- OTA      |
|     +--------+--------+                    |
|              v                              |
|     +-----------------+                    |
|     |  OTA Manager    | <- Bank B     |
|     +--------+--------+                    |
|              v                              |
|     +-----------------+                    |
|     |  TLS Client     | <- Gateway      |
|     +--------+--------+                    |
+--------------+-----------------------------+
               | TLS
               v
+---------------------------------------------+
|  3. Gateway (vehicle_gateway) [OK]            |
|     - OTA                           |
|     - UDS                           |
+---------------------------------------------+
```

### **OTA  :**

```
[Gateway] OTA  
    |
[Application] UDS RequestDownload 
    |
[Application] Bank B  erase
    |
[Gateway]    (UDS TransferData)
    |
[Application] Bank B Flash 
    |
[Application] CRC/Signature 
    |
[Application] Active Bank B 
    |
[Application]   (UDS ECU Reset)
    |
[Bootloader]  !
    |
[Bootloader] Bank B  -> OK
    |
[Bootloader] Bank B 
    |
[New Application] !
```

---

##  **  **

### **AURIX Development Studio (ADS):**

```bash
# 1.   
File -> New -> AURIX Project
  - Name: TC375_Bootloader
  - Target: TC375TP
  - Template: Empty Project

# 2.   
boot_main.c
flash_driver.c
crypto_verify.c

# 3.   
Project -> Properties -> C/C++ Build -> Settings
  -> TriCore C Linker -> General
    -> Linker Script: tc375_boot.ld

# 4. 
Build Project
  -> : bootloader.elf, bootloader.hex

# 5. Flash 
Run -> Debug Configurations
  -> Flash bootloader.hex to 0x80000000
```

---

## [TABLE] **  ()**

```
TC375 Flash (6 MB):

0x80000000  +-------------------------+
            |  Bootloader             |  256 KB
            |  - boot_main.c          |  <-  
            |  - flash_driver.c       |
0x80040000  +-------------------------+
            |  Bank A Metadata        |  4 KB
0x80041000  +-------------------------+
            |  Bank A Application     |  2.5 MB
            |  - main.cpp             |  <- tc375_simulator 
            |  - uds_handler.cpp      |  [OK]  
            |  - ota_manager.cpp      |  [OK]  
            |  - tls_client.cpp       |  [OK]  
0x802C1000  +-------------------------+
            |  Bank B Metadata        |  4 KB
0x802C2000  +-------------------------+
            |  Bank B Application     |  2.5 MB
            |  (Same code, new version)|
0x80542000  +-------------------------+
            |  Config / Logs          |  768 KB
0x80600000  +-------------------------+
```

---

## [UPDATE] **  **

### **1. Bootloader **
```c
//  
// Application 
// : bootloader.hex (256 KB)
```

### **2. Application **
```cpp
// tc375_simulator  
// Bootloader  
// : application_a.hex (2.5 MB)
```

### **3.  &  **
```bash
# Step 1:   &  ( !)
cd tc375_bootloader
./build_boot.sh
flash_tool --addr 0x80000000 bootloader.hex

# Step 2: Application  & 
cd tc375_application
./build.sh
flash_tool --addr 0x80041000 application.hex  # Bank A

#  OTA Gateway !
```

---

## [INFO] **Mac  **

### ** tc375_simulator:**

```cpp
[OK] Application  
  - UDS Handler
  - OTA Manager
  - TLS Client
  - Protocol

[X] Bootloader  
  (MCU  )
```

### ** :**

```
Mac:
  tc375_simulator 
  -> Gateway  
  -> OTA  
  -> UDS  

TC375:
  Bootloader + Application 
  ->  A/B   
  ->   
```

---

## [TARGET] ** **

### **    :**

1. [OK] **tc375_simulator** - Gateway    (Mac)
2. [PENDING] **tc375_bootloader** -    (ADS )
3. [PENDING] **tc375_application** - simulator  TC375 

### **?**

    ? 
(  Windows + ADS  ,    !)
