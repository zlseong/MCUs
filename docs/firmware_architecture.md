# TC375 펌웨어 아키텍처: UDS, 롤백, A/B 파티션

## 📋 개요

TC375에서 안전한 OTA 업데이트를 위한 완전한 아키텍처입니다.

---

## 1️⃣ **UDS (Unified Diagnostic Services)**

### ISO 14229 표준 구현

#### **CAN/CAN-FD Transport Layer**

```c
// TC375 실제 구현
#include "Ifx_Cfg.h"
#include "IfxCan.h"

#define UDS_REQUEST_ID   0x7E0   // 진단 요청
#define UDS_RESPONSE_ID  0x7E8   // 진단 응답

void sendUdsResponse(uint8_t service, uint8_t* data, size_t len) {
    IfxCan_Message msg;
    msg.id = UDS_RESPONSE_ID;
    msg.data[0] = service + 0x40;  // Positive response
    memcpy(&msg.data[1], data, len);
    msg.lengthCode = len + 1;
    
    IfxCan_Can_sendMessage(&canModule, &msg);
}
```

#### **서비스 핸들러 등록**

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

## 2️⃣ **에러 핸들링 & 롤백 설계**

### **상태 머신**

```
                    ┌─────────┐
                    │  IDLE   │
                    └────┬────┘
                         │
                    startOTA()
                         │
                         ▼
                  ┌─────────────┐
                  │ DOWNLOADING │
                  └──────┬──────┘
                         │
                   verify() ──┐
                         │    │ FAIL
                         ▼    │
                  ┌───────────▼──┐
                  │  VERIFYING   │
                  └──────┬───────┘
                         │
                  install() ──┐
                         │    │ FAIL
                         ▼    │
                  ┌───────────▼──┐
                  │ INSTALLING   │
                  └──────┬───────┘
                         │
                         ▼
                    ┌─────────┐      ┌──────────┐
                    │ SUCCESS │ ◄────┤ ROLLBACK │
                    └─────────┘      └──────────┘
                                           ▲
                                           │
                                      boot fail
```

### **트랜잭션 관리**

```cpp
class OtaTransaction {
public:
    bool begin() {
        // 1. 현재 상태 백업
        saveCurrentState();
        
        // 2. 트랜잭션 시작 마킹
        markTransactionStart();
        
        // 3. 타겟 뱅크 준비
        prepareTargetBank();
        
        return true;
    }
    
    bool commit() {
        // 1. 검증 완료 확인
        if (!verificationComplete_) {
            return false;
        }
        
        // 2. 뱅크 전환
        switchToNewBank();
        
        // 3. 트랜잭션 완료 마킹
        markTransactionComplete();
        
        return true;
    }
    
    bool rollback() {
        // 1. 진행 중인 작업 중단
        abortCurrentOperation();
        
        // 2. 백업된 상태 복원
        restorePreviousState();
        
        // 3. 이전 뱅크로 복귀
        switchToPreviousBank();
        
        // 4. 실패 로그 기록
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

### **부트 검증 & 자동 롤백**

```cpp
// Bootloader에서 실행
void bootloader_main(void) {
    BootBank active = getActiveBank();
    
    // 1. 부트 카운터 증가
    incrementBootCount(active);
    
    // 2. 과도한 재부팅 감지 (brick 방지)
    if (getBootCount(active) > MAX_BOOT_ATTEMPTS) {
        // 3번 이상 부팅 실패 → 자동 롤백
        printf("[Bootloader] Too many boot failures, rolling back...\n");
        
        BootBank fallback = (active == BANK_A) ? BANK_B : BANK_A;
        
        if (isValidFirmware(fallback)) {
            setActiveBank(fallback);
            resetBootCount(fallback);
            systemReset();
        } else {
            // 양쪽 다 실패 → 복구 모드
            enterRecoveryMode();
        }
    }
    
    // 3. CRC 검증
    if (!verifyCRC(active)) {
        printf("[Bootloader] CRC failed, trying fallback...\n");
        autoRollback();
        return;
    }
    
    // 4. 서명 검증
    if (!verifySignature(active)) {
        printf("[Bootloader] Signature failed, trying fallback...\n");
        autoRollback();
        return;
    }
    
    // 5. 정상 부팅
    printf("[Bootloader] Booting from Bank %c\n", 
           active == BANK_A ? 'A' : 'B');
    resetBootCount(active);
    jumpToApplication(active);
}
```

---

## 3️⃣ **메모리 A/B 분할 (Dual Bank)**

### **TC375 Flash 메모리 맵**

```
TC375 Flash: 6 MB (0x00000000 - 0x00600000)

