# 데이터 관리 전략: ECU ID, MAC, IP, VIN 등

## 📊 데이터 분류 및 저장 위치

### **3-Tier Storage Strategy**

```
┌─────────────────────────────────────────────────┐
│  Flash (Permanent - Immutable)                  │
│  - ECU Serial Number ✓                          │
│  - MAC Address ✓                                │
│  - VIN (Vehicle ID) ✓                           │
│  - Hardware Version ✓                           │
│  - Calibration Data ✓                           │
│                                                 │
│  특징:                                           │
│  • 공장에서 1회 프로그래밍                       │
│  • Write-Protected                              │
│  • 절대 변경 안 함                               │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│  EEPROM (Configuration - Rewritable)            │
│  - IP Address ⚙️                                │
│  - Gateway Host/Port ⚙️                         │
│  - Feature Flags ⚙️                             │
│  - CAN Settings ⚙️                              │
│  - Log Level ⚙️                                 │
│                                                 │
│  특징:                                           │
│  • UDS WriteDataByID로 변경 가능                │
│  • 재부팅 후에도 유지                            │
│  • 10만 회 쓰기 가능                             │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│  RAM (Runtime - Volatile)                       │
│  - 센서 값 (Temperature, Pressure) 📊           │
│  - CPU/Memory 사용률 📊                         │
│  - 연결 상태 📊                                  │
│  - Uptime 📊                                    │
│                                                 │
│  특징:                                           │
│  • 실시간으로 변함                               │
│  • 재부팅 시 사라짐                              │
│  • JSON으로 Gateway에 주기 전송                  │
└─────────────────────────────────────────────────┘
```

---

## 📡 **JSON 전송 전략**

### **1. 디바이스 등록 (최초 1회)**

```json
// TC375 → Gateway (전원 켜진 후 첫 연결)

{
  "type": "DEVICE_REGISTRATION",
  "device": {
    // === Flash 데이터 (영구) ===
    "ecu_serial": "TC375-SIM-001-20251021",
    "mac_address": "02:00:00:AA:BB:CC",
    "hardware_version": "TC375TP-LiteKit-v2.0",
    "vin": "KMHGH4JH1NU123456",
    "vehicle_model": "Genesis G80 EV",
    "vehicle_year": 2025,
    
    // === EEPROM 데이터 (설정) ===
    "ip_address": "192.168.1.100",
    "gateway_host": "gateway.example.com",
    "gateway_port": 8765,
    "tls_enabled": true,
    "ota_enabled": true,
    
    // === Flash 데이터 (펌웨어) ===
    "firmware_version": "1.0.0",
    "bootloader_version": "1.0.0"
  },
  "timestamp": "2025-10-21 15:30:00"
}

// Gateway → Server DB에 저장
// 이후 ecu_serial이 Primary Key
```

### **2. Heartbeat (10초마다)**

```json
// TC375 → Gateway (주기적)

{
  "type": "HEARTBEAT",
  "device_id": "TC375-SIM-001-20251021",  // 식별자만!
  "payload": {
    "status": "alive"
  },
  "timestamp": "2025-10-21 15:30:10"
}

// 💡 MAC, VIN 등은 매번 안 보냄!
// Server DB에 이미 있으니까!
```

### **3. 센서 데이터 (5초마다)**

```json
// TC375 → Gateway (주기적)

{
  "type": "SENSOR_DATA",
  "device_id": "TC375-SIM-001-20251021",
  "payload": {
    // RAM 데이터만 (실시간)
    "temperature": 25.5,
    "pressure": 101.3,
    "voltage": 12.0,
    "rpm": 2000,
    "speed": 80
  },
  "timestamp": "2025-10-21 15:30:15"
}
```

### **4. 상태 보고 (1분마다)**

```json
// TC375 → Gateway

{
  "type": "STATUS_REPORT",
  "device_id": "TC375-SIM-001-20251021",
  "payload": {
    // RAM (동적)
    "uptime": 3600,
    "cpu_usage": 45.2,
    "memory_free": 2048,
    "gateway_connected": true,
    
    // EEPROM (현재 설정)
    "current_ip": "192.168.1.100",
    "ota_enabled": true,
    
    // Flash (펌웨어 정보)
    "active_bank": "A",
    "firmware_version": "1.0.0"
  },
  "timestamp": "2025-10-21 15:31:00"
}
```

---

## 🔧 **UDS로 설정 변경**

### **Gateway → TC375: IP 주소 변경**

