# Zonal Gateway Architecture

계층적 차량 네트워크 아키텍처: CCU (VMG) → Zonal Gateway → ECU

## 📊 전체 구조

```
                    ┌──────────────────────┐
                    │   Cloud Server       │
                    │   (OTA Management)   │
                    └──────────┬───────────┘
                               │ MQTT/HTTPS (TCP)
                               │ 포트 8883/443
                    ┌──────────▼───────────┐
                    │   VMG (CCU)          │
                    │   Central Gateway    │
                    │   192.168.1.1        │
                    │                      │
                    │ - DoIP Server:13400  │
                    │ - JSON Server:8765   │
                    │ - 여러 Zone 통합     │
                    └──┬────────┬────────┬─┘
                       │        │        │
          ┌────────────┘        │        └────────────┐
          │ DoIP/JSON           │ DoIP/JSON           │ DoIP/JSON
          │ (In-Vehicle)        │ (In-Vehicle)        │ (In-Vehicle)
          │                     │                     │
┌─────────▼──────────┐ ┌────────▼─────────┐ ┌────────▼─────────┐
│ MCU #1 (ZG)        │ │ MCU #3 (ZG)      │ │ MCU #5 (ZG)      │
│ Zonal Gateway      │ │ Zonal Gateway    │ │ Zonal Gateway    │
│ Zone 1: Powertrain │ │ Zone 2: Chassis  │ │ Zone 3: Body     │
│ 192.168.1.10       │ │ 192.168.1.20     │ │ 192.168.1.30     │
│                    │ │                  │ │                  │
│ ┌────────────────┐ │ │ ┌──────────────┐ │ │ ┌──────────────┐ │
│ │ DoIP Server    │ │ │ │ DoIP Server  │ │ │ │ DoIP Server  │ │
│ │ (Zone ECU용)   │ │ │ │ (Zone ECU용) │ │ │ │ (Zone ECU용) │ │
│ │ Port: 13400    │ │ │ │ Port: 13400  │ │ │ │ Port: 13400  │ │
│ └────────────────┘ │ │ └──────────────┘ │ │ └──────────────┘ │
│ ┌────────────────┐ │ │ ┌──────────────┐ │ │ ┌──────────────┐ │
│ │ DoIP Client    │ │ │ │ DoIP Client  │ │ │ │ DoIP Client  │ │
│ │ (VMG 연결용)   │ │ │ │ (VMG 연결용) │ │ │ │ (VMG 연결용) │ │
│ └────────────────┘ │ │ └──────────────┘ │ │ └──────────────┘ │
└─────┬──────┬───────┘ └─────┬──────┬─────┘ └─────┬──────┬─────┘
      │      │                │      │             │      │
      │      │ DoIP           │      │ DoIP        │      │ DoIP
      │      │ (Zone Net)     │      │ (Zone Net)  │      │ (Zone Net)
      │      │                │      │             │      │
┌─────▼──┐ ┌─▼────┐    ┌─────▼──┐ ┌─▼────┐  ┌─────▼──┐ ┌─▼────┐
│ MCU #2 │ │MCU #4│    │ MCU #6 │ │MCU #8│  │MCU #10 │ │MCU #12│
│ (ECU)  │ │(ECU) │    │ (ECU)  │ │(ECU) │  │ (ECU)  │ │(ECU)  │
│Engine  │ │Trans │    │ABS/ESC │ │Steer │  │BCM     │ │Door  │
│        │ │      │    │        │ │      │  │        │ │      │
│DoIP    │ │DoIP  │    │DoIP    │ │DoIP  │  │DoIP    │ │DoIP  │
│Client  │ │Client│    │Client  │ │Client│  │Client  │ │Client│
└────────┘ └──────┘    └────────┘ └──────┘  └────────┘ └──────┘
```

---

## 🎯 Zonal Gateway (MCU #1) 역할

### 이중 역할 (Dual Role)