┌─────────────────────────────────┐ 0x00000000
│  Bootloader (256 KB)            │ ← 부트로더 (변경 불가)
├─────────────────────────────────┤ 0x00040000
│  Bank Metadata A (4 KB)         │ ← Bank A 메타데이터
├─────────────────────────────────┤ 0x00041000
│  Bank A - Firmware (2.5 MB)     │ ← Bank A 애플리케이션
│                                 │
│  [Application Code]             │
│  [Vector Table]                 │
│  [Const Data]                   │
├─────────────────────────────────┤ 0x002C1000
│  Bank Metadata B (4 KB)         │ ← Bank B 메타데이터
├─────────────────────────────────┤ 0x002C2000
│  Bank B - Firmware (2.5 MB)     │ ← Bank B 애플리케이션
│                                 │
│  [Application Code]             │
│  [Vector Table]                 │
│  [Const Data]                   │
├─────────────────────────────────┤ 0x00542000
│  Configuration Data (256 KB)    │ ← 영구 설정
├─────────────────────────────────┤ 0x00582000
│  Log Buffer (512 KB)            │ ← 로그 저장
└─────────────────────────────────┘ 0x00600000
```

### **Bank Metadata 구조**

```c
typedef struct {
    uint32_t magic_number;        // 0xA5A5A5A5 (valid marker)
    uint32_t firmware_version;
    uint32_t firmware_size;
    uint32_t crc32;
    uint8_t  signature[256];      // PQC Dilithium signature
    uint32_t build_timestamp;
    uint32_t boot_count;          // 부팅 시도 횟수
    uint32_t last_boot_time;
    uint8_t  status;              // 0=Invalid, 1=Valid, 2=Testing
    uint8_t  reserved[243];
} __attribute__((packed)) BankMetadata;  // Total: 512 bytes
```

### **Flash 프로그래밍 (실제 TC375)**

```c
#include "IfxFlash.h"

bool writeFlashSector(uint32_t address, const uint8_t* data, size_t length) {
    // 1. Flash 쓰기 가능 확인
    if (!IfxFlash_isWriteProtected(address)) {
        return false;
    }
    
    // 2. 섹터 단위로 정렬 (256 KB)
    uint32_t sector_addr = address & ~(0x40000 - 1);
    
    // 3. 섹터 지우기 (필수!)
    IfxFlash_eraseSector(sector_addr);
    
    // 4. 페이지 단위로 쓰기 (512 bytes)
    for (size_t i = 0; i < length; i += 512) {
        IfxFlash_writePage(address + i, &data[i], 512);
        
        // 5. 쓰기 검증
        if (memcmp((void*)(address + i), &data[i], 512) != 0) {
            return false;  // Write verify failed
        }
    }
    
    return true;
}
```

### **부트로더 로직 (실제 TC375)**

```c
// bootloader.c - Runs first after reset

#define BANK_A_START  0x00041000
#define BANK_B_START  0x002C2000
#define METADATA_SIZE 0x1000

void bootloader_main(void) {
    // 1. 하드웨어 초기화 (최소한만)
    init_minimal_hardware();
    
    // 2. Bank 메타데이터 읽기
    BankMetadata* meta_a = (BankMetadata*)0x00040000;
    BankMetadata* meta_b = (BankMetadata*)0x002C1000;
    
    BootBank active_bank = getStoredActiveBank();
    
    // 3. 부팅 시도 카운터 증가
    incrementBootCountNV(active_bank);
    
    // 4. Watchdog 방어: 3회 실패 시 자동 롤백
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
    
    // 5. 현재 뱅크 검증
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
    
    // 6. 정상 부팅
    printf("[Bootloader] Booting Bank %c v%d\n", 
           active_bank == BANK_A ? 'A' : 'B',
           current_meta->firmware_version);
    
    resetBootCountNV(active_bank);  // 성공 시 리셋
    
    // 7. 애플리케이션으로 점프
    uint32_t app_start = (active_bank == BANK_A) ? BANK_A_START : BANK_B_START;
    jumpToApplication(app_start);
}
```

### **Application 첫 실행 시 (자가 검증)**

```c
// application_main.c