```json
// Gateway에서 명령

{
  "command": "WRITE_CONFIG",
  "device_id": "TC375-SIM-001-20251021",
  "parameters": {
    "data_id": "0xF190",  // IP Address (UDS DID)
    "value": "192.168.1.200"
  }
}

// ↓ UDS 프로토콜로 변환
// UDS: WriteDataByID(0xF190, [192, 168, 1, 200])

// TC375 처리:
handleWriteDataById(request) {
    if (data_id == 0xF190) {
        // EEPROM에 저장
        device_info.updateIPAddress(new_ip);
        saveToEEPROM();
        
        return POSITIVE_RESPONSE;
    }
}

// TC375 → Gateway 확인

{
  "type": "CONFIG_UPDATED",
  "device_id": "TC375-SIM-001-20251021",
  "data_id": "0xF190",
  "old_value": "192.168.1.100",
  "new_value": "192.168.1.200"
}
```

---

## 💾 **실제 TC375 메모리 구조**

### **Flash Configuration Sector:**

```c
// 0x80572000 - Configuration Sector (256 KB)

struct FlashConfigSector {
    // Offset 0x0000
    PermanentConfig permanent;     // 1 KB
    
    // Offset 0x0400 (1 KB)
    uint8_t production_cert[1024]; // 공장 인증서
    
    // Offset 0x0800 (2 KB)
    uint8_t calibration_table[2048];
    
    // 나머지: Reserved for future
} __attribute__((aligned(4096)));

// Write Protection
void lockPermanentConfig(void) {
    IfxFlash_setProtection(
        0x80572000, 
        0x80573000,  // 첫 4KB만 보호
        IfxFlash_Protection_write
    );
}
```

### **EEPROM Layout:**

```c
// TC375 EEPROM (64 KB total)

0xAF000000  ┌────────────────────────┐
            │ Boot Config (512 B)    │  ← Stage 1/2, App 뱅크 선택
0xAF000200  ├────────────────────────┤
            │ Device Config (1 KB)   │  ← IP, Port, Flags
0xAF000600  ├────────────────────────┤
            │ Network Config (1 KB)  │  ← DNS, Subnet, etc.
0xAF000A00  ├────────────────────────┤
            │ CAN Config (2 KB)      │  ← CAN IDs, Filters
0xAF001200  ├────────────────────────┤
            │ User Settings (4 KB)   │  ← 사용자 정의
0xAF002200  ├────────────────────────┤
            │ Reserved (56 KB)       │
0xAF010000  └────────────────────────┘
```

---

## 📤 **전송 최적화**

### **불필요한 데이터 전송 방지:**

```cpp
// BAD: 매번 모든 정보 전송 (❌)
void sendHeartbeat() {
    json msg = {
        {"ecu_serial", "..."},      // 매번 보냄 (불필요!)
        {"mac", "..."},             // 매번 보냄 (불필요!)
        {"vin", "..."},             // 매번 보냄 (불필요!)
        {"status", "alive"}
    };
    send(msg);
}

// GOOD: 식별자만 전송 (✅)
void sendHeartbeat() {
    json msg = {
        {"device_id", device_info.getEcuSerial()},  // 식별자
        {"type", "HEARTBEAT"},
        {"payload", {"status", "alive"}}
    };
    send(msg);
}

// Server는 device_id로 DB 조회하여
// MAC, VIN 등을 알아낼 수 있음!
```

---

## 🎯 **답변 요약**

### **Q: ECU ID, MAC, IP, VIN 같은 정보는 JSON으로 보내지나요?**

### **A: 경우에 따라 다릅니다!**

**최초 등록 시 (1회):**
```json
✅ 모든 정보를 JSON으로 전송
   - ECU Serial, MAC, VIN, IP 등
```

**이후 통신 (주기적):**
```json
✅ device_id만 전송
   - ECU Serial만 보냄
   - Server DB에서 나머지 조회
   
⚠️ 필요한 경우만 설정 포함
   - IP 변경되었을 때
   - Config 업데이트 시
```

**저장 위치:**
```
ECU Serial, MAC, VIN → Flash (공장 프로그래밍, 영구)
IP, Port, Flags      → EEPROM (UDS로 변경 가능)
센서 값, 상태        → RAM (JSON으로만 전송)
```

**전송 방법:**
```
등록:       전체 정보 JSON ← 1회만!
일반 통신:   device_id만 ← 매번
설정 변경:   UDS WriteDataByID ← 필요 시
```

명확하신가요? 😊
