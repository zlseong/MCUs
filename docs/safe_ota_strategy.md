# 안전한 OTA 전략 (ISO 22842 기반)

## ⚠️ **핵심 문제: 운행 중 Flash 쓰기 위험**

### **위험성:**

```
시나리오: 고속도로 주행 중 OTA

Engine ECU (운행 중):
  ├─ Flash에서 연료분사 코드 실행 중
  ├─ OTA: Bank B에 쓰기 시작
  │   └─ Flash 섹터 erase (500ms)
  │       └─ 해당 섹터 읽기 블록!
  ├─ Critical ISR이 Flash에 있음
  │   └─ ISR 실행 불가!
  └─ 엔진 제어 실패 💥

Result: 주행 중 엔진 정지 → 사고!
```

### **Flash 물리적 제약:**

```
TC375 Flash:
  Erase Sector: 500 ms (읽기 블록됨)
  Write Page:   5-10 ms (읽기 블록됨)
  
Critical Functions:
  - ISR (Interrupt Service Routine)
  - Fuel injection
  - Brake control
  - Steering assist
  
→ 이들이 Flash에 있으면 쓰기 중 멈춤!
```

---

## ✅ **산업 표준: Staged OTA**

### **3단계 프로세스:**

```
┌──────────────────────────────────────────────────┐
│  Phase 1: Download (IGN ON, 운행 중 가능)       │
│  ─────────────────────────────────────────────   │
│  외부 저장소에 다운로드                          │
│  • Internal Flash 건드리지 않음!                │
│  • 정상 운행 계속                                │
└──────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────┐
│  Phase 2: Verify (IGN ON, 운행 중 가능)         │
│  ─────────────────────────────────────────────   │
│  다운로드된 파일 검증                            │
│  • CRC, Signature 확인                          │
│  • Internal Flash 건드리지 않음                  │
└──────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────┐
│  Phase 3: Install (IGN OFF, 시동 꺼진 후!)       │
│  ─────────────────────────────────────────────   │
│  실제 Flash 프로그래밍                           │
│  • 차량 정지 상태                                │
│  • 안전 확인 후 진행                             │
└──────────────────────────────────────────────────┘
```

---

## 🏗️ **Zonal/Domain Controller 역할 (당신의 제안!)**

### **계층 구조:**

```
                 ┌──────────────┐
                 │   Server     │
                 └──────┬───────┘
                        │ OTA Package
                        ▼
              ┌─────────────────────┐
              │  Gateway            │
              │  (Zonal Controller) │
              │                     │
              │  ┌───────────────┐  │
              │  │ Flash 4 Click │  │ ← 64 MB 외부 저장소!
              │  │ (OTA Buffer)  │  │
              │  └───────────────┘  │
              └──────────┬──────────┘
                         │ CAN/Ethernet
                         │
           ┌─────────────┼─────────────┐
           ▼             ▼             ▼
      ┌────────┐    ┌────────┐    ┌────────┐
      │ ECU #1 │    │ ECU #2 │    │ ECU #3 │
      │ (TC375)│    │ (TC375)│    │ (TC375)│
      └────────┘    └────────┘    └────────┘
```

### **Flash 4 Click 활용:**

```
Flash 4 Click (64 MB):
┌─────────────────────────────────┐
│  OTA Download Buffer            │
│  ────────────────────────────   │
│  ECU #1 Firmware: 3 MB          │
│  ECU #2 Firmware: 3 MB          │
│  ECU #3 Firmware: 3 MB          │
│  ...                            │
│  Gateway Firmware: 5 MB         │
│                                 │
│  Total: 최대 20개 ECU 펌웨어    │
└─────────────────────────────────┘

장점:
✅ 운행 중 다운로드 → 외부 Flash (안전!)
✅ IGN OFF 시 Internal Flash로 복사
✅ 다중 ECU 동시 업데이트 준비
```

---

## 💻 **구현 코드**

### **1. OTA State Machine (안전 조건 추가)**