void application_init(void) {
    BootBank my_bank = Bootloader::getActiveBank();
    
    // 1. Watchdog 시작
    startWatchdog(5000);  // 5초 타임아웃
    
    // 2. 자가 진단 (Self-Test)
    bool self_test_ok = true;
    
    // RAM 테스트
    if (!testRAM()) {
        self_test_ok = false;
        logError("RAM test failed");
    }
    
    // 주요 하드웨어 테스트
    if (!testCAN() || !testEthernet() || !testADC()) {
        self_test_ok = false;
        logError("Hardware test failed");
    }
    
    // Gateway 연결 테스트
    if (!connectToGateway()) {
        self_test_ok = false;
        logError("Gateway connection failed");
    }
    
    // 3. 자가 진단 실패 시 롤백
    if (!self_test_ok) {
        printf("[App] Self-test failed, marking firmware invalid\n");
        Bootloader::markFirmwareInvalid(my_bank);
        systemReset();  // Bootloader가 자동으로 fallback 선택
    }
    
    // 4. 자가 진단 성공 → 펌웨어 검증 완료
    Bootloader::markFirmwareValid(my_bank);
    resetBootCount(my_bank);
    
    // 5. Watchdog 정상 킥
    kickWatchdog();
    
    // 6. 정상 동작 시작
    printf("[App] Firmware validated, entering normal operation\n");
}
```

---

## 3️⃣ **A/B 파티션 관리**

### **Non-Volatile Storage (영구 저장)**

#### **Option 1: EEPROM 사용**

```c
// TC375 EEPROM (64 KB)
#define BOOT_CONFIG_ADDR  0xAF000000

typedef struct {
    uint8_t active_bank;      // 0=Bank A, 1=Bank B
    uint8_t boot_count_a;
    uint8_t boot_count_b;
    uint32_t crc;             // 데이터 무결성
} BootConfig;