#### 1. **Server 역할** (Downstream - Zone 내부)
```c
/* Zone 내 ECU들에게 서버 */
- DoIP Server (포트 13400, TCP/UDP)
- JSON Server (포트 8765, TCP)
- Vehicle Discovery (UDP Broadcast 응답)
- UDS 요청 처리
- Zone 내 ECU 관리
```

#### 2. **Client 역할** (Upstream - VMG 연결)
```c
/* VMG(CCU)에게 클라이언트 */
- DoIP Client (VMG 포트 13400 연결)
- Zone VCI 집계 및 전송
- Heartbeat 전송
- OTA 조율 및 중계
```

---

## 📡 통신 프로토콜

### Zonal Gateway ↔ Zone ECU (Downstream)

| 프로토콜 | 포트 | 방향 | 용도 |
|---------|------|------|------|
| **UDP** | 13400 | ECU → ZG (Broadcast) | Vehicle Discovery |
| **UDP** | 13400 | ZG → ECU (Unicast) | Discovery Response |
| **TCP (DoIP)** | 13400 | ECU ↔ ZG | 진단 통신 (UDS) |
| **TCP (JSON)** | 8765 | ECU → ZG | 제어 메시지 |

### Zonal Gateway ↔ VMG (Upstream)

| 프로토콜 | 포트 | 방향 | 용도 |
|---------|------|------|------|
| **TCP (DoIP)** | 13400 | ZG → VMG | Zone VCI, Status |
| **TCP (DoIP)** | 13400 | VMG → ZG | OTA Commands |
| **TCP (JSON)** | 8765 | ZG ↔ VMG | JSON 메시지 |

### VMG ↔ Server (Cloud)

| 프로토콜 | 포트 | 방향 | 용도 |
|---------|------|------|------|
| **MQTT/TCP** | 8883 | VMG ↔ Server | Wake up, VCI, Status |
| **HTTPS/TCP** | 443 | VMG ← Server | OTA 파일 다운로드 |

---

## 🔄 메시지 흐름 시나리오

### 1. 시동 및 Discovery

```
MCU #2 (ECU)        MCU #1 (ZG)         VMG (CCU)          Server
    │                   │                   │                  │
    │                   │                   │                  │
    │◄──System Boot─────┤                   │                  │
    │                   │                   │                  │
    │──UDP Broadcast───►│                   │                  │
    │ "ZG 찾기"          │                   │                  │
    │ 255.255.255:13400  │                   │                  │
    │                   │                   │                  │
    │◄──UDP Response────┤                   │                  │
    │ VIN: KMHGH...     │                   │                  │
    │ Address: 0x0201   │                   │                  │
    │ ZG IP: 192.168.1.10                   │                  │
    │                   │                   │                  │
    │──TCP Connect─────►│                   │                  │
    │ DoIP: 192.168.1.10:13400              │                  │
    │                   │                   │                  │
    │──Routing Act Req─►│                   │                  │
    │                   │                   │                  │
    │◄──Routing Act Res─┤                   │                  │
    │ (Success)         │                   │                  │
    │                   │                   │                  │
    │                   │──TCP Connect─────►│                  │
    │                   │ DoIP: 192.168.1.1:13400              │
    │                   │                   │                  │
    │                   │──Routing Act Req─►│                  │
    │                   │◄──Routing Act Res─┤                  │
    │                   │                   │                  │
    │                   │                   │──MQTT Connect───►│
    │                   │                   │ (Wake Up)         │
    │                   │                   │◄──ACK────────────┤
```

### 2. VCI 수집

