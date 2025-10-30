# 2   OTA 

## [TARGET]   

```
+-----------------------------------------------------+
|  TC375 Flash (6 MB) - 3-Tier Boot System            |
|                                                     |
|  Tier 1: Stage 1 Bootloader (64 KB)    |
|          |  , Write-Protected              |
|                                                     |
|  Tier 2: Stage 2 Bootloader (A/B)                  |
|          +- Stage 2A (188 KB) -+                   |
|          +- Stage 2B (188 KB) -+- OTA !        |
|                      |         |                   |
|                                                     |
|  Tier 3: Application (A/B)                         |
|          +- App A (2.4 MB) ----+                   |
|          +- App B (2.4 MB) ----+- OTA !        |
+-----------------------------------------------------+
```

---

## [LIST] **4 OTA **

### **1⃣ Application  ( )**

```
Before: Stage 2A + App A (v1.0)
After:  Stage 2A + App B (v1.1)

Steps:
+---------------------------------------------+
| 1. Gateway -> App A: "New app available"    |
|                                             |
| 2. App A: Download to Bank B                |
|    +- UDS RequestDownload(0x802F2000)      |
|    +- UDS TransferData (blocks)            |
|    +- UDS RequestTransferExit              |
|                                             |
| 3. App A: Verify Bank B                    |
|    +- CRC32 check                          |
|    +- Dilithium3 signature verify          |
|    +- Save metadata                        |
|                                             |
| 4. App A: Switch config                    |
|    EEPROM.app_active = B                   |
|                                             |
| 5. App A: Reboot (UDS ECU Reset)           |
|                                             |
| 6. Stage 1 -> Stage 2A (unchanged)          |
|                                             |
| 7. Stage 2A -> App B (NEW!)                 |
|                                             |
| 8. App B: Self-test                        |
|    +- OK -> Mark valid                      |
|    +- FAIL -> Auto-rollback to App A        |
+---------------------------------------------+

Result: [OK] Application updated, Bootloader unchanged
```

---

### **2⃣ Stage 2 Bootloader **

```
Before: Stage 2A (v1.0) + App A
After:  Stage 2B (v1.1) + App A

Steps:
+---------------------------------------------+
| 1. Gateway -> App A: "New bootloader!"      |
|                                             |
| 2. App A: Download to Stage 2B              |
|    [WARNING]  : 0x80041000                |
|    +- UDS RequestDownload(0x80041000)      |
|    +- UDS TransferData (bootloader code)   |
|    +- UDS RequestTransferExit              |
|                                             |
| 3. App A: Verify Stage 2B                  |
|    +- CRC32                                 |
|    +- Signature                             |
|    +- Bootloader-specific checks           |
|                                             |
| 4. App A: Switch Stage 2                   |
|    EEPROM.stage2_active = B                |
|                                             |
| 5. App A: Reboot                           |
|                                             |
| 6. Stage 1                                 |
|    +- Stage 2B                         |
|    +-  -> Stage 2B (NEW!)               |
|                                             |
| 7. Stage 2B ( !)                 |
|    +- App A  ->                     |
|                                             |
| 8. App A:                          |
+---------------------------------------------+

Result: [OK] Bootloader updated, App unchanged
```

---

### **3⃣    (Full OTA)**

```
Before: Stage 2A + App A
After:  Stage 2B + App B

Steps:
+---------------------------------------------+
| 1. Stage 2B  ->                 |
| 2. App B  ->                    |
| 3. EEPROM: stage2_active=B, app_active=B   |
| 4. Reboot                                  |
| 5. Stage 1 -> Stage 2B -> App B              |
+---------------------------------------------+

Result: [OK] Complete system refresh!
```

---

### **4⃣  (  )**

```
Scenario: App B  

Boot 1: Stage 2A -> App B (FAIL, boot_cnt=1)
Boot 2: Stage 2A -> App B (FAIL, boot_cnt=2)  
Boot 3: Stage 2A -> App B (FAIL, boot_cnt=3)
        +- Stage 2A: boot_cnt >= 3 !
           +- Auto-rollback: app_active = A
              +- Reboot

Boot 4: Stage 2A -> App A (SUCCESS!)

+---------------------------------------------+
| Stage 2B   :                  |
|                                             |
| Stage 1: stage2_boot_cnt >= 3               |
|   +- Auto-rollback: stage2_active = A       |
|      +- Reboot -> Stage 2A                   |
+---------------------------------------------+
```

