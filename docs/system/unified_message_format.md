# VMG 통합 메시지 포맷 (Unified Message Format)

OTA 프로젝트와 현재 VMG 구현을 통합한 표준 메시지 포맷입니다.

**참고:**
- 기존 프로토콜: `docs/protocol.md`, `docs/data_management.md`
- OTA 프로젝트: https://github.com/zlseong/OTA-Project-with-PQC-hybrid-TLS.git

---

## 📋 공통 메시지 구조

### 기본 구조 (Base Message Format)

```json
{
  "message_type": "MESSAGE_TYPE",
  "message_id": "uuid-v4",
  "correlation_id": "uuid-v4",
  "timestamp": "2025-10-30T15:30:00.000Z",
  "source": {
    "entity": "VMG|SERVER|ECU",
    "identifier": "device_id or server_id"
  },
  "target": {
    "entity": "VMG|SERVER|ECU",
    "identifier": "device_id or server_id"
  },
  "payload": {},
  "metadata": {
    "protocol_version": "1.0",
    "encryption": "ML-KEM-768",
    "signature": "ML-DSA-65"
  }
}
```

### 필드 설명

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `message_type` | string | ✅ | 메시지 타입 (대문자 스네이크 케이스) |
| `message_id` | string(uuid) | ✅ | 메시지 고유 ID (UUID v4) |
| `correlation_id` | string(uuid) | ⚪ | 요청-응답 추적 ID (응답 시 필수) |
| `timestamp` | string(ISO8601) | ✅ | UTC 타임스탬프 (ISO 8601 형식) |
| `source` | object | ✅ | 발신자 정보 |
| `target` | object | ⚪ | 수신자 정보 (명시적 지정 시) |
| `payload` | object | ✅ | 메시지 페이로드 (내용) |
| `metadata` | object | ⚪ | 확장 메타데이터 |

---

## 🚗 1. ECU ↔ VMG 메시지 (로컬)

### 1.1 디바이스 등록 (DEVICE_REGISTRATION)

**방향:** ECU → VMG (최초 연결 시 1회)

```json
{
  "message_type": "DEVICE_REGISTRATION",
  "message_id": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp": "2025-10-30T15:30:00.000Z",
  "source": {
    "entity": "ECU",
    "identifier": "TC375-SIM-001-20251030"
  },
  "target": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "device_info": {
      // Flash 저장 (불변)
      "ecu_serial": "TC375-SIM-001-20251030",
      "mac_address": "02:00:00:AA:BB:CC",
      "hardware_version": "TC375TP-LiteKit-v2.0",
      "vin": "KMHGH4JH1NU123456",
      "vehicle_model": "Genesis G80 EV",
      "vehicle_year": 2025,
      
      // EEPROM 저장 (설정)
      "ip_address": "192.168.1.100",
      "gateway_host": "192.168.1.1",
      "gateway_port": 8765,
      "doip_port": 13400,
      "tls_enabled": true,
      "ota_enabled": true,
      
      // Flash 저장 (펌웨어)
      "firmware_version": "1.0.0",
      "bootloader_version": "1.0.0",
      "application_version": "1.0.0"
    },
    "capabilities": {
      "max_package_size": 104857600,
      "supported_protocols": ["DoIP", "UDS", "CAN"],
      "compression_supported": true,
      "delta_update_supported": true
    }
  },
  "metadata": {
    "protocol_version": "1.0",
    "encryption": "ML-KEM-768"
  }
}
```

**VMG 응답:**

```json
{
  "message_type": "DEVICE_REGISTRATION_ACK",
  "message_id": "650e8400-e29b-41d4-a716-446655440001",
  "correlation_id": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp": "2025-10-30T15:30:00.500Z",
  "source": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "target": {
    "entity": "ECU",
    "identifier": "TC375-SIM-001-20251030"
  },
  "payload": {
    "status": "success",
    "assigned_logical_address": "0x0100",
    "server_time": "2025-10-30T15:30:00.500Z",
    "sync_interval": 10,
    "heartbeat_timeout": 30
  }
}
```

---

