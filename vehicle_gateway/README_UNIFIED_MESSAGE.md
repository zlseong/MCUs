# Unified Message Format - 통합 가이드

OTA 프로젝트(https://github.com/zlseong/OTA-Project-with-PQC-hybrid-TLS.git)의 메시지 포맷을 현재 VMG 구현에 통합했습니다.

## 📊 분석 결과

### ✅ OTA 프로젝트 강점
1. **MQTT + HTTPS 분리** - 제어 메시지와 대용량 파일 전송 구분
2. **correlation_id** - 요청-응답 추적
3. **PQC 보안** - ML-KEM 768 적용
4. **차량 중심 설계** - VCI 기반 업데이트 관리
5. **지역별 Calibration** - 지역별 패키지 선택

### ⚠️ 개선 사항 (적용 완료)
1. **메시지 구조 통일** - `message_type`, `message_id`, `timestamp` 표준화
2. **에러 핸들링 체계화** - 표준 에러 코드 및 카테고리
3. **확장성 강화** - `metadata` 필드 추가
4. **타임스탬프 표준화** - ISO 8601 형식으로 통일
5. **식별자 명확화** - `source`/`target` 엔티티 구조

---

## 🏗️ 통합 구조

```
┌─────────────────────────────────────────────┐
│ ECU (TC375)                                 │
│ - DoIP/UDS (바이너리)                        │
│ - 로컬 통신 (포트 13400)                     │
└─────────────────┬───────────────────────────┘
                  │ 프로토콜 변환
┌─────────────────▼───────────────────────────┐
│ VMG (Gateway)                               │
│ - Unified Message Format (JSON)             │
│ - 로컬: TCP+JSON (포트 8765)                 │
│ - 클라우드: MQTT/HTTPS                       │
└─────────────────┬───────────────────────────┘
                  │ MQTT (제어) + HTTPS (파일)
┌─────────────────▼───────────────────────────┐
│ Server (Cloud/Backend)                      │
│ - OTA 캠페인 관리                            │
│ - VCI 기반 업데이트 결정                     │
│ - 결과 수집 및 모니터링                      │
└─────────────────────────────────────────────┘
```

---

## 📋 메시지 타입

### ECU ↔ VMG (로컬)
| 메시지 타입 | 방향 | 주기 | 설명 |
|-----------|------|------|------|
| `DEVICE_REGISTRATION` | ECU → VMG | 1회 | 최초 연결 시 디바이스 등록 |
| `DEVICE_REGISTRATION_ACK` | VMG → ECU | 1회 | 등록 확인 응답 |
| `HEARTBEAT` | ECU → VMG | 10초 | Keep-alive |
| `SENSOR_DATA` | ECU → VMG | 5초 | 센서 데이터 전송 |
| `STATUS_REPORT` | ECU → VMG | 1분 | 시스템 상태 리포트 |
| `COMMAND_ACK` | ECU → VMG | 이벤트 | 명령 실행 결과 |
| `ERROR` | 양방향 | 이벤트 | 에러 발생 시 |

### VMG ↔ Server (클라우드)
| 메시지 타입 | 방향 | 프로토콜 | 설명 |
|-----------|------|---------|------|
| `WAKEUP` | VMG → Server | MQTT | 차량 시동 시 웨이크업 |
| `WAKEUP_ACK` | Server → VMG | MQTT | 웨이크업 확인 |
| `REQUEST_VCI` | Server → VMG | MQTT | VCI 정보 요청 |
| `VCI_REPORT` | VMG → Server | MQTT | VCI 정보 전송 |
| `REQUEST_READINESS` | Server → VMG | MQTT | 다운로드 준비 확인 |
| `READINESS_RESPONSE` | VMG → Server | MQTT | 준비 상태 응답 |
| `OTA_DOWNLOAD_PROGRESS` | VMG → Server | MQTT | OTA 다운로드 진행률 |
| `OTA_UPDATE_RESULT` | VMG → Server | MQTT | OTA 업데이트 결과 |

---

## 💻 사용 예제

### 빌드

```bash
cd vehicle_gateway

# nlohmann_json 설치 (Ubuntu/Debian)
sudo apt-get install nlohmann-json3-dev

# 또는 vcpkg
vcpkg install nlohmann-json

# CMake 빌드
mkdir build && cd build
cmake -f ../CMakeLists_unified_message.txt ..
make

# 실행
./unified_message_example
```

### C++ 코드 예시

#### 1. Heartbeat 전송 (ECU → VMG)

```cpp
#include "unified_message.hpp"
using namespace vmg;

// Heartbeat 생성
auto heartbeat = MessageBuilder::createHeartbeat("TC375-SIM-001");

// JSON 직렬화
std::string json_str = heartbeat.toString();

// TCP 소켓으로 전송
send(socket_fd, json_str.c_str(), json_str.length(), 0);
```

#### 2. Wakeup 전송 (VMG → Server)

```cpp
json vehicle_info = {
    {"vehicle_model", "Genesis G80 EV"},
    {"vehicle_year", 2025},
    {"current_versions", {
        {"vmg_version", "2.0.0"},
        {"ecu_versions", json::array({
            {{"ecu_id", "TC375-SIM-001"}, {"firmware_version", "1.0.0"}}
        })}
    }}
};

auto wakeup = MessageBuilder::createWakeup("KMHGH4JH1NU123456", vehicle_info);

// MQTT 발행
mqtt_publish(mqtt_client, "v2x/vmg/KMHGH4JH1NU123456/wakeup", 
             wakeup.toString().c_str(), wakeup.toString().length(), 1);
```

#### 3. VCI Report 전송 (VMG → Server)

```cpp
json vci_data = {
    {"vin", "KMHGH4JH1NU123456"},
    {"vehicle_info", {
        {"model", "Genesis G80 EV"},
        {"year", 2025},
        {"region", "KR"}
    }},
    {"ecus", json::array({
        {
            {"ecu_id", "TC375-SIM-001"},
            {"firmware_version", "1.0.0"},
            {"capabilities", {
                {"ota_capable", true},
                {"delta_update", true}
            }}
        }
    })}
};

auto vci_report = MessageBuilder::createVCIReport(
    request_msg.getMessageId(),  // correlation_id
    vci_data
);

mqtt_publish(mqtt_client, "v2x/vmg/KMHGH4JH1NU123456/vci",
             vci_report.toString().c_str(), vci_report.toString().length(), 1);
```

#### 4. Readiness Response (VMG → Server)

```cpp
// 준비 완료
json checks_ready = {
    {"battery_level", 85},
    {"available_storage_mb", 256},
    {"vehicle_state", "parked"},
    {"user_consent", true}
};

auto readiness = MessageBuilder::createReadinessResponse(
    request_id,
    "OTA-2025-001",
    "ready",
    checks_ready
);

// 준비 불가
json checks_not_ready = {
    {"battery_level", 30},
    {"blocked_by", json::array({
        {
            {"check", "battery_level"},
            {"current_value", 30},
            {"required_value", 50}
        }
    })}
};

auto readiness = MessageBuilder::createReadinessResponse(
    request_id,
    "OTA-2025-001",
    "not_ready",
    checks_not_ready
);
```

#### 5. OTA Progress (VMG → Server)

```cpp
auto progress = MessageBuilder::createOTAProgress(
    "OTA-2025-001",         // campaign_id
    "PKG-ENGINE-v1.1.0",    // package_id
    45,                     // 45%
    4718592,                // ~4.5MB downloaded
    10485760                // 10MB total
);

mqtt_publish(mqtt_client, "v2x/vmg/KMHGH4JH1NU123456/ota/progress",
             progress.toString().c_str(), progress.toString().length(), 1);
```

#### 6. OTA Result (VMG → Server)

```cpp
// 성공
json ecus_success = json::array({
    {
        {"ecu_id", "TC375-SIM-001"},
        {"status", "success"},
        {"previous_version", "1.0.0"},
        {"current_version", "1.1.0"},
        {"verification_status", "passed"}
    }
});

auto result = MessageBuilder::createOTAResult(
    "OTA-2025-001",
    "success",
    ecus_success
);

// 실패
json ecus_failed = json::array({
    {
        {"ecu_id", "TC375-SIM-001"},
        {"status", "failed"},
        {"error_code", "ERR_VERIFICATION_FAILED"},
        {"rollback_performed", true}
    }
});

auto result = MessageBuilder::createOTAResult(
    "OTA-2025-001",
    "failed",
    ecus_failed
);
```

#### 7. Error Message

```cpp
json error_details = {
    {"target_ecu", "TC375-SIM-001"},
    {"retry_count", 3}
};

auto error = MessageBuilder::createError(
    original_request_id,
    "ERR_CONNECTION_TIMEOUT",
    "Connection to ECU timed out",
    error_details
);
```

---

## 🔄 OTA 업데이트 시퀀스

```
Server              VMG                 ECU
  │                  │                   │
  │                  │◄────WAKEUP────────┤ (시동 시)
  │◄─────WAKEUP──────┤                   │
  │──────ACK────────►│                   │
  │                  │                   │
  │──REQUEST_VCI────►│                   │
  │                  │───STATUS_REQ─────►│
  │                  │◄──STATUS_RES──────┤
  │◄────VCI_REPORT───┤                   │
  │                  │                   │
  │─REQUEST_READINESS►                   │
  │                  │───CHECK_COND─────►│
  │◄─READINESS_RESP──┤◄──COND_OK─────────┤
  │                  │                   │
  │  (HTTPS Download)│                   │
  │──────────────────┤                   │
  │                  │──UDS_DOWNLOAD────►│
  │◄──PROGRESS───────┤◄──PROGRESS────────┤
  │◄──PROGRESS───────┤◄──PROGRESS────────┤
  │                  │                   │
  │                  │──UDS_INSTALL─────►│
  │                  │◄──INSTALLING──────┤
  │◄──RESULT─────────┤◄──SUCCESS─────────┤
  │                  │                   │
```

---

## 📊 메시지 포맷 비교

### 기존 (현재 VMG)

```json
{
  "type": "HEARTBEAT",
  "device_id": "tc375-sim-001",
  "payload": {"status": "alive"},
  "timestamp": "2025-10-21 15:30:00"
}
```

### 통합 (Unified)

```json
{
  "message_type": "HEARTBEAT",
  "message_id": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp": "2025-10-30T15:30:00.000Z",
  "source": {
    "entity": "ECU",
    "identifier": "TC375-SIM-001-20251030"
  },
  "payload": {
    "status": "alive",
    "uptime": 3600
  },
  "metadata": {
    "protocol_version": "1.0",
    "encryption": "ML-KEM-768"
  }
}
```

**개선 사항:**
- ✅ UUID 기반 `message_id` 추가 (추적성)
- ✅ ISO 8601 타임스탬프 (표준 준수)
- ✅ `source`/`target` 명확화
- ✅ `metadata` 확장성
- ✅ `correlation_id` 지원

---

## 🔐 보안

### PQC-TLS 설정

```cpp
// VMG → Server (MQTT over TLS)
const PQC_Config* config = &PQC_CONFIGS[4]; // ML-KEM-768 + ML-DSA-65
MQTT_Client* mqtt = mqtt_client_create(
    "mqtts://ota-broker.example.com:8883",
    config,
    "certs/mlkem768_mldsa65_client.crt",
    "certs/mlkem768_mldsa65_client.key",
    "certs/ca.crt"
);
```

### 메시지 서명

```cpp
// metadata에 서명 추가
MessageMetadata metadata;
metadata.encryption = "ML-KEM-768";
metadata.signature = {
    {"algorithm", "ML-DSA-65"},
    {"value", "base64-encoded-signature"},
    {"certificate_thumbprint", "sha256:cert-hash"}
};

message.setMetadata(metadata);
```

---

## 🧪 테스트

```bash
# 예제 실행
./unified_message_example

# 출력:
# === ECU → VMG: Device Registration ===
# {
#   "message_type": "DEVICE_REGISTRATION",
#   "message_id": "550e8400-...",
#   ...
# }
```

---

## 📚 참고 문서

- **통합 명세**: `docs/unified_message_format.md`
- **OTA 프로젝트**: https://github.com/zlseong/OTA-Project-with-PQC-hybrid-TLS.git
- **기존 프로토콜**: `docs/protocol.md`
- **데이터 관리**: `docs/data_management.md`
- **DoIP**: `docs/doip_tls_architecture.md`

---

## ✅ 체크리스트

통합 시 확인사항:

- [ ] `message_id` UUID v4 생성
- [ ] `timestamp` ISO 8601 형식
- [ ] `correlation_id` 요청-응답 매칭
- [ ] `source`/`target` 명시
- [ ] PQC 서명 추가
- [ ] 표준 에러 코드 사용
- [ ] MQTT QoS 1 이상
- [ ] HTTPS 다운로드 체크섬 검증
- [ ] 로그에 `correlation_id` 포함

---

## 🎯 결론

### ✅ 통합 완료
- OTA 프로젝트 메시지 포맷 분석 완료
- 현재 VMG 구현과 호환되도록 통합
- 확장성 및 추적성 향상
- PQC 보안 적용
- 표준화된 에러 핸들링

### 🚀 다음 단계
1. VMG 메인 코드에 통합
2. MQTT 클라이언트 구현
3. HTTPS 다운로드 구현
4. OTA 캠페인 관리
5. 실제 서버 연동 테스트

**라이선스:** MIT