---

## [CODE] **Application  **

### Application OTA Manager :

```cpp
// ota_manager.cpp (Application)

enum class OtaTarget {
    APPLICATION,      // App A/B 
    STAGE2_BOOTLOADER // Stage 2  (!)
};

bool OtaManager::startDownload(OtaTarget target, uint32_t size, 
                                const FirmwareMetadata& meta) {
    target_type_ = target;
    
    if (target == OtaTarget::STAGE2_BOOTLOADER) {
        //   ( !)
        std::cout << "[OTA] [WARNING]  BOOTLOADER UPDATE MODE!" << std::endl;
        std::cout << "[OTA] Extra caution enabled" << std::endl;
        
        // Target: Stage 2 
        BootBank current_stage2 = getCurrentStage2Bank();
        BootBank target_bank = (current_stage2 == BANK_A) ? BANK_B : BANK_A;
        
        target_address_ = (target_bank == BANK_A) ? STAGE2A_START : STAGE2B_START;
        target_bank_ = target_bank;
        
        std::cout << "[OTA] Target: Stage 2" 
                  << (target_bank == BANK_A ? "A" : "B")
                  << " @ 0x" << std::hex << target_address_ << std::dec << std::endl;
        
    } else {
        //  Application 
        BootBank current_app = getCurrentAppBank();
        BootBank target_bank = (current_app == BANK_A) ? BANK_B : BANK_A;
        
        target_address_ = (target_bank == BANK_A) ? APP_A_START : APP_B_START;
        target_bank_ = target_bank;
    }
    
    return true;
}

bool OtaManager::verify() {
    if (target_type_ == OtaTarget::STAGE2_BOOTLOADER) {
        //   ( !)
        
        // 1. CRC
        if (!verifyCRC(target_address_, target_metadata_.crc32)) {
            return false;
        }
        
        // 2. Signature
        if (!verifySignature(target_address_, target_metadata_.signature)) {
            return false;
        }
        
        // 3.   
        if (!verifyBootloaderStructure(target_address_)) {
            std::cerr << "[OTA] Bootloader structure invalid!" << std::endl;
            return false;
        }
        
        // 4. Vector table 
        uint32_t* vectors = (uint32_t*)target_address_;
        if (vectors[0] < 0x70000000 || vectors[0] > 0x70100000) {
            std::cerr << "[OTA] Invalid stack pointer in bootloader!" << std::endl;
            return false;
        }
        
        std::cout << "[OTA] [OK] Bootloader verification PASSED" << std::endl;
        
    } else {
        //  Application 
        return verifyApplication();
    }
    
    return true;
}

bool OtaManager::install() {
    if (target_type_ == OtaTarget::STAGE2_BOOTLOADER) {
        // Stage 2 
        std::cout << "[OTA] Installing new Stage 2 bootloader..." << std::endl;
        
        // EEPROM 
        BootConfig cfg;
        cfg.stage2_active = (target_bank_ == BANK_A) ? 0 : 1;
        cfg.stage2_boot_cnt_a = 0;
        cfg.stage2_boot_cnt_b = 0;
        writeBootConfig(&cfg);
        
        std::cout << "[OTA] [WARNING]  BOOTLOADER WILL UPDATE ON NEXT REBOOT" << std::endl;
        std::cout << "[OTA] Please ensure stable power supply!" << std::endl;
        
    } else {
        // App 
        installApplication();
    }
    
    return true;
}
```

---

## [TEST] ** **

### Test 1: Stage 2  

```bash
# 1.   
vmg> tc375
{
  "stage2": "A",
  "app": "A",
  "versions": {
    "stage2_a": "1.0.0",
    "app_a": "2.0.0"
  }
}

# 2. Stage 2B 
vmg> ota stage2 stage2b_v1.1.hex
[OTA] Downloading to Stage 2B...
[OTA] Progress: 100%
[OTA] Verification: OK
[OTA] Installed, reboot required

# 3. Reboot
vmg> reboot

# 4. 
vmg> tc375
{
  "stage2": "B",  <- Changed!
  "app": "A",
  "versions": {
    "stage2_b": "1.1.0",  <- Updated!
    "app_a": "2.0.0"
  }
}
```