### 1.2 Heartbeat (HEARTBEAT)

**방향:** ECU → VMG (10초마다)

```json
{
  "message_type": "HEARTBEAT",
  "message_id": "750e8400-e29b-41d4-a716-446655440002",
  "timestamp": "2025-10-30T15:30:10.000Z",
  "source": {
    "entity": "ECU",
    "identifier": "TC375-SIM-001-20251030"
  },
  "payload": {
    "status": "alive",
    "uptime": 3600,
    "last_activity": "2025-10-30T15:30:00.000Z"
  }
}
```

---

### 1.3 센서 데이터 (SENSOR_DATA)

**방향:** ECU → VMG (5초마다 또는 이벤트 기반)

```json
{
  "message_type": "SENSOR_DATA",
  "message_id": "850e8400-e29b-41d4-a716-446655440003",
  "timestamp": "2025-10-30T15:30:15.000Z",
  "source": {
    "entity": "ECU",
    "identifier": "TC375-SIM-001-20251030"
  },
  "payload": {
    "sensors": {
      "temperature": 25.5,
      "pressure": 101.3,
      "voltage": 12.0,
      "rpm": 2000,
      "speed": 80,
      "battery_soc": 85.5
    }
  }
}
```

---

### 1.4 상태 리포트 (STATUS_REPORT)

**방향:** ECU → VMG (1분마다 또는 요청 시)

```json
{
  "message_type": "STATUS_REPORT",
  "message_id": "950e8400-e29b-41d4-a716-446655440004",
  "timestamp": "2025-10-30T15:31:00.000Z",
  "source": {
    "entity": "ECU",
    "identifier": "TC375-SIM-001-20251030"
  },
  "payload": {
    "system": {
      "uptime": 3660,
      "cpu_usage": 45.2,
      "memory_free": 2048,
      "temperature": 55.0
    },
    "network": {
      "gateway_connected": true,
      "signal_strength": -65,
      "ip_address": "192.168.1.100"
    },
    "firmware": {
      "active_bank": "A",
      "firmware_version": "1.0.0",
      "last_update": "2025-10-25T14:00:00Z"
    },
    "diagnostics": {
      "dtc_count": 2,
      "dtc_codes": ["P0420", "P0300"],
      "error_flags": 0x0000
    }
  }
}
```

---

## 🌐 2. VMG ↔ Server 메시지 (MQTT/HTTPS)

### 2.1 Wake Up Message

**방향:** VMG → Server (차량 시동 시)  
**프로토콜:** MQTT  
**Topic:** `v2x/vmg/{vin}/wakeup`

```json
{
  "message_type": "WAKEUP",
  "message_id": "a50e8400-e29b-41d4-a716-446655440005",
  "timestamp": "2025-10-30T15:00:00.000Z",
  "source": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "vin": "KMHGH4JH1NU123456",
    "vehicle_model": "Genesis G80 EV",
    "vehicle_year": 2025,
    "wakeup_reason": "ignition_on",
    "location": {
      "latitude": 37.5665,
      "longitude": 126.9780,
      "country": "KR"
    },
    "network": {
      "type": "LTE",
      "signal_strength": -75,
      "ip_address": "203.0.113.100"
    },
    "current_versions": {
      "vmg_version": "2.0.0",
      "ecu_versions": [
        {
          "ecu_id": "TC375-SIM-001-20251030",
          "firmware_version": "1.0.0",
          "hardware_version": "TC375TP-LiteKit-v2.0"
        }
      ]
    }
  },
  "metadata": {
    "protocol_version": "1.0",
    "encryption": "ML-KEM-768",
    "priority": "high"
  }
}
```

**Server 응답:**

```json
{
  "message_type": "WAKEUP_ACK",
  "message_id": "b50e8400-e29b-41d4-a716-446655440006",
  "correlation_id": "a50e8400-e29b-41d4-a716-446655440005",
  "timestamp": "2025-10-30T15:00:01.000Z",
  "source": {
    "entity": "SERVER",
    "identifier": "OTA-SERVER-001"
  },
  "target": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "status": "acknowledged",
    "server_time": "2025-10-30T15:00:01.000Z",
    "pending_actions": [
      "vci_request",
      "ota_check"
    ],
    "next_poll_interval": 300
  }
}
```

