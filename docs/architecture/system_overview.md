# 시스템 개요 - VMG and MCUs

## 📊 전체 아키텍처

```
                         ┌─────────────┐
                         │   Server    │ (Cloud)
                         │  (PQC-TLS)  │
                         └──────┬──────┘
                                │ HTTPS/MQTT
                                │ JSON Messages
                    ┌───────────▼───────────┐
                    │        VMG            │ MacBook Air (Linux x86)
                    │  (Central Gateway)    │
                    │  - DoIP Server        │
                    │  - PQC-TLS Client     │
                    │  - VCI Aggregation    │
                    └───────────┬───────────┘
                                │ DoIP (TCP 13400)
                    ┌───────────┼───────────┐
                    │           │           │
            ┌───────▼────┐ ┌───▼─────┐ ┌──▼──────┐
            │   ZG #1    │ │  ZG #2  │ │  ZG #3  │ TC375 or Linux
            │  (Zone 1)  │ │(Zone 2) │ │(Zone 3) │
            │  - Server  │ │         │ │         │
            │  - Client  │ │         │ │         │
            └─────┬──────┘ └────┬────┘ └────┬────┘
                  │             │           │
         ┌────────┼────┐       │           │
         │        │    │       │           │
    ┌────▼──┐ ┌──▼───┐│  ┌───▼──┐    ┌───▼──┐
    │ECU #1 │ │ECU #2││  │ECU #4│    │ECU #6│    TC375 Only
    │0x0201 │ │0x0202││  │0x0211│    │0x0221│
    └───────┘ └──────┘│  └──────┘    └──────┘
                      │
                 ┌────▼───┐
                 │ECU #3  │
                 │0x0203  │
                 └────────┘
```

## 🎯 3-Tier 아키텍처

### Tier 1: Cloud Server
- **역할**: OTA 패키지 배포, 차량 관리
- **프로토콜**: HTTPS, MQTT
- **보안**: PQC-TLS (ML-KEM + ML-DSA)
- **데이터**: JSON 메시지

### Tier 2: VMG (Central Gateway / CCU)
- **플랫폼**: MacBook Air (Linux x86)
- **역할**:
  - Central Gateway
  - DoIP Server (ZG들 대상)
  - PQC-TLS Client (Server 대상)
  - Vehicle VCI 통합
  - OTA 조율
- **주소**: 192.168.1.1
- **포트**: 13400 (DoIP)

### Tier 3: Zonal Gateway (ZG)
- **플랫폼**: 
  - TC375 MCU (실제 하드웨어)
  - Linux x86 (개발/시뮬레이션)
- **역할**:
  - Zone 내 ECU 관리
  - DoIP Server (ECU 대상)
  - DoIP Client (VMG 대상)
  - Zone VCI 집계
  - OTA Zone 분배
- **Zone 구성**:
  - Zone 1: 192.168.1.10 (0x0201)
  - Zone 2: 192.168.1.20 (0x0202)
  - Zone 3: 192.168.1.30 (0x0203)

### Tier 4: End Node ECU
- **플랫폼**: TC375 MCU 전용
- **역할**:
  - 최종 End Node
  - DoIP Client (ZG 대상)
  - UDS 서비스 제공
  - OTA 펌웨어 수신/설치
- **주소**: Zone별로 할당

## 🌐 프로토콜 스택

### Layer 7: Application
```
VMG ↔ Server:  JSON (MQTT/HTTPS)
VMG ↔ ZG:      DoIP/UDS
ZG ↔ ECU:      DoIP/UDS
```

### Layer 4: Transport
```
MQTT:          TCP 1883/8883
HTTPS:         TCP 443
DoIP:          TCP 13400 (Diagnostic)
               UDP 13400 (Discovery)
```

### Layer 3: Network
```
IPv4:          192.168.1.0/24
               192.168.1.1    (VMG)
               192.168.1.10-39 (ZG)
               192.168.1.100+  (ECU)
```

## 📡 통신 프로토콜

### 1. Server ↔ VMG

#### MQTT (Control Messages)
```json
{
  "topic": "v2x/vmg/{vin}/command",
  "payload": {
    "message_type": "OTA_PACKAGE_AVAILABLE",
    "campaign_id": "OTA-2025-001",
    "package_url": "https://cdn.ota.com/..."
  }
}
```

#### HTTPS (File Transfer)
```
GET /packages/global_v2.0.0.tar.gz
Authorization: Bearer <token>
→ 200 OK
Content-Type: application/octet-stream
Content-Length: 104857600
```

### 2. VMG ↔ ZG (DoIP)

#### Vehicle Discovery (UDP)
```
ECU → ZG: DoIP Vehicle Identification Request (UDP Broadcast)
ZG → ECU: DoIP Vehicle Identification Response (VIN, Address)
```

#### Diagnostic Communication (TCP)
```
VMG → ZG: DoIP Routing Activation Request
ZG → VMG: DoIP Routing Activation Response (0x10 Success)

VMG → ZG: DoIP Diagnostic Message (UDS 0x22 F190)
ZG → VMG: DoIP Diagnostic Message (62 F1 90 + VIN)
```

