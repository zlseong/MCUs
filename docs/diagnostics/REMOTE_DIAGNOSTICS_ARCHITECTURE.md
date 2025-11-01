# Remote Diagnostics Architecture

## 원격 진단 시스템 구조

```
┌─────────────────────────────────────────────────────────────┐
│                      OTA Server                             │
│  - REST API: POST /api/v1/diagnostics/send                  │
│  - MQTT Publish: vehicle/{vin}/diagnostics/request          │
└──────────────────────┬──────────────────────────────────────┘
                       │ MQTT (PQC-TLS)
                       │
┌──────────────────────▼──────────────────────────────────────┐
│                        VMG                                   │
│  - MQTT Subscribe: vehicle/{vin}/diagnostics/request        │
│  - DoIP Client: Forward to Zonal Gateway                    │
│  - MQTT Publish: vehicle/{vin}/diagnostics/response         │
└──────────────────────┬──────────────────────────────────────┘
                       │ DoIP (mbedTLS)
                       │
┌──────────────────────▼──────────────────────────────────────┐
│                   Zonal Gateway                              │
│  - DoIP Server: Receive from VMG                            │
│  - Route to target ECU (or broadcast)                       │
│  - Aggregate responses                                       │
│  - DoIP Client: Send response to VMG                        │
└──────────────────────┬──────────────────────────────────────┘
                       │ DoIP (mbedTLS)
                       │
┌──────────────────────▼──────────────────────────────────────┐
│                      ECU                                     │
│  - DoIP Client: Receive diagnostic request                  │
│  - UDS Handler: Process service (0x22, 0x19, etc)          │
│  - DoIP Client: Send response                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 메시지 흐름

### 1. 진단 요청 (Server → ECU)

```
[Admin/Server]
    │
    ├─ POST /api/v1/diagnostics/send
    │  Body: {
    │    "vin": "KMHGH4JH1NU123456",
    │    "ecu_id": "ECU_001",
    │    "service_id": "0x22",
    │    "data": "F190"  // Read VIN
    │  }
    │
    ↓
[OTA Server]
    │
    ├─ MQTT Publish
    │  Topic: vehicle/KMHGH4JH1NU123456/diagnostics/request
    │  Payload: {
    │    "request_id": "diag-12345",
    │    "ecu_id": "ECU_001",
    │    "service_id": "0x22",
    │    "data": "F190"
    │  }
    │
    ↓
[VMG]
    │
    ├─ MQTT Subscribe 수신
    ├─ DoIP 메시지 생성
    │  [0x8001] Diagnostic Message
    │  Source: 0x0E00 (Tester)
    │  Target: 0x0100 (ECU_001)
    │  UDS: [0x22, 0xF1, 0x90]
    │
    ├─ Zonal Gateway 찾기
    │  ECU_001 → ZG_POWERTRAIN
    │
    ├─ DoIP Send to ZG
    │
    ↓
[Zonal Gateway]
    │
    ├─ DoIP 메시지 수신
    ├─ Target ECU 확인 (0x0100 = ECU_001)
    ├─ DoIP Forward to ECU_001
    │
    ↓
[ECU_001]
    │
    ├─ DoIP 메시지 수신
    ├─ UDS 처리
    │  Service: 0x22 (Read Data By ID)
    │  DID: 0xF190 (VIN)
    │  Response: [0x62, 0xF1, 0x90, "KMHGH4JH1NU123456"]
    │
    ├─ DoIP Response to ZG
    │
    ↓
[Zonal Gateway]
    │
    ├─ DoIP 응답 수신
    ├─ DoIP Forward to VMG
    │
    ↓
[VMG]
    │
    ├─ DoIP 응답 수신
    ├─ MQTT Publish
    │  Topic: vehicle/KMHGH4JH1NU123456/diagnostics/response
    │  Payload: {
    │    "request_id": "diag-12345",
    │    "ecu_id": "ECU_001",
    │    "success": true,
    │    "response_data": "62F190KMHGH4JH1NU123456"
    │  }
    │
    ↓
[OTA Server]
    │
    ├─ MQTT Subscribe 수신
    ├─ 데이터베이스 저장
    ├─ Admin에게 응답
    │
    ↓
[Admin]
    │
    └─ GET /api/v1/diagnostics/results/{request_id}
       Response: {
         "request_id": "diag-12345",
         "status": "completed",
         "response_data": "62F190KMHGH4JH1NU123456",
         "decoded": {
           "service": "ReadDataByIdentifier",
           "did": "0xF190",
           "value": "KMHGH4JH1NU123456"
         }
       }
