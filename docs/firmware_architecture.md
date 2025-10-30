# TC375  : UDS, , A/B 

## [LIST] 

TC375  OTA    .

---

## 1⃣ **UDS (Unified Diagnostic Services)**

### ISO 14229  

#### **CAN/CAN-FD Transport Layer**

```c
// TC375  
#include "Ifx_Cfg.h"
#include "IfxCan.h"

#define UDS_REQUEST_ID   0x7E0   //  
#define UDS_RESPONSE_ID  0x7E8   //  

void sendUdsResponse(uint8_t service, uint8_t* data, size_t len) {
    IfxCan_Message msg;
    msg.id = UDS_RESPONSE_ID;
    msg.data[0] = service + 0x40;  // Positive response
    memcpy(&msg.data[1], data, len);
    msg.lengthCode = len + 1;
    
    IfxCan_Can_sendMessage(&canModule, &msg);
}
```

#### **  **

```c
typedef UdsResponse (*UdsServiceHandler)(UdsMessage*);

UdsServiceHandler uds_handlers[256] = {0};

void registerUdsService(uint8_t serviceId, UdsServiceHandler handler) {
    uds_handlers[serviceId] = handler;
}

void processUdsRequest(uint8_t* data, size_t len) {
    uint8_t serviceId = data[0];
    
    if (uds_handlers[serviceId]) {
        UdsResponse resp = uds_handlers[serviceId](&msg);
        sendUdsResponse(resp);
    } else {
        sendNegativeResponse(serviceId, NRC_SERVICE_NOT_SUPPORTED);
    }
}
```

---

## 2⃣ **  &  **

### ** **

```
                    +---------+
                    |  IDLE   |
                    +----+----+
                         |
                    startOTA()
                         |
                         v
                  +-------------+
                  | DOWNLOADING |
                  +------+------+
                         |
                   verify() --+
                         |    | FAIL
                         v    |
                  +-----------v--+
                  |  VERIFYING   |
                  +------+-------+
                         |
                  install() --+
                         |    | FAIL
                         v    |
                  +-----------v--+
                  | INSTALLING   |
                  +------+-------+
                         |
                         v
                    +---------+      +----------+
                    | SUCCESS | <----+ ROLLBACK |
                    +---------+      +----------+
                                           ^
                                           |
                                      boot fail
```

### ** **

```cpp
class OtaTransaction {
public:
    bool begin() {
        // 1.   
        saveCurrentState();
        
        // 2.   
        markTransactionStart();
        
        // 3.   
        prepareTargetBank();
        
        return true;
    }
    
    bool commit() {
        // 1.   
        if (!verificationComplete_) {
            return false;
        }
        
        // 2.  
        switchToNewBank();
        
        // 3.   
        markTransactionComplete();
        
        return true;
    }
    
    bool rollback() {
        // 1.    
        abortCurrentOperation();
        
        // 2.   
        restorePreviousState();
        
        // 3.   
        switchToPreviousBank();
        
        // 4.   
        logRollbackReason();
        
        return true;
    }

private:
    struct CheckPoint {
        BootBank original_bank;
        uint32_t timestamp;
        OtaState state;
        uint32_t bytes_written;
    };
    CheckPoint checkpoint_;
};
```

### **  &  **

```cpp
// Bootloader 
void bootloader_main(void) {
    BootBank active = getActiveBank();
    
    // 1.   
    incrementBootCount(active);
    
    // 2.    (brick )
    if (getBootCount(active) > MAX_BOOT_ATTEMPTS) {
        // 3    ->  
        printf("[Bootloader] Too many boot failures, rolling back...\n");
        
        BootBank fallback = (active == BANK_A) ? BANK_B : BANK_A;
        
        if (isValidFirmware(fallback)) {
            setActiveBank(fallback);
            resetBootCount(fallback);
            systemReset();
        } else {
            //    ->  
            enterRecoveryMode();
        }
    }
    
    // 3. CRC 
    if (!verifyCRC(active)) {
        printf("[Bootloader] CRC failed, trying fallback...\n");
        autoRollback();
        return;
    }
    
    // 4.  
    if (!verifySignature(active)) {
        printf("[Bootloader] Signature failed, trying fallback...\n");
        autoRollback();
        return;
    }
    
    // 5.  
    printf("[Bootloader] Booting from Bank %c\n", 
           active == BANK_A ? 'A' : 'B');
    resetBootCount(active);
    jumpToApplication(active);
}
```

