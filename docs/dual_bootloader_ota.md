# 2단계 부트로더 완전한 OTA 가이드

## 🎯 전체 시스템 구성

```
┌─────────────────────────────────────────────────────┐
│  TC375 Flash (6 MB) - 3-Tier Boot System            │
│                                                     │
│  Tier 1: Stage 1 Bootloader (64 KB) ════════════   │
│          ↓ 절대 불변, Write-Protected              │
│                                                     │
│  Tier 2: Stage 2 Bootloader (A/B)                  │
│          ├─ Stage 2A (188 KB) ─┐                   │
│          └─ Stage 2B (188 KB) ─┼─ OTA 가능!        │
│                      ↓         │                   │
│                                                     │
│  Tier 3: Application (A/B)                         │
│          ├─ App A (2.4 MB) ────┐                   │
│          └─ App B (2.4 MB) ────┼─ OTA 가능!        │
└─────────────────────────────────────────────────────┘
```

---

## 📋 **4가지 OTA 시나리오**

### **1️⃣ Application만 업데이트 (가장 흔함)**

```
Before: Stage 2A + App A (v1.0)
After:  Stage 2A + App B (v1.1)

Steps:
┌─────────────────────────────────────────────┐
│ 1. Gateway → App A: "New app available"    │
│                                             │
│ 2. App A: Download to Bank B                │
│    ├─ UDS RequestDownload(0x802F2000)      │
│    ├─ UDS TransferData (blocks)            │
│    └─ UDS RequestTransferExit              │
│                                             │
│ 3. App A: Verify Bank B                    │
│    ├─ CRC32 check                          │
│    ├─ Dilithium3 signature verify          │
│    └─ Save metadata                        │
│                                             │
│ 4. App A: Switch config                    │
│    EEPROM.app_active = B                   │
│                                             │
│ 5. App A: Reboot (UDS ECU Reset)           │
│                                             │
│ 6. Stage 1 → Stage 2A (unchanged)          │
│                                             │
│ 7. Stage 2A → App B (NEW!)                 │
│                                             │
│ 8. App B: Self-test                        │
│    ├─ OK → Mark valid                      │
│    └─ FAIL → Auto-rollback to App A        │
└─────────────────────────────────────────────┘

Result: ✅ Application updated, Bootloader unchanged
```

---

### **2️⃣ Stage 2 Bootloader 업데이트**

```
Before: Stage 2A (v1.0) + App A
After:  Stage 2B (v1.1) + App A

Steps:
┌─────────────────────────────────────────────┐
│ 1. Gateway → App A: "New bootloader!"      │
│                                             │
│ 2. App A: Download to Stage 2B              │
│    ⚠️ 특수 주소: 0x80041000                │
│    ├─ UDS RequestDownload(0x80041000)      │
│    ├─ UDS TransferData (bootloader code)   │
│    └─ UDS RequestTransferExit              │
│                                             │
│ 3. App A: Verify Stage 2B                  │
│    ├─ CRC32                                 │
│    ├─ Signature                             │
│    └─ Bootloader-specific checks           │
│                                             │
│ 4. App A: Switch Stage 2                   │
│    EEPROM.stage2_active = B                │
│                                             │
│ 5. App A: Reboot                           │
│                                             │
│ 6. Stage 1                                 │
│    ├─ Stage 2B 검증                        │
│    └─ 점프 → Stage 2B (NEW!)               │
│                                             │
│ 7. Stage 2B (새 부트로더!)                 │
│    └─ App A 검증 → 점프                    │
│                                             │
│ 8. App A: 정상 실행                        │
└─────────────────────────────────────────────┘

Result: ✅ Bootloader updated, App unchanged
```

---

### **3️⃣ 전체 시스템 업데이트 (Full OTA)**

```
Before: Stage 2A + App A
After:  Stage 2B + App B

Steps:
┌─────────────────────────────────────────────┐
│ 1. Stage 2B 다운로드 → 검증                │
│ 2. App B 다운로드 → 검증                   │
│ 3. EEPROM: stage2_active=B, app_active=B   │
│ 4. Reboot                                  │
│ 5. Stage 1 → Stage 2B → App B              │
└─────────────────────────────────────────────┘

Result: ✅ Complete system refresh!
```

---

### **4️⃣ 롤백 (문제 발생 시)**