---

### 2.2 Request VCI (Vehicle Configuration Information)

**방향:** Server → VMG  
**프로토콜:** MQTT  
**Topic:** `v2x/vmg/{vin}/command`

```json
{
  "message_type": "REQUEST_VCI",
  "message_id": "c50e8400-e29b-41d4-a716-446655440007",
  "timestamp": "2025-10-30T15:00:05.000Z",
  "source": {
    "entity": "SERVER",
    "identifier": "OTA-SERVER-001"
  },
  "target": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "request_type": "full",
    "include_sections": [
      "hardware",
      "software",
      "configuration",
      "diagnostics"
    ]
  }
}
```

---

### 2.3 Send VCI

**방향:** VMG → Server  
**프로토콜:** MQTT  
**Topic:** `v2x/vmg/{vin}/vci`

```json
{
  "message_type": "VCI_REPORT",
  "message_id": "d50e8400-e29b-41d4-a716-446655440008",
  "correlation_id": "c50e8400-e29b-41d4-a716-446655440007",
  "timestamp": "2025-10-30T15:00:10.000Z",
  "source": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "target": {
    "entity": "SERVER",
    "identifier": "OTA-SERVER-001"
  },
  "payload": {
    "vin": "KMHGH4JH1NU123456",
    "vehicle_info": {
      "model": "Genesis G80 EV",
      "year": 2025,
      "manufacturing_date": "2024-12-01",
      "region": "KR",
      "calibration_region": "Asia_Pacific"
    },
    "ecus": [
      {
        "ecu_id": "TC375-SIM-001-20251030",
        "ecu_type": "Engine_Controller",
        "hardware_version": "TC375TP-LiteKit-v2.0",
        "firmware_version": "1.0.0",
        "bootloader_version": "1.0.0",
        "part_number": "39100-K0100",
        "supplier": "Infineon",
        "install_date": "2024-12-15",
        "capabilities": {
          "ota_capable": true,
          "delta_update": true,
          "max_package_size": 104857600
        }
      },
      {
        "ecu_id": "TC375-SIM-002-20251030",
        "ecu_type": "Transmission_Controller",
        "hardware_version": "TC375TP-LiteKit-v2.0",
        "firmware_version": "1.0.0",
        "bootloader_version": "1.0.0",
        "part_number": "39200-K0200",
        "supplier": "Infineon",
        "install_date": "2024-12-15"
      }
    ],
    "diagnostics": {
      "dtc_count": 2,
      "dtc_codes": ["P0420", "P0300"],
      "last_diagnostic_session": "2025-10-29T10:00:00Z"
    },
    "storage": {
      "total_capacity_mb": 512,
      "available_capacity_mb": 256,
      "ota_partition_size_mb": 100
    }
  }
}
```

---

### 2.4 Request Readiness to Download

**방향:** Server → VMG  
**프로토콜:** MQTT  
**Topic:** `v2x/vmg/{vin}/command`

```json
{
  "message_type": "REQUEST_READINESS",
  "message_id": "e50e8400-e29b-41d4-a716-446655440009",
  "timestamp": "2025-10-30T15:05:00.000Z",
  "source": {
    "entity": "SERVER",
    "identifier": "OTA-SERVER-001"
  },
  "target": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "campaign_id": "OTA-2025-001",
    "update_packages": [
      {
        "ecu_id": "TC375-SIM-001-20251030",
        "package_id": "PKG-ENGINE-v1.1.0",
        "package_type": "full",
        "from_version": "1.0.0",
        "to_version": "1.1.0",
        "package_size_bytes": 10485760,
        "checksum": "sha256:1a2b3c4d5e6f...",
        "download_url": "https://ota-cdn.example.com/packages/PKG-ENGINE-v1.1.0.bin",
        "priority": "normal",
        "estimated_install_time_sec": 600
      }
    ],
    "prerequisites": {
      "minimum_battery_level": 50,
      "required_storage_mb": 50,
      "vehicle_state": "parked",
      "ignition_state": "off_or_acc"
    },
    "scheduling": {
      "immediate": false,
      "user_consent_required": true,
      "deadline": "2025-11-30T23:59:59Z"
    }
  }
}
```