BootBank readActiveBank(void) {
    BootConfig config;
    IfxFlash_readEeprom(BOOT_CONFIG_ADDR, &config, sizeof(config));
    
    // CRC 검증
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

#### **Option 2: Flash Data Sector 사용**

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

## 🔄 **완전한 OTA 흐름 (에러 처리 포함)**

### **1. Download Phase**

```c
OtaResult ota_download(FirmwareMetadata* meta) {
    OtaTransaction txn;
    
    // Transaction 시작
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
    
    // 1. CRC32 검증
    uint32_t calculated_crc = calculateCRC32(target);
    if (calculated_crc != meta->crc32) {
        printf("[OTA] CRC mismatch: calc=0x%08X, expect=0x%08X\n",
               calculated_crc, meta->crc32);
        
        // Rollback: erase invalid firmware
        eraseBank(target);
        return OTA_ERROR_CRC;
    }
    
    // 2. PQC 서명 검증 (Dilithium3)
    if (!verifyDilithiumSignature(target, meta->signature)) {
        printf("[OTA] Signature verification failed\n");
        eraseBank(target);
        return OTA_ERROR_SIGNATURE;
    }
    
    // 3. Metadata 저장
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
    
    // 1. 타겟 뱅크를 "Testing" 상태로 마킹
    markBankTesting(target);
    
    // 2. Active 뱅크 전환 (원자적 연산!)
    setActiveBank(target);
    
    // 3. 이전 뱅크는 백업으로 유지
    markBankBackup(current);
    
    enableInterrupts();
    // Critical Section End
    
    printf("[OTA] Install complete. Reboot to activate.\n");
    printf("[OTA] New firmware will boot from Bank %c\n",
           target == BANK_A ? 'A' : 'B');
    
    return OTA_SUCCESS;
}
```

### **4. Rollback (언제든 가능)**

```c
OtaResult ota_rollback(void) {
    BootBank current = getCurrentBank();
    BootBank previous = (current == BANK_A) ? BANK_B : BANK_A;
    
    // 1. 이전 뱅크 유효성 확인
    BankMetadata prev_meta;
    if (!loadBankMetadata(previous, &prev_meta)) {
        return OTA_ERROR_NO_BACKUP;
    }
    
    if (!prev_meta.valid) {
        return OTA_ERROR_BACKUP_INVALID;
    }
    
    // 2. 현재 뱅크를 Invalid로 마킹
    markBankInvalid(current);
    
    // 3. 이전 뱅크로 전환
    setActiveBank(previous);
    resetBootCount(previous);
    
    printf("[OTA] Rollback to Bank %c v%d\n",
           previous == BANK_A ? 'A' : 'B',
           prev_meta.firmware.version);
    
    // 4. 시스템 재부팅
    systemReset();
    
    return OTA_SUCCESS;
}
```

---

## 🛡️ **Fail-Safe 메커니즘**

### **1. Watchdog 보호**

```c
void application_main_loop(void) {
    while (1) {
        // 정상 동작
        processMessages();
        updateSensors();
        
        // Watchdog 킥 (정상 동작 증명)
        IfxScuWdt_clearCpuEndinit();
        IfxScuWdt_setCpuEndinit();
        
        // 만약 여기 도달 못하면 → Watchdog reset → Bootloader가 감지
    }
}
```

### **2. 다단계 Fallback**

```
1차 시도: Bank A 부팅
  ↓ (CRC 실패)
2차 시도: Bank B 부팅
  ↓ (Signature 실패)
3차 시도: Recovery Mode (USB DFU)
```

### **3. Power Loss 방어**

```c
// Flash 쓰기 중 전원 끊김 대비
bool safeFlashWrite(uint32_t addr, const uint8_t* data, size_t len) {
    // 1. Transaction marker 쓰기
    writeTransactionMarker(TRANSACTION_START);
    
    // 2. 실제 데이터 쓰기
    bool result = IfxFlash_writePage(addr, data, len);
    
    // 3. Transaction 완료 마킹
    if (result) {
        writeTransactionMarker(TRANSACTION_COMPLETE);
    }
    
    // 부팅 시 체크:
    // - TRANSACTION_START만 있고 COMPLETE 없으면 → 쓰기 중 끊김
    // - 해당 섹터 무효화
    
    return result;
}
```

---

## 📝 **실제 사용 시나리오**

### **시나리오 1: 정상 OTA**

```
1. Gateway → UDS: RequestDownload
2. TC375: Bank B erase → OK
3. Gateway → UDS: TransferData (blocks)
4. TC375: Write to Bank B → OK
5. Gateway → UDS: RequestTransferExit
6. TC375: Verify CRC → OK
7. TC375: Verify Signature → OK
8. TC375: Switch to Bank B
9. Reboot → Boot from Bank B → Success!
10. Bank B validated → Bank A kept as backup
```

### **시나리오 2: 다운로드 중 에러**

```
1-4. (동일)
5. Transfer block #50: CRC error!
6. TC375: Rollback → Erase Bank B
7. Status: Bank A still active (안전!)
```

### **시나리오 3: 새 펌웨어 부팅 실패**

```
1-8. (동일)
9. Reboot → Bank B 부팅 시도
10. Self-test 실패! (예: Gateway 연결 실패)
11. Bootloader: boot_count_b = 1
12. Reboot → Bank B 재시도
13. 또 실패! boot_count_b = 2
14. Reboot → Bank B 재시도
15. 또 실패! boot_count_b = 3 ≥ MAX
16. Bootloader: Auto-rollback to Bank A
17. Bank A 부팅 → 정상!
```

---

## 🔧 **TC375 실제 구현 팁**

### **1. Flash 4 Click (외부 64 MB) 활용**

```
TC375 내부 Flash (6 MB):
├── Bootloader (256 KB)
├── Bank A (2.5 MB)       ← 현재 실행
└── Bank B (2.5 MB)       ← OTA 타겟

Flash 4 Click (64 MB):
├── Backup Bank A (2.5 MB)     ← 추가 백업!
├── Backup Bank B (2.5 MB)
├── OTA Download Buffer (10 MB) ← 임시 버퍼
└── Logs (나머지)
```

**장점:**
- 3-way backup (Internal A + B, External backup)
- 다운로드 버퍼 → 검증 후 내부 Flash로 복사

### **2. 메모리 최적화**

```c
// 섹터 단위 프로그래밍 (256 KB)
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

## 📊 **성능 고려사항**

### **Flash 쓰기 시간 (TC375)**

```
Sector Erase (256 KB):  ~500 ms
Page Write (512 B):     ~5 ms

Bank 전체 (2.5 MB):
  Erase: 10 sectors × 500 ms = 5초
  Write: 5120 pages × 5 ms = 25초
  ───────────────────────────
  Total: ~30초
```

### **Rollback 시간**

```
단순 뱅크 전환: <100 ms (메타데이터만 변경)
Reboot: ~2초
Total: ~2초

→ 매우 빠른 복구!
```

---

## 🎯 **요약**

### **핵심 3요소:**

1. **UDS**: 표준 진단 프로토콜로 OTA 제어
2. **Rollback**: 다단계 fallback으로 brick 방지
3. **A/B 파티션**: 무중단 업데이트, 즉시 복구

### **Safety Net 순서:**

```
Level 1: CRC 검증 실패 → 다운로드 중단
Level 2: Signature 실패 → 설치 거부
Level 3: 부팅 3회 실패 → 자동 rollback
Level 4: 양쪽 뱅크 실패 → USB Recovery
```

**결과:** Brick 가능성 거의 0%! 🛡️