```cpp
// ota_manager.hpp

enum class VehicleState {
    IGN_OFF,           // 시동 꺼짐 (안전)
    IGN_ON_PARKED,     // 시동 켜짐 + 주차 (조건부 안전)
    IGN_ON_DRIVING,    // 주행 중 (위험!)
    CHARGING           // 전기차 충전 중 (안전)
};

class SafeOtaManager {
public:
    // Download to external storage (IGN ON OK)
    bool downloadToBuffer(const std::string& firmware_url);
    
    // Verify without touching internal Flash (IGN ON OK)
    bool verifyInBuffer();
    
    // Install to internal Flash (IGN OFF ONLY!)
    bool installFromBuffer();
    
    // Safety checks
    bool isInstallSafe() const;
    VehicleState getVehicleState() const;
    
private:
    std::string external_buffer_;  // Flash 4 Click path
    VehicleState vehicle_state_;
};

bool SafeOtaManager::isInstallSafe() const {
    VehicleState state = getVehicleState();
    
    // 안전 조건
    if (state == VehicleState::IGN_OFF) {
        return true;  // ✅ 가장 안전
    }
    
    if (state == VehicleState::CHARGING) {
        return true;  // ✅ 전기차 충전 중
    }
    
    if (state == VehicleState::IGN_ON_PARKED) {
        // 추가 조건 확인
        if (isParked() && getSpeed() == 0 && getBrakePressed()) {
            return true;  // ⚠️ 조건부 안전
        }
    }
    
    return false;  // ❌ 위험!
}

bool SafeOtaManager::downloadToBuffer(const std::string& url) {
    // IGN 상태 무관 - 외부 저장소 사용
    std::cout << "[OTA] Downloading to external buffer (safe in any state)" << std::endl;
    
    external_buffer_ = "/external_flash/ota_buffer.bin";
    
    // Download from server
    auto firmware = http_client_->get(url);
    
    // Save to Flash 4 Click (외부 저장소)
    writeToExternalFlash(external_buffer_, firmware.body);
    
    std::cout << "[OTA] Download complete, ready for install" << std::endl;
    std::cout << "[OTA] Waiting for safe install condition (IGN OFF)" << std::endl;
    
    return true;
}

bool SafeOtaManager::installFromBuffer() {
    // 안전 조건 확인!
    if (!isInstallSafe()) {
        std::cerr << "[OTA] Install NOT SAFE in current vehicle state!" << std::endl;
        std::cerr << "[OTA] Please turn off ignition" << std::endl;
        return false;
    }
    
    std::cout << "[OTA] Vehicle state: SAFE for installation" << std::endl;
    std::cout << "[OTA] Installing firmware to internal Flash..." << std::endl;
    
    // Read from external buffer
    auto firmware = readFromExternalFlash(external_buffer_);
    
    // Write to internal Flash Bank B
    // (IGN OFF이므로 안전!)
    writeToInternalFlash(BANK_B_START, firmware);
    
    return true;
}
```

---

## 🔄 **완전한 OTA 흐름 (안전 버전)**

### **시나리오: ECU #1 업데이트**

```
Day 1 (주행 중):
  10:00 Server: "ECU #1 v1.1 available"
  10:01 Gateway: Download to Flash 4 Click (3 MB)
        └─ 외부 저장소 ✅ 안전!
        └─ path: /flash4/ecu1_v1.1.bin
        
  10:05 Gateway: Verify
        ├─ CRC32: OK
        ├─ Signature: OK
        └─ Version: OK
        
  10:06 Gateway → Driver:
        "ECU #1 update ready. Will install when parked."
        
  15:00 Still driving... (업데이트 대기 중)
        └─ Flash 4 Click에 파일만 저장됨
        └─ ECU #1은 정상 동작 중

Day 1 (시동 off):
  18:00 Driver: 시동 끄기
        ↓
  18:01 Gateway: IGN OFF 감지!
        ↓
  18:02 Gateway → ECU #1 (CAN):
        "Start OTA install"
        ↓
  18:03 ECU #1: 
        ├─ Gateway에서 펌웨어 수신 (Flash 4 Click)
        ├─ Internal Bank B에 쓰기 ✅ 안전!
        └─ 완료 (30초)
        
  18:04 ECU #1 → Gateway: "Install complete"
        
Day 2 (다음 시동):
  08:00 Driver: 시동 켜기
        ↓
  08:01 ECU #1 Bootloader:
        ├─ Bank B 검증
        ├─ Bank B 부팅
        └─ v1.1 실행! ✅
```

---

## 🏗️ **Zonal Controller 패턴 (업계 표준)**