---

### 2.5 Send Ready Response

**방향:** VMG → Server  
**프로토콜:** MQTT  
**Topic:** `v2x/vmg/{vin}/readiness`

```json
{
  "message_type": "READINESS_RESPONSE",
  "message_id": "f50e8400-e29b-41d4-a716-446655440010",
  "correlation_id": "e50e8400-e29b-41d4-a716-446655440009",
  "timestamp": "2025-10-30T15:05:30.000Z",
  "source": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "target": {
    "entity": "SERVER",
    "identifier": "OTA-SERVER-001"
  },
  "payload": {
    "campaign_id": "OTA-2025-001",
    "readiness_status": "ready",
    "checks": {
      "battery_level": 85,
      "available_storage_mb": 256,
      "vehicle_state": "parked",
      "ignition_state": "acc",
      "network_quality": "excellent",
      "user_consent": true
    },
    "estimated_download_time_sec": 120,
    "scheduled_install_time": "2025-10-30T16:00:00Z",
    "notes": "All prerequisites met. Ready to download."
  }
}
```

**상태별 응답:**

```json
// 조건 불만족
{
  "readiness_status": "not_ready",
  "blocked_by": [
    {
      "check": "battery_level",
      "current_value": 30,
      "required_value": 50,
      "message": "Battery level too low"
    },
    {
      "check": "vehicle_state",
      "current_value": "driving",
      "required_value": "parked",
      "message": "Vehicle must be parked"
    }
  ],
  "retry_after_sec": 600
}
```

---

### 2.6 OTA Download Progress

**방향:** VMG → Server  
**프로토콜:** MQTT  
**Topic:** `v2x/vmg/{vin}/ota/progress`

```json
{
  "message_type": "OTA_DOWNLOAD_PROGRESS",
  "message_id": "050e8400-e29b-41d4-a716-446655440011",
  "timestamp": "2025-10-30T15:10:30.000Z",
  "source": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "campaign_id": "OTA-2025-001",
    "package_id": "PKG-ENGINE-v1.1.0",
    "status": "downloading",
    "progress_percentage": 45,
    "bytes_downloaded": 4718592,
    "total_bytes": 10485760,
    "download_speed_kbps": 2048,
    "estimated_time_remaining_sec": 120,
    "errors": []
  }
}
```

---

### 2.7 OTA Update Results

**방향:** VMG → Server  
**프로토콜:** MQTT  
**Topic:** `v2x/vmg/{vin}/ota/result`

```json
{
  "message_type": "OTA_UPDATE_RESULT",
  "message_id": "150e8400-e29b-41d4-a716-446655440012",
  "timestamp": "2025-10-30T15:20:00.000Z",
  "source": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "campaign_id": "OTA-2025-001",
    "overall_status": "success",
    "completion_time": "2025-10-30T15:20:00.000Z",
    "ecus": [
      {
        "ecu_id": "TC375-SIM-001-20251030",
        "package_id": "PKG-ENGINE-v1.1.0",
        "status": "success",
        "previous_version": "1.0.0",
        "current_version": "1.1.0",
        "install_start_time": "2025-10-30T15:15:00Z",
        "install_end_time": "2025-10-30T15:19:30Z",
        "verification_status": "passed",
        "rollback_performed": false
      }
    ],
    "diagnostics": {
      "post_update_dtc_count": 0,
      "dtc_codes": [],
      "system_health": "normal"
    },
    "logs": {
      "download_log": "https://vmg-logs.example.com/download-001.log",
      "install_log": "https://vmg-logs.example.com/install-001.log"
    }
  }
}
```

**실패 케이스:**

```json
{
  "overall_status": "failed",
  "ecus": [
    {
      "ecu_id": "TC375-SIM-001-20251030",
      "status": "failed",
      "error_code": "ERR_VERIFICATION_FAILED",
      "error_message": "Firmware signature verification failed",
      "rollback_performed": true,
      "current_version": "1.0.0"
    }
  ]
}
```