---

## 3⃣ ** A/B  (Dual Bank)**

### **TC375 Flash  **

```
TC375 Flash: 6 MB (0x00000000 - 0x00600000)

+---------------------------------+ 0x00000000
|  Bootloader (256 KB)            | <-  ( )
+---------------------------------+ 0x00040000
|  Bank Metadata A (4 KB)         | <- Bank A 
+---------------------------------+ 0x00041000
|  Bank A - Firmware (2.5 MB)     | <- Bank A 
|                                 |
|  [Application Code]             |
|  [Vector Table]                 |
|  [Const Data]                   |
+---------------------------------+ 0x002C1000
|  Bank Metadata B (4 KB)         | <- Bank B 
+---------------------------------+ 0x002C2000
|  Bank B - Firmware (2.5 MB)     | <- Bank B 
|                                 |
|  [Application Code]             |
|  [Vector Table]                 |
|  [Const Data]                   |
+---------------------------------+ 0x00542000
|  Configuration Data (256 KB)    | <-  
+---------------------------------+ 0x00582000
|  Log Buffer (512 KB)            | <-  
+---------------------------------+ 0x00600000
```

### **Bank Metadata **

```c
typedef struct {
    uint32_t magic_number;        // 0xA5A5A5A5 (valid marker)
    uint32_t firmware_version;
    uint32_t firmware_size;
    uint32_t crc32;
    uint8_t  signature[256];      // PQC Dilithium signature
    uint32_t build_timestamp;
    uint32_t boot_count;          //   
    uint32_t last_boot_time;
    uint8_t  status;              // 0=Invalid, 1=Valid, 2=Testing
    uint8_t  reserved[243];
} __attribute__((packed)) BankMetadata;  // Total: 512 bytes
```

### **Flash  ( TC375)**

```c
#include "IfxFlash.h"

bool writeFlashSector(uint32_t address, const uint8_t* data, size_t length) {
    // 1. Flash   
    if (!IfxFlash_isWriteProtected(address)) {
        return false;
    }
    
    // 2.    (256 KB)
    uint32_t sector_addr = address & ~(0x40000 - 1);
    
    // 3.   (!)
    IfxFlash_eraseSector(sector_addr);
    
    // 4.    (512 bytes)
    for (size_t i = 0; i < length; i += 512) {
        IfxFlash_writePage(address + i, &data[i], 512);
        
        // 5.  
        if (memcmp((void*)(address + i), &data[i], 512) != 0) {
            return false;  // Write verify failed
        }
    }
    
    return true;
}
```

### **  ( TC375)**