### Test 2:  

```bash
# 1.    
vmg> ota stage2 corrupt.hex --force

# 2. Reboot
Boot 1: Stage 1 -> Stage 2B (CRASH!)
Boot 2: Stage 1 -> Stage 2B (CRASH!)
Boot 3: Stage 1 -> Stage 2B (CRASH!)
        Stage 1: boot_cnt >= 3 
        Stage 1: Rollback to Stage 2A
Boot 4: Stage 1 -> Stage 2A -> App A (SUCCESS!)

# 3.  !
vmg> tc375
{
  "stage2": "A",  <- Rolled back!
  "status": "recovered_from_failure"
}
```

---

## [TOOL] **Application  **

### UDS  OTA :

```cpp
// Application UDS Handler

UdsResponse handleRequestDownload(const UdsMessage& request) {
    //  
    uint32_t address = parseAddress(request.data);
    uint32_t size = parseSize(request.data);
    
    //    
    OtaTarget target;
    
    if (address >= STAGE2A_START && address < STAGE2B_START + STAGE2A_SIZE) {
        // Stage 2 !
        target = OtaTarget::STAGE2_BOOTLOADER;
        
        std::cout << "[UDS] [WARNING]  BOOTLOADER DOWNLOAD REQUEST!" << std::endl;
        
        //   
        if (security_level_ != UNLOCKED) {
            return createNegativeResponse(NRC_SECURITY_ACCESS_DENIED);
        }
        
        //  
        std::cout << "[UDS] Bootloader update is CRITICAL operation" << std::endl;
        
    } else if (address >= APP_A_START && address < APP_B_START + APP_B_SIZE) {
        // Application 
        target = OtaTarget::APPLICATION;
        
    } else {
        return createNegativeResponse(NRC_REQUEST_OUT_OF_RANGE);
    }
    
    // OTA Manager 
    ota_manager_->startDownload(target, size, metadata);
    
    return createPositiveResponse({max_block_size});
}
```

---

## [SECURITY] ** **

### Stage 2    :

```cpp
bool OtaManager::verifyBootloaderStructure(uint32_t addr) {
    // 1. Vector Table 
    uint32_t* vectors = (uint32_t*)addr;
    
    // Stack Pointer ( 4 )
    if (vectors[0] < 0x70000000 || vectors[0] > 0x70100000) {
        return false;  // Invalid stack
    }
    
    // Reset Handler ( 4 )
    if (vectors[1] < addr || vectors[1] > addr + STAGE2A_SIZE) {
        return false;  // Reset handler out of range
    }
    
    // 2.  Signature  
    //     
    // - stage2_main
    // - stage2_verify_application
    //    
    
    // 3.   
    if (target_metadata_.size < 32768) {  // < 32 KB
        return false;  // Too small to be a valid bootloader
    }
    
    if (target_metadata_.size > STAGE2A_SIZE) {
        return false;  // Too large
    }
    
    return true;
}
```

---

## [TARGET] **Gateway **

### Gateway  OTA :

```cpp
// Gateway (C++)

void OtaController::updateStage2Bootloader(const std::string& device_id) {
    std::cout << "[Gateway] [WARNING]  BOOTLOADER UPDATE for " << device_id << std::endl;
    std::cout << "[Gateway] This is a CRITICAL operation!" << std::endl;
    
    // 1.   
    std::ifstream file("firmware/stage2_v1.1.bin", std::ios::binary);
    std::vector<uint8_t> bootloader_data(...);
    
    // 2.  
    FirmwareMetadata meta;
    meta.version = 0x00010001;  // v1.1
    meta.size = bootloader_data.size();
    meta.crc32 = calculateCRC32(bootloader_data);
    
    // 3. PQC  (Dilithium3)
    signWithDilithium3(bootloader_data, meta.signature);
    
    // 4. UDS 
    // Target address: Stage 2B (if current is 2A)
    uint32_t target_addr = getCurrentStage2() == "A" ? STAGE2B_START : STAGE2A_START;
    
    sendUdsRequestDownload(target_addr, meta.size);
    
    // 5.   
    for (size_t offset = 0; offset < bootloader_data.size(); offset += 4096) {
        sendUdsTransferData(block_counter++, &bootloader_data[offset], 4096);
        
        //   
        updateProgress(offset * 100 / bootloader_data.size());
    }
    
    // 6. 
    sendUdsRequestTransferExit();
    
    std::cout << "[Gateway] Bootloader uploaded successfully" << std::endl;
    std::cout << "[Gateway] Device will use new bootloader on next reboot" << std::endl;
}
```