---

## ⚠️ 3. 에러 메시지 (ERROR)

**방향:** 양방향  
**프로토콜:** MQTT / 로컬 TCP

```json
{
  "message_type": "ERROR",
  "message_id": "250e8400-e29b-41d4-a716-446655440013",
  "correlation_id": "original-message-id",
  "timestamp": "2025-10-30T15:30:00.000Z",
  "source": {
    "entity": "VMG",
    "identifier": "VMG-001"
  },
  "payload": {
    "error_code": "ERR_CONNECTION_TIMEOUT",
    "error_category": "network",
    "severity": "critical",
    "message": "Connection to ECU timed out after 30 seconds",
    "details": {
      "target_ecu": "TC375-SIM-001-20251030",
      "last_response_time": "2025-10-30T15:29:30Z",
      "retry_count": 3
    },
    "recovery_action": "retry_with_backoff",
    "timestamp": "2025-10-30T15:30:00.000Z"
  }
}
```

### 표준 에러 코드

| 카테고리 | 에러 코드 | 설명 |
|---------|----------|------|
| **Network** | `ERR_CONNECTION_TIMEOUT` | 연결 타임아웃 |
| | `ERR_CONNECTION_REFUSED` | 연결 거부 |
| | `ERR_NETWORK_UNREACHABLE` | 네트워크 도달 불가 |
| **Security** | `ERR_AUTH_FAILED` | 인증 실패 |
| | `ERR_CERT_INVALID` | 인증서 무효 |
| | `ERR_SIGNATURE_MISMATCH` | 서명 불일치 |
| **OTA** | `ERR_DOWNLOAD_FAILED` | 다운로드 실패 |
| | `ERR_VERIFICATION_FAILED` | 검증 실패 |
| | `ERR_INSTALL_FAILED` | 설치 실패 |
| | `ERR_ROLLBACK_FAILED` | 롤백 실패 |
| **System** | `ERR_OUT_OF_MEMORY` | 메모리 부족 |
| | `ERR_STORAGE_FULL` | 저장공간 부족 |
| | `ERR_BATTERY_LOW` | 배터리 부족 |
| **Protocol** | `ERR_INVALID_MESSAGE` | 메시지 형식 오류 |
| | `ERR_UNSUPPORTED_VERSION` | 버전 미지원 |

---

## 🔐 4. 보안 및 인증

### 메시지 서명 (metadata.signature)

모든 메시지는 ML-DSA-65로 서명:

```json
{
  "metadata": {
    "protocol_version": "1.0",
    "encryption": "ML-KEM-768",
    "signature": {
      "algorithm": "ML-DSA-65",
      "value": "base64-encoded-signature",
      "certificate_thumbprint": "sha256:cert-hash"
    }
  }
}
```

### TLS 설정

```
VMG ↔ Server:
  - TLS 1.3
  - Key Exchange: ML-KEM-768 (PQC)
  - Signature: ML-DSA-65 (PQC) 또는 ECDSA P-256
  - mTLS (상호 인증)

ECU ↔ VMG:
  - TLS 1.3
  - Key Exchange: ML-KEM-768 (PQC)
  - Signature: ML-DSA-65 (PQC)
  - mTLS (상호 인증)
```

---

## 📊 5. 프로토콜 매핑

| 구간 | 제어 메시지 | 데이터 전송 | 파일 다운로드 |
|------|-----------|-----------|-------------|
| **ECU ↔ VMG** | TCP + JSON (포트 8765) | DoIP (포트 13400) | - |
| **VMG ↔ Server** | MQTT (포트 8883) | MQTT | HTTPS (포트 443) |

### MQTT Topic 구조