```c
// bootloader.c - Runs first after reset

#define BANK_A_START  0x00041000
#define BANK_B_START  0x002C2000
#define METADATA_SIZE 0x1000

void bootloader_main(void) {
    // 1.   ()
    init_minimal_hardware();
    
    // 2. Bank  
    BankMetadata* meta_a = (BankMetadata*)0x00040000;
    BankMetadata* meta_b = (BankMetadata*)0x002C1000;
    
    BootBank active_bank = getStoredActiveBank();
    
    // 3.    
    incrementBootCountNV(active_bank);
    
    // 4. Watchdog : 3    
    if (getBootCountNV(active_bank) >= 3) {
        printf("[Bootloader] Boot failure detected, auto-rollback\n");
        
        BootBank fallback = (active_bank == BANK_A) ? BANK_B : BANK_A;
        
        if (isValidBank(fallback)) {
            setStoredActiveBank(fallback);
            resetBootCountNV(fallback);
            systemReset();  // Reboot to fallback
        } else {
            // Catastrophic failure!
            enterUsbDfuMode();  // USB recovery mode
        }
    }
    
    // 5.   
    BankMetadata* current_meta = (active_bank == BANK_A) ? meta_a : meta_b;
    
    if (current_meta->magic_number != 0xA5A5A5A5) {
        goto_fallback();
    }
    
    if (!verifyCRC32(active_bank, current_meta->crc32)) {
        printf("[Bootloader] CRC failed\n");
        goto_fallback();
    }
    
    if (!verifySignaturePQC(active_bank, current_meta->signature)) {
        printf("[Bootloader] Signature failed\n");
        goto_fallback();
    }
    
    // 6.  
    printf("[Bootloader] Booting Bank %c v%d\n", 
           active_bank == BANK_A ? 'A' : 'B',
           current_meta->firmware_version);
    
    resetBootCountNV(active_bank);  //   
    
    // 7.  
    uint32_t app_start = (active_bank == BANK_A) ? BANK_A_START : BANK_B_START;
    jumpToApplication(app_start);
}
```

### **Application    ( )**

```c
// application_main.c

void application_init(void) {
    BootBank my_bank = Bootloader::getActiveBank();
    
    // 1. Watchdog 
    startWatchdog(5000);  // 5 
    
    // 2.   (Self-Test)
    bool self_test_ok = true;
    
    // RAM 
    if (!testRAM()) {
        self_test_ok = false;
        logError("RAM test failed");
    }
    
    //   
    if (!testCAN() || !testEthernet() || !testADC()) {
        self_test_ok = false;
        logError("Hardware test failed");
    }
    
    // Gateway  
    if (!connectToGateway()) {
        self_test_ok = false;
        logError("Gateway connection failed");
    }
    
    // 3.     
    if (!self_test_ok) {
        printf("[App] Self-test failed, marking firmware invalid\n");
        Bootloader::markFirmwareInvalid(my_bank);
        systemReset();  // Bootloader  fallback 
    }
    
    // 4.    ->   
    Bootloader::markFirmwareValid(my_bank);
    resetBootCount(my_bank);
    
    // 5. Watchdog  
    kickWatchdog();
    
    // 6.   
    printf("[App] Firmware validated, entering normal operation\n");
}
```

---

## 3⃣ **A/B  **

### **Non-Volatile Storage ( )**

#### **Option 1: EEPROM **

```c
// TC375 EEPROM (64 KB)
#define BOOT_CONFIG_ADDR  0xAF000000

typedef struct {
    uint8_t active_bank;      // 0=Bank A, 1=Bank B
    uint8_t boot_count_a;
    uint8_t boot_count_b;
    uint32_t crc;             //  
} BootConfig;

BootBank readActiveBank(void) {
    BootConfig config;
    IfxFlash_readEeprom(BOOT_CONFIG_ADDR, &config, sizeof(config));
    
    // CRC 
    if (calculateCRC(&config, sizeof(config) - 4) != config.crc) {
        // Corruption! Default to Bank A
        return BANK_A;
    }
    
    return (config.active_bank == 0) ? BANK_A : BANK_B;
}

void setActiveBank(BootBank bank) {
    BootConfig config;
    config.active_bank = (bank == BANK_A) ? 0 : 1;
    config.crc = calculateCRC(&config, sizeof(config) - 4);
    
    IfxFlash_writeEeprom(BOOT_CONFIG_ADDR, &config, sizeof(config));
}
```

#### **Option 2: Flash Data Sector **

```c
// Dedicated Flash sector for boot config
#define BOOT_SECTOR_A   0x005F0000  // Last sector, Bank A config
#define BOOT_SECTOR_B   0x005F8000  // Last sector, Bank B config

void updateBankMetadata(BootBank bank, BankMetadata* meta) {
    uint32_t addr = (bank == BANK_A) ? BOOT_SECTOR_A : BOOT_SECTOR_B;
    
    // 1. Erase sector
    IfxFlash_eraseSector(addr);
    
    // 2. Write metadata
    meta->magic_number = 0xA5A5A5A5;
    IfxFlash_waitUnbusy();
    IfxFlash_enterPageMode(addr);
    IfxFlash_loadPage((uint32_t)meta, sizeof(BankMetadata));
    IfxFlash_writeBurst();
}
```