```
Scenario: App B 부팅 실패

Boot 1: Stage 2A → App B (FAIL, boot_cnt=1)
Boot 2: Stage 2A → App B (FAIL, boot_cnt=2)  
Boot 3: Stage 2A → App B (FAIL, boot_cnt=3)
        └─ Stage 2A: boot_cnt >= 3 감지!
           └─ Auto-rollback: app_active = A
              └─ Reboot

Boot 4: Stage 2A → App A (SUCCESS!)

┌─────────────────────────────────────────────┐
│ Stage 2B 부팅 실패도 동일:                  │
│                                             │
│ Stage 1: stage2_boot_cnt >= 3               │
│   └─ Auto-rollback: stage2_active = A       │
│      └─ Reboot → Stage 2A                   │
└─────────────────────────────────────────────┘
```

---

## 💻 **Application에서 부트로더 업데이트하기**

### Application의 OTA Manager 코드:

```cpp
// ota_manager.cpp (Application)

enum class OtaTarget {
    APPLICATION,      // App A/B 업데이트
    STAGE2_BOOTLOADER // Stage 2 업데이트 (특수!)
};

bool OtaManager::startDownload(OtaTarget target, uint32_t size, 
                                const FirmwareMetadata& meta) {
    target_type_ = target;
    
    if (target == OtaTarget::STAGE2_BOOTLOADER) {
        // 부트로더 업데이트 (특수 처리!)
        std::cout << "[OTA] ⚠️  BOOTLOADER UPDATE MODE!" << std::endl;
        std::cout << "[OTA] Extra caution enabled" << std::endl;
        
        // Target: Stage 2의 반대편
        BootBank current_stage2 = getCurrentStage2Bank();
        BootBank target_bank = (current_stage2 == BANK_A) ? BANK_B : BANK_A;
        
        target_address_ = (target_bank == BANK_A) ? STAGE2A_START : STAGE2B_START;
        target_bank_ = target_bank;
        
        std::cout << "[OTA] Target: Stage 2" 
                  << (target_bank == BANK_A ? "A" : "B")
                  << " @ 0x" << std::hex << target_address_ << std::dec << std::endl;
        
    } else {
        // 일반 Application 업데이트
        BootBank current_app = getCurrentAppBank();
        BootBank target_bank = (current_app == BANK_A) ? BANK_B : BANK_A;
        
        target_address_ = (target_bank == BANK_A) ? APP_A_START : APP_B_START;
        target_bank_ = target_bank;
    }
    
    return true;
}

bool OtaManager::verify() {
    if (target_type_ == OtaTarget::STAGE2_BOOTLOADER) {
        // 부트로더 검증 (더 엄격!)
        
        // 1. CRC
        if (!verifyCRC(target_address_, target_metadata_.crc32)) {
            return false;
        }
        
        // 2. Signature
        if (!verifySignature(target_address_, target_metadata_.signature)) {
            return false;
        }
        
        // 3. 부트로더 특수 검증
        if (!verifyBootloaderStructure(target_address_)) {
            std::cerr << "[OTA] Bootloader structure invalid!" << std::endl;
            return false;
        }
        
        // 4. Vector table 검증
        uint32_t* vectors = (uint32_t*)target_address_;
        if (vectors[0] < 0x70000000 || vectors[0] > 0x70100000) {
            std::cerr << "[OTA] Invalid stack pointer in bootloader!" << std::endl;
            return false;
        }
        
        std::cout << "[OTA] ✅ Bootloader verification PASSED" << std::endl;
        
    } else {
        // 일반 Application 검증
        return verifyApplication();
    }
    
    return true;
}

bool OtaManager::install() {
    if (target_type_ == OtaTarget::STAGE2_BOOTLOADER) {
        // Stage 2 설치
        std::cout << "[OTA] Installing new Stage 2 bootloader..." << std::endl;
        
        // EEPROM 업데이트
        BootConfig cfg;
        cfg.stage2_active = (target_bank_ == BANK_A) ? 0 : 1;
        cfg.stage2_boot_cnt_a = 0;
        cfg.stage2_boot_cnt_b = 0;
        writeBootConfig(&cfg);
        
        std::cout << "[OTA] ⚠️  BOOTLOADER WILL UPDATE ON NEXT REBOOT" << std::endl;
        std::cout << "[OTA] Please ensure stable power supply!" << std::endl;
        
    } else {
        // App 설치
        installApplication();
    }
    
    return true;
}
```

---

## 🧪 **테스트 시나리오**

### Test 1: Stage 2 업데이트 성공

```bash
# 1. 현재 상태 확인
vmg> tc375
{
  "stage2": "A",
  "app": "A",
  "versions": {
    "stage2_a": "1.0.0",
    "app_a": "2.0.0"
  }
}

# 2. Stage 2B 업데이트
vmg> ota stage2 stage2b_v1.1.hex
[OTA] Downloading to Stage 2B...
[OTA] Progress: 100%
[OTA] Verification: OK
[OTA] Installed, reboot required

# 3. Reboot
vmg> reboot

# 4. 확인
vmg> tc375
{
  "stage2": "B",  ← Changed!
  "app": "A",
  "versions": {
    "stage2_b": "1.1.0",  ← Updated!
    "app_a": "2.0.0"
  }
}
```