```
v2x/vmg/{vin}/wakeup         - Wake up message
v2x/vmg/{vin}/command         - Server → VMG 명령
v2x/vmg/{vin}/vci            - VCI 정보
v2x/vmg/{vin}/readiness      - 준비 상태
v2x/vmg/{vin}/ota/progress   - OTA 진행률
v2x/vmg/{vin}/ota/result     - OTA 결과
v2x/vmg/{vin}/telemetry      - 텔레메트리
v2x/vmg/{vin}/diagnostics    - 진단 정보
v2x/vmg/{vin}/error          - 에러 리포트
```

---

## 🔄 6. 메시지 시퀀스 다이어그램

### OTA 업데이트 전체 플로우

```
Server              VMG                 ECU
  │                  │                   │
  │◄─────WAKEUP──────┤                   │
  │                  │                   │
  │──REQUEST_VCI────►│                   │
  │                  │───STATUS_REPORT──►│
  │                  │◄──VCI_DATA────────┤
  │◄────VCI_REPORT───┤                   │
  │                  │                   │
  │─REQUEST_READINESS►                   │
  │◄─READINESS_RESP──┤                   │
  │                  │                   │
  │  (HTTPS Download)│                   │
  │──────────────────┼──────────────────►│
  │                  │──UDS_DOWNLOAD────►│
  │◄──PROGRESS───────┤◄──PROGRESS────────┤
  │                  │                   │
  │                  │──UDS_INSTALL─────►│
  │◄──RESULT─────────┤◄──RESULT──────────┤
  │                  │                   │
```

---

## 📝 7. 구현 예시

### C++ (VMG)

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class UnifiedMessageBuilder {
public:
    static json createWakeup(const std::string& vin) {
        return json{
            {"message_type", "WAKEUP"},
            {"message_id", generateUUID()},
            {"timestamp", getCurrentTimestampISO8601()},
            {"source", {
                {"entity", "VMG"},
                {"identifier", "VMG-001"}
            }},
            {"payload", {
                {"vin", vin},
                {"wakeup_reason", "ignition_on"}
            }}
        };
    }
    
    static json createVCIReport(const VehicleConfig& vci) {
        // ... VCI 생성 로직
    }
};
```

### Python (Server)

```python
from dataclasses import dataclass
from datetime import datetime
import uuid

@dataclass
class UnifiedMessage:
    message_type: str
    message_id: str = None
    correlation_id: str = None
    timestamp: str = None
    source: dict = None
    target: dict = None
    payload: dict = None
    metadata: dict = None
    
    def to_json(self):
        if not self.message_id:
            self.message_id = str(uuid.uuid4())
        if not self.timestamp:
            self.timestamp = datetime.utcnow().isoformat() + 'Z'
        return {
            'message_type': self.message_type,
            'message_id': self.message_id,
            'correlation_id': self.correlation_id,
            'timestamp': self.timestamp,
            'source': self.source,
            'target': self.target,
            'payload': self.payload,
            'metadata': self.metadata
        }
```

---

## ✅ 체크리스트

구현 시 확인 사항:

- [ ] 모든 메시지에 `message_id` (UUID v4) 포함
- [ ] 응답 메시지에 `correlation_id` 포함
- [ ] Timestamp는 ISO 8601 형식 (UTC)
- [ ] `source`와 `target` 명시
- [ ] PQC 서명 포함 (`metadata.signature`)
- [ ] 에러 메시지에 표준 에러 코드 사용
- [ ] MQTT QoS 1 이상 사용 (최소 1회 전달 보장)
- [ ] HTTPS 다운로드 시 체크섬 검증
- [ ] 민감한 정보는 payload 암호화
- [ ] 로그에 `correlation_id` 기록 (추적성)

---

## 📚 참고 문서

- [OTA 프로젝트 GitHub](https://github.com/zlseong/OTA-Project-with-PQC-hybrid-TLS.git)
- [기존 프로토콜](docs/protocol.md)
- [데이터 관리](docs/data_management.md)
- [DoIP 아키텍처](docs/doip_tls_architecture.md)
- [ISO 13400 (DoIP)](https://www.iso.org/standard/68519.html)
- [ISO 14229 (UDS)](https://www.iso.org/standard/72439.html)

---

**버전:** 1.0  
**최종 수정:** 2025-10-30  
**작성자:** VMG Development Team