### **Tesla/Mercedes/BMW 방식:**

```
                    ┌────────────────┐
                    │  Cloud Server  │
                    └────────┬───────┘
                             │ Full OTA Package
                             ▼
              ┌──────────────────────────┐
              │  Central Gateway         │
              │  (Zonal Controller)      │
              │                          │
              │  ┌────────────────────┐  │
              │  │ Large Storage      │  │
              │  │ - eMMC/SSD (32GB)  │  │
              │  │ or                 │  │
              │  │ - Flash 4 Click    │  │
              │  │   (64 MB)          │  │
              │  │                    │  │
              │  │ Stores:            │  │
              │  │ • ECU#1 FW (3MB)   │  │
              │  │ • ECU#2 FW (3MB)   │  │
              │  │ • ECU#N FW         │  │
              │  │ • Gateway FW       │  │
              │  └────────────────────┘  │
              └────────┬─────────────────┘
                       │
                       │ CAN/Ethernet
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼
    ┌────────┐    ┌────────┐    ┌────────┐
    │ ECU #1 │    │ ECU #2 │    │ ECU #N │
    │        │    │        │    │        │
    │ 6 MB   │    │ 6 MB   │    │ 6 MB   │
    │ Flash  │    │ Flash  │    │ Flash  │
    └────────┘    └────────┘    └────────┘
```

### **메모리 전략:**

```
Gateway (With Flash 4 Click 64 MB):
┌─────────────────────────────────────┐
│  Internal Storage (eMMC/SD)         │
│  or Flash 4 Click (64 MB)           │
│                                     │
│  OTA Package Repository:            │
│  ├─ gateway_v1.1.bin      (5 MB)   │
│  ├─ ecu_engine_v2.0.bin   (3 MB)   │
│  ├─ ecu_brake_v1.5.bin    (3 MB)   │
│  ├─ ecu_adas_v3.0.bin     (4 MB)   │
│  └─ ... (최대 10+ ECUs)             │
│                                     │
│  각 ECU는 작은 Flash (6 MB)만 가짐  │
│  Gateway가 모든 펌웨어 보관!        │
└─────────────────────────────────────┘
```

---

## 🔧 **구현: 안전한 OTA Manager**

### **파일 작성:**

```cpp
// ota_orchestrator.hpp

namespace vmg {

// Vehicle Safety State
enum class VehicleState {
    IGN_OFF,           // ✅ 완전 안전
    IGN_ACC,           // ✅ 안전 (액세서리만)
    IGN_ON_PARKED,     // ⚠️ 조건부 (주차 + 0km/h)
    IGN_ON_DRIVING,    // ❌ 위험!
    CHARGING           // ✅ 안전 (EV)
};

// OTA Target ECU
struct EcuOtaTarget {
    std::string ecu_id;
    std::string firmware_version;
    uint32_t firmware_size;
    std::string buffer_path;  // Flash 4 Click path
    bool download_complete;
    bool verified;
    bool install_pending;
};

// OTA Orchestrator (Gateway)
class OtaOrchestrator {
public:
    OtaOrchestrator();
    
    // === Phase 1: Download (IGN ON OK) ===
    bool downloadFirmware(const std::string& ecu_id, 
                         const std::string& firmware_url);
    
    // === Phase 2: Verify (IGN ON OK) ===
    bool verifyFirmware(const std::string& ecu_id);
    
    // === Phase 3: Install (IGN OFF ONLY!) ===
    bool installFirmware(const std::string& ecu_id);
    
    // Batch operations
    bool downloadAllPendingUpdates();
    bool installAllWhenSafe();
    
    // Safety checks
    VehicleState getVehicleState() const;
    bool isInstallSafe() const;
    void waitForSafeState();
    
    // Storage management (Flash 4 Click)
    std::string allocateBufferSpace(const std::string& ecu_id, uint32_t size);
    bool freeBufferSpace(const std::string& ecu_id);
    uint32_t getAvailableBufferSpace() const;
    
private:
    std::map<std::string, EcuOtaTarget> pending_updates_;
    std::string external_storage_path_;  // "/mnt/flash4click"
    
    // Vehicle state monitoring
    bool readIgnitionState() const;
    bool readParkingBrake() const;
    float readVehicleSpeed() const;
    bool readChargingState() const;
};

} // namespace vmg
```