---

## [UPDATE] ** OTA  (  )**

### **1. Download Phase**

```c
OtaResult ota_download(FirmwareMetadata* meta) {
    OtaTransaction txn;
    
    // Transaction 
    if (!txn.begin()) {
        return OTA_ERROR_INIT;
    }
    
    BootBank target = getTargetBank();
    
    try {
        // Erase target bank
        if (!eraseBank(target)) {
            txn.rollback();
            return OTA_ERROR_ERASE;
        }
        
        // Download via UDS RequestDownload + TransferData
        uint32_t offset = 0;
        while (offset < meta->size) {
            // UDS TransferData receives blocks
            uint8_t block[4096];
            size_t received = receiveBlock(block, sizeof(block));
            
            if (!writeFlash(target, offset, block, received)) {
                txn.rollback();
                return OTA_ERROR_WRITE;
            }
            
            offset += received;
            updateProgress(offset * 100 / meta->size);
        }
        
    } catch (...) {
        txn.rollback();
        return OTA_ERROR_EXCEPTION;
    }
    
    return OTA_SUCCESS;
}
```

### **2. Verify Phase**

```c
OtaResult ota_verify(FirmwareMetadata* meta) {
    BootBank target = getTargetBank();
    
    // 1. CRC32 
    uint32_t calculated_crc = calculateCRC32(target);
    if (calculated_crc != meta->crc32) {
        printf("[OTA] CRC mismatch: calc=0x%08X, expect=0x%08X\n",
               calculated_crc, meta->crc32);
        
        // Rollback: erase invalid firmware
        eraseBank(target);
        return OTA_ERROR_CRC;
    }
    
    // 2. PQC   (Dilithium3)
    if (!verifyDilithiumSignature(target, meta->signature)) {
        printf("[OTA] Signature verification failed\n");
        eraseBank(target);
        return OTA_ERROR_SIGNATURE;
    }
    
    // 3. Metadata 
    saveBankMetadata(target, meta);
    
    return OTA_SUCCESS;
}
```

### **3. Install Phase (Atomic Switch)**

```c
OtaResult ota_install(void) {
    BootBank current = getCurrentBank();
    BootBank target = getTargetBank();
    
    // Critical Section Start
    disableInterrupts();
    
    // 1.   "Testing"  
    markBankTesting(target);
    
    // 2. Active   ( !)
    setActiveBank(target);
    
    // 3.    
    markBankBackup(current);
    
    enableInterrupts();
    // Critical Section End
    
    printf("[OTA] Install complete. Reboot to activate.\n");
    printf("[OTA] New firmware will boot from Bank %c\n",
           target == BANK_A ? 'A' : 'B');
    
    return OTA_SUCCESS;
}
```

### **4. Rollback ( )**

```c
OtaResult ota_rollback(void) {
    BootBank current = getCurrentBank();
    BootBank previous = (current == BANK_A) ? BANK_B : BANK_A;
    
    // 1.    
    BankMetadata prev_meta;
    if (!loadBankMetadata(previous, &prev_meta)) {
        return OTA_ERROR_NO_BACKUP;
    }
    
    if (!prev_meta.valid) {
        return OTA_ERROR_BACKUP_INVALID;
    }
    
    // 2.   Invalid 
    markBankInvalid(current);
    
    // 3.   
    setActiveBank(previous);
    resetBootCount(previous);
    
    printf("[OTA] Rollback to Bank %c v%d\n",
           previous == BANK_A ? 'A' : 'B',
           prev_meta.firmware.version);
    
    // 4.  
    systemReset();
    
    return OTA_SUCCESS;
}
```

---

##  **Fail-Safe **

### **1. Watchdog **