```

---

## 2. 브로드캐스트 진단 (Zone 전체)

```
[Admin] → [Server]
    POST /api/v1/diagnostics/broadcast
    Body: {
      "vin": "KMHGH4JH1NU123456",
      "zone_id": "ZG_POWERTRAIN",
      "service_id": "0x19",
      "data": "02FF"  // Read DTC
    }
    ↓
[Server] → [VMG]
    MQTT: vehicle/{vin}/diagnostics/broadcast
    ↓
[VMG] → [ZG]
    DoIP: Broadcast to all ECUs in zone
    ↓
[ZG] → [All ECUs in zone]
    DoIP: Forward to ECU_001, ECU_002, ECU_003...
    ↓
[ECUs] → [ZG]
    DoIP: Individual responses
    ↓
[ZG] → [VMG]
    DoIP: Aggregated response
    {
      "ecu_001": "59020001...",
      "ecu_002": "59020002...",
      "ecu_003": "7F1922"  // Error
    }
    ↓
[VMG] → [Server]
    MQTT: Aggregated results
    ↓
[Server] → [Admin]
    REST API: Results for all ECUs
```

---

## 구현 파일

### 1. Server (Python)
- `server/diagnostics_server.py` - ✅ 이미 구현됨
- `server/https_server.py` - ✅ API 엔드포인트 있음

### 2. VMG (C++)
- `vehicle_gateway/src/remote_diagnostics_handler.cpp` - ❌ 구현 필요
- `vehicle_gateway/include/remote_diagnostics_handler.hpp` - ❌ 구현 필요

### 3. Zonal Gateway (C)
- `zonal_gateway/tc375/src/diagnostic_router.c` - ❌ 구현 필요
- `zonal_gateway/tc375/include/diagnostic_router.h` - ❌ 구현 필요

### 4. ECU (C)
- `end_node_ecu/tc375/src/uds_handler.c` - ✅ 이미 구현됨

---

## 지원 UDS 서비스

### 읽기 서비스
- **0x22**: Read Data By Identifier
  - VIN, Serial Number, Software Version 등
- **0x19**: Read DTC Information
  - DTC 목록, 상태 등

### 쓰기 서비스
- **0x2E**: Write Data By Identifier
  - 설정 값 변경

### 제어 서비스
- **0x31**: Routine Control
  - 자가 진단, 캘리브레이션 등
- **0x11**: ECU Reset
  - 원격 재부팅

### 세션 관리
- **0x10**: Diagnostic Session Control
  - Extended Session 진입
- **0x27**: Security Access
  - 보안 레벨 상승
- **0x3E**: Tester Present
  - 세션 유지

---

## 타임아웃 및 재시도

```
Request Timeout: 5초
Retry Count: 3회
Total Timeout: 15초

[Server] → [VMG]: 1초 이내
[VMG] → [ZG]: 500ms 이내
[ZG] → [ECU]: 500ms 이내
[ECU] Processing: 1-3초
[Response Path]: 역순 2초 이내
```

---

## 에러 처리

### 1. ECU 응답 없음
```json
{
  "request_id": "diag-12345",
  "status": "timeout",
  "error": "ECU_001 did not respond within 5 seconds"
}
```

### 2. UDS Negative Response
```json
{
  "request_id": "diag-12345",
  "status": "negative_response",
  "nrc": "0x33",
  "nrc_description": "Security Access Denied"
}
```

### 3. 네트워크 오류
```json
{
  "request_id": "diag-12345",
  "status": "network_error",
  "error": "VMG connection lost"
}
```

---

## 보안

### 1. 인증
- Server ↔ VMG: PQC-TLS + mTLS
- VMG ↔ ZG: mbedTLS + mTLS
- ZG ↔ ECU: mbedTLS + mTLS

### 2. 권한 관리
- Level 1: 읽기 전용 (0x22, 0x19)
- Level 2: 쓰기 가능 (0x2E)
- Level 3: 제어 가능 (0x31, 0x11)

### 3. 감사 로그
- 모든 진단 요청/응답 기록
- 타임스탬프, 사용자, ECU, 서비스 ID

---

## 다음 단계

1. ✅ Server: 이미 구현됨
2. ❌ VMG: Remote Diagnostics Handler 구현 필요
3. ❌ ZG: Diagnostic Router 구현 필요
4. ✅ ECU: UDS Handler 이미 구현됨