---

## 🚦 **안전 조건 체크**

### **실제 구현:**

```cpp
VehicleState OtaOrchestrator::getVehicleState() const {
    // CAN 버스에서 차량 상태 읽기
    bool ign_on = readIgnitionState();
    bool charging = readChargingState();  // EV only
    float speed = readVehicleSpeed();
    bool parked = readParkingBrake();
    
    if (!ign_on) {
        return VehicleState::IGN_OFF;  // ✅ 가장 안전
    }
    
    if (charging) {
        return VehicleState::CHARGING;  // ✅ EV 충전 중
    }
    
    if (speed > 0.1f) {
        return VehicleState::IGN_ON_DRIVING;  // ❌ 위험!
    }
    
    if (parked && speed == 0.0f) {
        return VehicleState::IGN_ON_PARKED;  // ⚠️ 조건부
    }
    
    return VehicleState::IGN_ON_DRIVING;  // Default: 위험
}

bool OtaOrchestrator::isInstallSafe() const {
    VehicleState state = getVehicleState();
    
    switch (state) {
        case VehicleState::IGN_OFF:
        case VehicleState::CHARGING:
            return true;  // ✅ 완전 안전
            
        case VehicleState::IGN_ACC:
            return true;  // ✅ 엔진 꺼짐
            
        case VehicleState::IGN_ON_PARKED:
            // 조건부: Non-critical ECU만
            // (예: Infotainment는 OK, Engine은 NO)
            return false;  // 보수적 접근
            
        case VehicleState::IGN_ON_DRIVING:
            return false;  // ❌ 절대 안 됨!
            
        default:
            return false;
    }
}

void OtaOrchestrator::waitForSafeState() {
    std::cout << "[OTA] Waiting for safe install condition..." << std::endl;
    std::cout << "[OTA] Current state: ";
    
    while (!isInstallSafe()) {
        VehicleState state = getVehicleState();
        
        switch (state) {
            case VehicleState::IGN_ON_DRIVING:
                std::cout << "DRIVING (unsafe)" << std::endl;
                std::cout << "[OTA] Please park and turn off ignition" << std::endl;
                break;
                
            case VehicleState::IGN_ON_PARKED:
                std::cout << "PARKED (turn off ignition)" << std::endl;
                break;
                
            default:
                break;
        }
        
        // Wait and recheck
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "[OTA] Safe state detected: IGN OFF" << std::endl;
    std::cout << "[OTA] Starting installation..." << std::endl;
}
```

---

## 📋 **OTA 시퀀스 (Flash 4 Click 활용)**

### **Complete Flow:**

```cpp
// Gateway OTA Controller

void performSafeOta(const std::string& ecu_id, const std::string& firmware_url) {
    
    // ============================================================
    // PHASE 1: Download (운행 중 가능!)
    // ============================================================
    
    std::cout << "=== Phase 1: Download (IGN ON OK) ===" << std::endl;
    
    // 1.1 Flash 4 Click에 공간 확보
    std::string buffer = ota_->allocateBufferSpace(ecu_id, 3 * 1024 * 1024);
    // → "/mnt/flash4/ecu_engine_v2.0.bin"
    
    // 1.2 Server에서 다운로드
    auto response = http_client_->get(firmware_url);
    
    // 1.3 외부 Flash에 저장
    writeToFlash4Click(buffer, response.body);
    
    std::cout << "[OTA] Downloaded to external storage (SAFE)" << std::endl;
    
    // ============================================================
    // PHASE 2: Verify (운행 중 가능!)
    // ============================================================
    
    std::cout << "=== Phase 2: Verify (IGN ON OK) ===" << std::endl;
    
    // 2.1 CRC 검증
    uint32_t crc = calculateCRC32(buffer);
    if (crc != expected_crc) {
        std::cerr << "[OTA] CRC failed, aborting" << std::endl;
        return;
    }
    
    // 2.2 PQC 서명 검증
    if (!verifyDilithiumSignature(buffer, signature)) {
        std::cerr << "[OTA] Signature failed, aborting" << std::endl;
        return;
    }
    
    std::cout << "[OTA] Verification: PASSED" << std::endl;
    std::cout << "[OTA] Ready to install" << std::endl;
    
    // ============================================================
    // PHASE 3: Wait for Safe State
    // ============================================================
    
    std::cout << "=== Phase 3: Waiting for IGN OFF ===" << std::endl;
    
    // 3.1 Driver에게 알림
    notifyDriver("ECU update ready. Please park and turn off ignition.");
    
    // 3.2 안전 상태 대기
    ota_->waitForSafeState();  // Blocking!
    
    // ============================================================
    // PHASE 4: Install (IGN OFF - 안전!)
    // ============================================================
    
    std::cout << "=== Phase 4: Install (IGN OFF - SAFE) ===" << std::endl;
    
    // 4.1 ECU에게 준비 명령 (UDS)
    sendUdsCommand(ecu_id, UDS_REQUEST_DOWNLOAD, BANK_B_ADDRESS, size);
    
    // 4.2 Flash 4 Click → ECU 전송
    transferFromBufferToEcu(buffer, ecu_id);
    
    // 4.3 ECU: Internal Flash에 쓰기
    // (IGN OFF이므로 안전!)
    
    // 4.4 완료
    std::cout << "[OTA] Installation complete" << std::endl;
    std::cout << "[OTA] ECU will use new firmware on next IGN ON" << std::endl;
}
```