```c
void application_main_loop(void) {
    while (1) {
        //  
        processMessages();
        updateSensors();
        
        // Watchdog  (  )
        IfxScuWdt_clearCpuEndinit();
        IfxScuWdt_setCpuEndinit();
        
        //     -> Watchdog reset -> Bootloader 
    }
}
```

### **2.  Fallback**

```
1 : Bank A 
  | (CRC )
2 : Bank B 
  | (Signature )
3 : Recovery Mode (USB DFU)
```

### **3. Power Loss **

```c
// Flash     
bool safeFlashWrite(uint32_t addr, const uint8_t* data, size_t len) {
    // 1. Transaction marker 
    writeTransactionMarker(TRANSACTION_START);
    
    // 2.   
    bool result = IfxFlash_writePage(addr, data, len);
    
    // 3. Transaction  
    if (result) {
        writeTransactionMarker(TRANSACTION_COMPLETE);
    }
    
    //   :
    // - TRANSACTION_START  COMPLETE  ->   
    // -   
    
    return result;
}
```

---

## [NOTE] **  **

### ** 1:  OTA**

```
1. Gateway -> UDS: RequestDownload
2. TC375: Bank B erase -> OK
3. Gateway -> UDS: TransferData (blocks)
4. TC375: Write to Bank B -> OK
5. Gateway -> UDS: RequestTransferExit
6. TC375: Verify CRC -> OK
7. TC375: Verify Signature -> OK
8. TC375: Switch to Bank B
9. Reboot -> Boot from Bank B -> Success!
10. Bank B validated -> Bank A kept as backup
```

### ** 2:   **

```
1-4. ()
5. Transfer block #50: CRC error!
6. TC375: Rollback -> Erase Bank B
7. Status: Bank A still active (!)
```

### ** 3:    **

```
1-8. ()
9. Reboot -> Bank B  
10. Self-test ! (: Gateway  )
11. Bootloader: boot_count_b = 1
12. Reboot -> Bank B 
13.  ! boot_count_b = 2
14. Reboot -> Bank B 
15.  ! boot_count_b = 3 ≥ MAX
16. Bootloader: Auto-rollback to Bank A
17. Bank A  -> !
```

---

## [CONFIG] **TC375   **

### **1. Flash 4 Click ( 64 MB) **

```
TC375  Flash (6 MB):
+-- Bootloader (256 KB)
+-- Bank A (2.5 MB)       <-  
+-- Bank B (2.5 MB)       <- OTA 

Flash 4 Click (64 MB):
+-- Backup Bank A (2.5 MB)     <-  !
+-- Backup Bank B (2.5 MB)
+-- OTA Download Buffer (10 MB) <-  
+-- Logs ()
```

**:**
- 3-way backup (Internal A + B, External backup)
-   ->    Flash 

### **2.  **

```c
//    (256 KB)
#define SECTOR_SIZE  0x40000

for (uint32_t sector = 0; sector < BANK_SIZE; sector += SECTOR_SIZE) {
    eraseSector(bank_addr + sector);
    
    for (uint32_t page = 0; page < SECTOR_SIZE; page += 512) {
        writePage(bank_addr + sector + page, &data[offset], 512);
        offset += 512;
        
        kickWatchdog();  // Prevent timeout during long operations
    }
}
```

---

## [TABLE] ** **

### **Flash   (TC375)**

```
Sector Erase (256 KB):  ~500 ms
Page Write (512 B):     ~5 ms

Bank  (2.5 MB):
  Erase: 10 sectors × 500 ms = 5
  Write: 5120 pages × 5 ms = 25
  ---------------------------
  Total: ~30
```

### **Rollback **

```
  : <100 ms ( )
Reboot: ~2
Total: ~2

->   !
```

---

## [TARGET] ****

### ** 3:**

1. **UDS**:    OTA 
2. **Rollback**:  fallback brick 
3. **A/B **:  ,  

### **Safety Net :**

```
Level 1: CRC   ->  
Level 2: Signature  ->  
Level 3:  3  ->  rollback
Level 4:    -> USB Recovery
```

**:** Brick   0%! 