### 3. ZG ↔ ECU (DoIP)

동일하게 DoIP/UDS 사용

## 🔄 데이터 흐름

### VCI (Vehicle Configuration Information) 수집

```
ECU (UDS 0x22) → ZG → VMG → Server

1. ECU: Send DID values
   - VIN (F190)
   - SW Version (F195)
   - HW Version (F193)

2. ZG: Aggregate Zone VCI
   {
     "zone_id": 1,
     "ecus": [...]
   }

3. VMG: Aggregate Vehicle VCI
   {
     "vin": "...",
     "zones": [...]
   }

4. Server: Store in database
```

### OTA 패키지 분배

```
Server → VMG → ZG → ECU

1. Server: Push global package (100MB)
   → VMG: Download via HTTPS

2. VMG: Extract zone packages
   zone1_v2.0.0.bin (20MB)
   zone2_v2.0.0.bin (20MB)
   → ZG: Send via TCP

3. ZG: Extract ECU binaries
   ECU-002.bin (5MB)
   ECU-003.bin (5MB)
   → ECU: Send via DoIP (UDS 0x34/0x36/0x37)

4. ECU: Write to inactive bank (Region B)
```

## 🔐 보안 레이어

### Server ↔ VMG: PQC-TLS
```
ML-KEM-768    (Key Exchange)
ML-DSA-65     (Digital Signature)
ECDSA-P256    (Hybrid)
AES-256-GCM   (Encryption)
```

### VMG ↔ ZG: Optional TLS
```
TLS 1.3 (선택적)
또는 Plain DoIP
```

### ZG ↔ ECU: Plain DoIP
```
In-vehicle network
물리적 격리
```

## 📊 메시지 포맷

### JSON Messages (VMG ↔ Server)
- Heartbeat
- VCI Report
- OTA Status
- Diagnostic Events
- Error Reports

자세한 내용: [unified_message_format.md](../protocols/unified_message_format.md)

### Binary Messages (DoIP/UDS)
- DoIP: ISO 13400
- UDS: ISO 14229

자세한 내용: [ISO_13400_specification.md](../protocols/ISO_13400_specification.md)

## 🏗️ 배포 구성

### 개발 환경
```
VMG:        MacBook Air
ZG:         Linux (시뮬레이션)
ECU:        TC375 실제 하드웨어
```

### 프로토타입 환경
```
VMG:        MacBook Air
ZG:         TC375 (일부 노트북 대체 가능)
ECU:        TC375
```

### 실제 차량
```
VMG:        차량 내장 Linux 게이트웨이
ZG:         TC375 (Zone별 배치)
ECU:        TC375 (각 도메인별)
```

## 📈 확장성

### Zone 확장
- 현재: 3 Zones
- 최대: 8-16 Zones
- Zone당 ECU: 최대 8개

### ECU 확장
- Zone 1: 엔진 도메인
- Zone 2: 샤시 도메인
- Zone 3: 바디 도메인
- Zone 4-6: 미래 확장

### 프로토콜 확장
- CAN 통합
- FlexRay 지원
- Automotive Ethernet
- SOME/IP

## 🔧 개발 도구

### VMG
- **IDE**: VS Code, CLion
- **Build**: CMake, GCC
- **Debug**: GDB, Valgrind

### Zonal Gateway (Linux)
- **IDE**: VS Code
- **Build**: CMake, GCC
- **Debug**: GDB

### TC375 (ZG & ECU)
- **IDE**: AURIX Development Studio
- **Toolchain**: tricore-gcc
- **Debug**: JTAG (TASKING)
- **Flash**: Infineon Memtool

## 📚 관련 문서

- [OTA 시나리오](../protocols/ota_scenario_detailed.md)
- [Zonal Gateway 아키텍처](zonal_gateway_architecture.md)
- [펌웨어 아키텍처](firmware_architecture.md)
- [빌드 가이드](../guides/build_guide.md)

## 🎓 핵심 개념

### Zonal Architecture
- **목적**: 복잡도 감소, 확장성 증대
- **이점**: Zone별 독립 관리, OTA 병렬 처리
- **Zone Controller**: Zonal Gateway (ZG)

### Dual Bank Bootloader
- **목적**: 안전한 OTA 업데이트
- **Region A**: Active Bank
- **Region B**: Inactive Bank
- **Rollback**: 자동 복구

### DoIP/UDS
- **DoIP**: Diagnostics over IP (ISO 13400)
- **UDS**: Unified Diagnostic Services (ISO 14229)
- **통합**: In-vehicle Ethernet

### PQC (Post-Quantum Cryptography)
- **목적**: 양자 컴퓨터 대비
- **ML-KEM**: Key Encapsulation
- **ML-DSA**: Digital Signature
- **Hybrid**: ECDSA 병행

## 💡 설계 원칙

1. **계층적 구조**: 3-Tier (Server-VMG-ZG-ECU)
2. **표준 준수**: ISO 13400, ISO 14229
3. **보안 우선**: PQC-TLS, 서명 검증
4. **안전성**: Dual Bank, Rollback
5. **확장성**: Zonal Architecture
6. **성능**: 병렬 처리, 비동기 통신