```
MCU #2 (ECU)        MCU #1 (ZG)         VMG (CCU)          Server
    │                   │                   │                  │
    │                   │                   │◄──REQUEST_VCI────┤
    │                   │                   │ MQTT             │
    │                   │                   │                  │
    │                   │◄──Collect VCI─────┤                  │
    │                   │ "Zone 1 VCI 수집"  │                  │
    │                   │                   │                  │
    │◄──UDS: Read DID───┤                   │                  │
    │ 0x22 F190 (VIN)   │                   │                  │
    │                   │                   │                  │
    │──UDS: Res VIN────►│                   │                  │
    │ KMHGH4JH1NU123456 │                   │                  │
    │                   │                   │                  │
    │◄──UDS: Read DID───┤                   │                  │
    │ 0x22 F195 (SW Ver)│                   │                  │
    │                   │                   │                  │
    │──UDS: SW v1.0.0──►│                   │                  │
    │                   │                   │                  │
    │                   │[집계 중...]        │                  │
    │                   │                   │                  │
    │                   │──Zone 1 VCI──────►│                  │
    │                   │ {                 │                  │
    │                   │   "zone_id": 1,   │                  │
    │                   │   "ecus": [...]   │                  │
    │                   │ }                 │                  │
    │                   │                   │                  │
    │                   │                   │[전체 차량 VCI]    │
    │                   │                   │ = Zone1 + Zone2...│
    │                   │                   │                  │
    │                   │                   │──Vehicle VCI────►│
    │                   │                   │ {               │
    │                   │                   │   "vin": "...",  │
    │                   │                   │   "zones": [     │
    │                   │                   │     {zone1},     │
    │                   │                   │     {zone2}      │
    │                   │                   │   ]              │
    │                   │                   │ }                │
```

### 3. OTA 업데이트

```
Server              VMG (CCU)         MCU #1 (ZG)        MCU #2 (ECU)
    │                   │                   │                  │
    │──OTA Campaign────►│                   │                  │
    │ Zone 1: v1.1.0    │                   │                  │
    │                   │                   │                  │
    │                   │──Readiness Req───►│                  │
    │                   │ "Zone 1 준비?"     │                  │
    │                   │                   │                  │
    │                   │                   │──Check Ready────►│
    │                   │                   │ Battery? Park?   │
    │                   │                   │                  │
    │                   │                   │◄──Battery 85%────┤
    │                   │                   │◄──Parked─────────┤
    │                   │                   │                  │
    │                   │                   │[집계]             │
    │                   │◄──Zone Ready──────┤                  │
    │                   │ "Zone 1: Ready"   │                  │
    │                   │                   │                  │
    │◄──Vehicle Ready───┤                   │                  │
    │ All zones OK      │                   │                  │
    │                   │                   │                  │
    │   [HTTPS Download]│                   │                  │
    │──Firmware File───►│                   │                  │
    │ PKG-ENGINE-v1.1.0 │                   │                  │
    │                   │                   │                  │
    │                   │──UDS: Req DL─────►│                  │
    │                   │ 0x34 (펌웨어 시작) │                  │
    │                   │                   │                  │
    │                   │                   │──UDS: Req DL────►│
    │                   │                   │ 0x34             │
    │                   │                   │                  │
    │                   │──UDS: Transfer───►│                  │
    │                   │ 0x36 [1024 bytes] │──UDS: Transfer──►│
    │                   │                   │ 0x36 [data]      │
    │                   │                   │                  │
    │                   │                   │◄──Progress 10%───┤
    │                   │◄──Progress 10%────┤                  │
    │◄──Progress 10%────┤                   │                  │
    │                   │                   │                  │
    │                   │ ... (반복) ...    │                  │
    │                   │                   │                  │
    │                   │                   │◄──Success────────┤
    │                   │                   │ v1.1.0 Active    │
    │                   │◄──Zone Success────┤                  │
    │◄──Vehicle Success─┤                   │                  │
```

---

## 💾 Zone VCI 데이터 구조

### Zone Level (Zonal Gateway → VMG)