### Test 2: 롤백 시뮬레이션

```bash
# 1. 잘못된 부트로더 강제 설치
vmg> ota stage2 corrupt.hex --force

# 2. Reboot
Boot 1: Stage 1 → Stage 2B (CRASH!)
Boot 2: Stage 1 → Stage 2B (CRASH!)
Boot 3: Stage 1 → Stage 2B (CRASH!)
        Stage 1: boot_cnt >= 3 감지
        Stage 1: Rollback to Stage 2A
Boot 4: Stage 1 → Stage 2A → App A (SUCCESS!)

# 3. 시스템 복구됨!
vmg> tc375
{
  "stage2": "A",  ← Rolled back!
  "status": "recovered_from_failure"
}
```

---

## 🛠️ **Application 코드 예시**

### UDS에서 부트로더 OTA 지원:

```cpp
// Application의 UDS Handler

UdsResponse handleRequestDownload(const UdsMessage& request) {
    // 주소 파싱
    uint32_t address = parseAddress(request.data);
    uint32_t size = parseSize(request.data);
    
    // 주소 범위로 타겟 판별
    OtaTarget target;
    
    if (address >= STAGE2A_START && address < STAGE2B_START + STAGE2A_SIZE) {
        // Stage 2 영역!
        target = OtaTarget::STAGE2_BOOTLOADER;
        
        std::cout << "[UDS] ⚠️  BOOTLOADER DOWNLOAD REQUEST!" << std::endl;
        
        // 추가 보안 검증
        if (security_level_ != UNLOCKED) {
            return createNegativeResponse(NRC_SECURITY_ACCESS_DENIED);
        }
        
        // 경고 메시지
        std::cout << "[UDS] Bootloader update is CRITICAL operation" << std::endl;
        
    } else if (address >= APP_A_START && address < APP_B_START + APP_B_SIZE) {
        // Application 영역
        target = OtaTarget::APPLICATION;
        
    } else {
        return createNegativeResponse(NRC_REQUEST_OUT_OF_RANGE);
    }
    
    // OTA Manager에 전달
    ota_manager_->startDownload(target, size, metadata);
    
    return createPositiveResponse({max_block_size});
}
```

---

## 🔐 **보안 강화**

### Stage 2 업데이트 시 추가 검증:

```cpp
bool OtaManager::verifyBootloaderStructure(uint32_t addr) {
    // 1. Vector Table 검증
    uint32_t* vectors = (uint32_t*)addr;
    
    // Stack Pointer (첫 4 바이트)
    if (vectors[0] < 0x70000000 || vectors[0] > 0x70100000) {
        return false;  // Invalid stack
    }
    
    // Reset Handler (다음 4 바이트)
    if (vectors[1] < addr || vectors[1] > addr + STAGE2A_SIZE) {
        return false;  // Reset handler out of range
    }
    
    // 2. 부트로더 Signature 영역 확인
    // 부트로더는 특정 함수들이 있어야 함
    // - stage2_main
    // - stage2_verify_application
    // 등의 심볼 존재 여부
    
    // 3. 최소 크기 검증
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

## 🎯 **Gateway에서의 처리**

### Gateway가 부트로더 OTA 시작:

```cpp
// Gateway (C++)

void OtaController::updateStage2Bootloader(const std::string& device_id) {
    std::cout << "[Gateway] ⚠️  BOOTLOADER UPDATE for " << device_id << std::endl;
    std::cout << "[Gateway] This is a CRITICAL operation!" << std::endl;
    
    // 1. 부트로더 파일 로드
    std::ifstream file("firmware/stage2_v1.1.bin", std::ios::binary);
    std::vector<uint8_t> bootloader_data(...);
    
    // 2. 메타데이터 생성
    FirmwareMetadata meta;
    meta.version = 0x00010001;  // v1.1
    meta.size = bootloader_data.size();
    meta.crc32 = calculateCRC32(bootloader_data);
    
    // 3. PQC 서명 (Dilithium3)
    signWithDilithium3(bootloader_data, meta.signature);
    
    // 4. UDS로 전송
    // Target address: Stage 2B (if current is 2A)
    uint32_t target_addr = getCurrentStage2() == "A" ? STAGE2B_START : STAGE2A_START;
    
    sendUdsRequestDownload(target_addr, meta.size);
    
    // 5. 블록 단위 전송
    for (size_t offset = 0; offset < bootloader_data.size(); offset += 4096) {
        sendUdsTransferData(block_counter++, &bootloader_data[offset], 4096);
        
        // 진행 상황 모니터링
        updateProgress(offset * 100 / bootloader_data.size());
    }
    
    // 6. 완료
    sendUdsRequestTransferExit();
    
    std::cout << "[Gateway] Bootloader uploaded successfully" << std::endl;
    std::cout << "[Gateway] Device will use new bootloader on next reboot" << std::endl;
}
```

---

## ⚡ **최악의 시나리오 & 복구**

### 시나리오: Stage 2A, 2B 모두 손상

```
Boot Sequence:

1. Stage 1 → Stage 2A (CRC FAIL!)
   └─ Stage 1: Try Stage 2B

2. Stage 1 → Stage 2B (CRC FAIL!)
   └─ Stage 1: Both Stage 2 invalid!

3. Stage 1: Enter RECOVERY MODE
   ┌────────────────────────────┐
   │  USB DFU Mode              │
   │  - USB 연결 대기            │
   │  - Stage 2 재플래싱 허용    │
   └────────────────────────────┘

4. User: USB로 Stage 2 업로드
   └─ Stage 1: Stage 2 플래싱

5. Reboot → Stage 1 → Stage 2 (복구!)
```

### Recovery Mode 구현:

```c
// Stage 1 내부

void stage1_enter_recovery(void) {
    debug_print("============================================\n");
    debug_print("  RECOVERY MODE - Stage 2 Corrupted!      \n");
    debug_print("  Connect USB to upload new bootloader     \n");
    debug_print("============================================\n");
    
    // USB DFU (Device Firmware Update)
    initUSB();
    
    while(1) {
        if (usb_data_received()) {
            // USB로 받은 데이터를 Stage 2B에 쓰기
            write_to_stage2b(usb_buffer, usb_length);
            
            if (complete) {
                // 검증
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

## 📊 **메모리 오버헤드 분석**

### Single Bootloader vs Dual Bootloader

```
Single Bootloader:
  Bootloader:   256 KB (1개)
  Application: 5.7 MB (A/B)
  Total Boot:   256 KB
  ───────────────────────
  Available:   5.7 MB for App

Dual Bootloader:
  Stage 1:       64 KB (1개)
  Stage 2:      376 KB (A/B 각 188KB)
  Application: 4.8 MB (A/B 각 2.4MB)
  Total Boot:   440 KB
  ───────────────────────
  Available:   4.8 MB for App
  
오버헤드: 440 - 256 = 184 KB
손실: Application 공간 약 900 KB 감소

→ 하지만 부트로더 업데이트 능력 획득!
```

---

## 💡 **장점 요약**

### 2단계 부트로더의 이점:

1. ✅ **부트로더 버그 수정 가능**
   - 보안 취약점 발견 시 패치
   - 새로운 암호화 알고리즘 추가 (PQC 업그레이드)
   
2. ✅ **3단계 Fail-safe**
   ```
   Stage 2 실패 → Fallback → Recovery
   App 실패 → Fallback → Rollback
   ```

3. ✅ **완전한 시스템 업데이트**
   - 부트로더 + Application 동시 갱신 가능

4. ✅ **긴급 복구 메커니즘**
   - USB DFU로 Stage 2 복구
   - Stage 1은 항상 안전

---

## 🚀 **프로젝트 구조 (최종)**

```
MCUs/
├── tc375_bootloader/
│   ├── stage1/              # 64 KB, 절대 불변
│   │   ├── stage1_main.c
│   │   └── stage1_linker.ld
│   ├── stage2/              # 188 KB, OTA 가능
│   │   ├── stage2_main.c
│   │   └── stage2_linker.ld
│   ├── common/
│   │   └── boot_common.h
│   └── README.md
│
├── tc375_simulator/         # Application (Mac 시뮬레이터)
│   ├── src/
│   │   ├── uds_handler.cpp  ← 부트로더 OTA 처리!
│   │   └── ota_manager.cpp  ← Stage 2 / App 업데이트
│   └── ...
│
└── docs/
    ├── dual_bootloader_ota.md
    └── ...
```

---

## 🎯 **당신이 선택한 옵션 B의 가치:**

### **PQC 미래 대응:**

```
2025: Dilithium3 서명
  ↓
2030: 더 강력한 PQC 알고리즘 등장
  ↓
Stage 2 OTA로 새 알고리즘 지원!
  ↓
전체 시스템 업그레이드 (재플래싱 불필요)
```

**훌륭한 선택입니다!** 🎉

이 구조를 계속 구현하시겠습니까?