---

##  **  & **

### : Stage 2A, 2B  

```
Boot Sequence:

1. Stage 1 -> Stage 2A (CRC FAIL!)
   +- Stage 1: Try Stage 2B

2. Stage 1 -> Stage 2B (CRC FAIL!)
   +- Stage 1: Both Stage 2 invalid!

3. Stage 1: Enter RECOVERY MODE
   +----------------------------+
   |  USB DFU Mode              |
   |  - USB              |
   |  - Stage 2      |
   +----------------------------+

4. User: USB Stage 2 
   +- Stage 1: Stage 2 

5. Reboot -> Stage 1 -> Stage 2 (!)
```

### Recovery Mode :

```c
// Stage 1 

void stage1_enter_recovery(void) {
    debug_print("============================================\n");
    debug_print("  RECOVERY MODE - Stage 2 Corrupted!      \n");
    debug_print("  Connect USB to upload new bootloader     \n");
    debug_print("============================================\n");
    
    // USB DFU (Device Firmware Update)
    initUSB();
    
    while(1) {
        if (usb_data_received()) {
            // USB   Stage 2B 
            write_to_stage2b(usb_buffer, usb_length);
            
            if (complete) {
                // 
                if (verify_stage2(BANK_B)) {
                    set_stage2_active(BANK_B);
                    debug_print("[Recovery] Stage 2B restored!\n");
                    system_reset();
                }
            }
        }
    }
}
```

---

## [TABLE] **  **

### Single Bootloader vs Dual Bootloader

```
Single Bootloader:
  Bootloader:   256 KB (1)
  Application: 5.7 MB (A/B)
  Total Boot:   256 KB
  -----------------------
  Available:   5.7 MB for App

Dual Bootloader:
  Stage 1:       64 KB (1)
  Stage 2:      376 KB (A/B  188KB)
  Application: 4.8 MB (A/B  2.4MB)
  Total Boot:   440 KB
  -----------------------
  Available:   4.8 MB for App
  
: 440 - 256 = 184 KB
: Application   900 KB 

->     !
```

---

## [INFO] ** **

### 2  :

1. [OK] **   **
   -     
   -     (PQC )
   
2. [OK] **3 Fail-safe**
   ```
   Stage 2  -> Fallback -> Recovery
   App  -> Fallback -> Rollback
   ```

3. [OK] **  **
   -  + Application   

4. [OK] **  **
   - USB DFU Stage 2 
   - Stage 1  

---

## [START] **  ()**

```
MCUs/
+-- tc375_bootloader/
|   +-- stage1/              # 64 KB,  
|   |   +-- stage1_main.c
|   |   +-- stage1_linker.ld
|   +-- stage2/              # 188 KB, OTA 
|   |   +-- stage2_main.c
|   |   +-- stage2_linker.ld
|   +-- common/
|   |   +-- boot_common.h
|   +-- README.md
|
+-- tc375_simulator/         # Application (Mac )
|   +-- src/
|   |   +-- uds_handler.cpp  <-  OTA !
|   |   +-- ota_manager.cpp  <- Stage 2 / App 
|   +-- ...
|
+-- docs/
    +-- dual_bootloader_ota.md
    +-- ...
```

---

## [TARGET] **   B :**

### **PQC  :**

```
2025: Dilithium3 
  |
2030:   PQC  
  |
Stage 2 OTA   !
  |
   ( )
```

** !** [SUCCESS]

   ?