---

## 🎯 **당신의 구조에 적용**

### **필요한 추가 사항:**

```cpp
vehicle_gateway/
├── include/
│   ├── ota_orchestrator.hpp  ⏳ NEW!
│   └── flash4_driver.hpp     ⏳ NEW! (Flash 4 Click)
├── src/
│   ├── ota_orchestrator.cpp  ⏳ NEW!
│   └── flash4_driver.cpp     ⏳ NEW!
└── config/
    └── ota_policy.json       ⏳ NEW! (안전 정책)
```

### **OTA Policy 예시:**

```json
{
  "ota_safety_policy": {
    "download_allowed": {
      "ign_off": true,
      "ign_on_parked": true,
      "ign_on_driving": true,
      "charging": true
    },
    "install_allowed": {
      "ign_off": true,
      "ign_on_parked": false,
      "ign_on_driving": false,
      "charging": true
    },
    "critical_ecus": [
      "engine_ecu",
      "brake_ecu",
      "steering_ecu"
    ],
    "non_critical_ecus": [
      "infotainment",
      "hvac",
      "lights"
    ]
  },
  "storage": {
    "external_flash": "/mnt/flash4click",
    "max_concurrent_downloads": 5,
    "retention_days": 7
  }
}
```

---

## 🚀 **완전한 안전 OTA 시스템**

### **Requirements:**

```
Hardware:
✅ Gateway: Raspberry Pi 4 + Flash 4 Click (64 MB)
✅ TC375: Lite Kit (6 MB internal Flash)

Software:
✅ Gateway: OTA Orchestrator
✅ Gateway: Flash 4 Click driver
✅ Gateway: Vehicle state monitor (CAN reading)
✅ TC375: A/B Bootloader
✅ TC375: UDS Handler

Safety:
✅ IGN state monitoring
✅ Vehicle speed checking
✅ Parking brake status
✅ Download/Install separation
✅ External buffer (Flash 4 Click)
```

---

## 💡 **결론**

### **당신의 지적이 완벽합니다!**

```
❌ 잘못된 설계:
   운행 중 Internal Flash 직접 쓰기
   
✅ 올바른 설계 (당신의 제안!):
   1. 운행 중: Gateway의 Flash 4 Click에 다운로드
   2. 검증 완료
   3. IGN OFF 대기
   4. 시동 꺼진 후: ECU Internal Flash에 쓰기
```

### **Flash 4 Click의 진짜 가치:**

```
단순한 확장 저장소가 아니라,
OTA 안전성의 핵심 컴포넌트!

• 운행 중 안전한 다운로드 버퍼
• 다중 ECU 펌웨어 저장소
• 롤백용 백업 저장
```

---

## 🎯 **다음 작업 우선순위:**

1. ✅ Gateway ↔ Server 연결 (먼저!)
2. ⏳ OTA Orchestrator 구현
3. ⏳ Flash 4 Click 드라이버
4. ⏳ Vehicle State Monitor
5. ⏳ Safe Install Logic

**지금은 Server 연결 테스트가 우선이죠!** 👍

이 안전 전략을 문서로 저장해두시겠습니까?