```json
{
  "message_type": "ZONE_VCI_REPORT",
  "message_id": "uuid",
  "timestamp": "2025-10-30T15:00:00.000Z",
  "source": {
    "entity": "ZG",
    "identifier": "ZG-001"
  },
  "target": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "zone_id": 1,
    "zone_name": "Powertrain",
    "zonal_gateway": {
      "zg_id": "ZG-001",
      "logical_address": "0x0201",
      "firmware_version": "2.0.0",
      "ip_address": "192.168.1.10"
    },
    "ecus": [
      {
        "ecu_id": "TC375-SIM-002-Zone1-ECU1",
        "ecu_type": "Engine_Controller",
        "logical_address": "0x0211",
        "firmware_version": "1.0.0",
        "hardware_version": "TC375TP-LiteKit-v2.0",
        "part_number": "39100-K0100",
        "is_online": true,
        "ota_capable": true,
        "delta_update_supported": true,
        "max_package_size_mb": 10
      },
      {
        "ecu_id": "TC375-SIM-003-Zone1-ECU2",
        "ecu_type": "Transmission_Controller",
        "logical_address": "0x0212",
        "firmware_version": "1.0.0",
        "hardware_version": "TC375TP-LiteKit-v2.0",
        "ota_capable": true
      }
    ],
    "zone_statistics": {
      "total_ecus": 2,
      "online_ecus": 2,
      "ota_capable_ecus": 2,
      "total_storage_mb": 512,
      "available_storage_mb": 256,
      "average_battery_level": 85
    }
  }
}
```

### Vehicle Level (VMG → Server)

```json
{
  "message_type": "VCI_REPORT",
  "payload": {
    "vin": "KMHGH4JH1NU123456",
    "vehicle_info": {
      "model": "Genesis G80 EV",
      "year": 2025
    },
    "zones": [
      {
        "zone_id": 1,
        "zone_name": "Powertrain",
        "zg_id": "ZG-001",
        "ecus": [...]
      },
      {
        "zone_id": 2,
        "zone_name": "Chassis",
        "zg_id": "ZG-002",
        "ecus": [...]
      },
      {
        "zone_id": 3,
        "zone_name": "Body",
        "zg_id": "ZG-003",
        "ecus": [...]
      }
    ],
    "vehicle_statistics": {
      "total_zones": 3,
      "total_ecus": 15,
      "online_ecus": 14,
      "total_storage_mb": 2048,
      "available_storage_mb": 1024
    }
  }
}
```

---

## 🛠️ 구현 체크리스트

### Zonal Gateway (MCU #1)

#### Server 기능 (Zone 내부)
- [ ] DoIP Server TCP 소켓 (포트 13400)
- [ ] DoIP Server UDP 소켓 (포트 13400)
- [ ] JSON Server 소켓 (포트 8765)
- [ ] Vehicle Discovery 처리 (UDP Broadcast 응답)
- [ ] Zone ECU 연결 관리
- [ ] Zone ECU 정보 수집
- [ ] UDS 요청 처리 (진단 서비스)

#### Client 기능 (VMG 연결)
- [ ] DoIP Client 구현
- [ ] VMG 연결 및 Routing Activation
- [ ] Zone VCI 집계
- [ ] Zone VCI 전송 (VMG로)
- [ ] Heartbeat 전송
- [ ] Zone Status 전송
- [ ] OTA 준비 상태 확인
- [ ] OTA 진행률 보고

#### 데이터 관리
- [ ] Zone ECU 목록 관리
- [ ] ECU 상태 추적 (online/offline)
- [ ] VCI 데이터 캐싱
- [ ] Zone 통계 계산

### VMG (CCU)

#### Zone 통합 관리
- [ ] 여러 Zonal Gateway 동시 연결
- [ ] Zone별 VCI 수집
- [ ] 전체 차량 VCI 통합
- [ ] Zone별 OTA 조율
- [ ] Zone 상태 모니터링

#### Server 통신
- [ ] Vehicle VCI 전송 (MQTT)
- [ ] OTA 파일 다운로드 (HTTPS)
- [ ] 각 Zone으로 OTA 분배
- [ ] 전체 진행률 집계

---

## 📚 참고 파일

- `tc375_bootloader/common/zonal_gateway.h` - Zonal Gateway 헤더
- `tc375_bootloader/example_zonal_gateway.c` - 사용 예제
- `docs/unified_message_format.md` - 통합 메시지 포맷
- `docs/ISO_13400_specification.md` - DoIP 표준

---

**버전:** 1.0  
**최종 수정:** 2025-10-30

