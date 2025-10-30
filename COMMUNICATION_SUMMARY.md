# Communication Summary

## 전체 통신 구성 요약

### ✅ 현재 구현된 것

```
External Server (Cloud)
        ↓
    [OpenSSL + PQC] ← ML-KEM-768 + ECDSA-P256
        ↓
    VMG (MacBook)
        ↓
    [mbedTLS TLS 1.3] ← RSA-2048 + AES-256
        ↓
Zonal Gateway (TC375)
        ↓
    [mbedTLS TLS 1.3] ← RSA-2048 + AES-256
        ↓
End Node ECU (TC375)
```

**모두 DoIP over Ethernet (Port 13400)**

## 구간별 상세

### 1. External Server ↔ VMG

**기술**: OpenSSL + PQC
- **프로토콜**: HTTPS / MQTT over PQC-TLS
- **KEM**: ML-KEM-768 (Quantum-safe)
- **Signature**: ECDSA-P256 (lightweight)
- **Cipher**: AES-256-GCM
- **Handshake**: ~15ms
- **용도**: OTA download, Telemetry upload
- **파일**: 
  - `vehicle_gateway/src/https_client.cpp`
  - `vehicle_gateway/src/mqtt_client.cpp`
  - `vehicle_gateway/common/pqc_config.h/c`

### 2. VMG ↔ Zonal Gateway

**기술**: mbedTLS (Standard TLS 1.3)
- **프로토콜**: DoIP over TLS 1.3
- **포트**: 13400 (TCP/UDP)
- **Key Exchange**: ECDHE (Ephemeral)
- **Signature**: RSA-2048 or ECDSA-P256
- **Cipher**: AES-256-GCM
- **Handshake**: ~5ms
- **용도**: 진단, VCI 수집, OTA 전달
- **파일**:
  - `vehicle_gateway/example_vmg_doip_server_mbedtls.cpp`
  - `vehicle_gateway/common/mbedtls_doip.c/h`

### 3. Zonal Gateway ↔ End Node ECU

**기술**: mbedTLS (Standard TLS 1.3)
- **프로토콜**: DoIP over TLS 1.3
- **포트**: 13400 (TCP/UDP)
- **Key Exchange**: ECDHE
- **Signature**: RSA-2048 or ECDSA-P256
- **Cipher**: AES-256-GCM
- **Handshake**: ~5ms
- **용도**: 진단, OTA 업데이트
- **파일**:
  - `zonal_gateway/tc375/include/zonal_gateway.h`
  - `zonal_gateway/tc375/src/doip_client_mbedtls.c`
  - `end_node_ecu/tc375/include/ecu_node.h`

### 4. ECU ↔ ECU

**기술**: None (현재 구현 없음)
- **프로토콜**: CAN/CAN-FD (실시간 제어용)
- **암호화**: 없음 (또는 SecOC MAC)
- **상태**: ❌ 구현 안 됨 (별도 작업 필요)

## DoIP vs CAN 비교

| Feature | DoIP (Ethernet) | CAN/CAN-FD |
|---------|----------------|------------|
| **현재 상태** | ✅ 구현 완료 | ❌ 구현 안 됨 |
| **매체** | Ethernet | CAN bus |
| **속도** | 100Mbps+ | 1Mbps / 5Mbps |
| **프로토콜** | ISO 13400 (DoIP) | ISO 11898 (CAN) |
| **암호화** | mbedTLS (TLS 1.3) | None (또는 SecOC) |
| **용도** | 진단, OTA, VCI | 실시간 제어, 센서 |
| **포트/ID** | TCP 13400 | CAN ID (0x100~) |
| **메시지 크기** | ~4KB (분할 가능) | 8 bytes (CAN), 64 bytes (CAN-FD) |
| **지연시간** | ~1-2ms | <1ms |

## 네트워크 계층

### Ethernet 네트워크 (DoIP) - 현재 구현 ✅

```
┌──────────────────────────────────────────────────┐
│ Application Layer                                 │
│   - UDS (ISO 14229)                              │
│   - JSON messages                                │
└───────────────────┬──────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────┐
│ Transport Layer                                  │
│   - DoIP (ISO 13400)                            │
│   - TLS 1.3 (mbedTLS)                           │
└───────────────────┬──────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────┐
│ Network Layer                                    │
│   - TCP (Port 13400)                            │
│   - UDP (Port 13400, Discovery)                 │
└───────────────────┬──────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────┐
│ Physical Layer                                   │
│   - Ethernet (100BASE-TX)                       │
└──────────────────────────────────────────────────┘
```

### CAN 네트워크 (별도) - 구현 안 됨 ❌

```
┌──────────────────────────────────────────────────┐
│ Application Layer                                 │
│   - 차량 제어 메시지                               │
│   - 센서 데이터                                    │
└───────────────────┬──────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────┐
│ Transport Layer                                  │
│   - ISO-TP (ISO 15765-2) for >8 bytes          │
└───────────────────┬──────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────┐
│ Data Link Layer                                  │
│   - CAN 2.0B / CAN-FD                           │
│   - Arbitration (CAN ID)                        │
└───────────────────┬──────────────────────────────┘
                    │
┌───────────────────▼──────────────────────────────┐
│ Physical Layer                                   │
│   - CAN bus (Twisted pair)                      │
└──────────────────────────────────────────────────┘
```

## 구현 체크리스트

### ✅ DoIP over Ethernet (완료)

- [x] VMG DoIP Server (mbedTLS)
- [x] Zonal Gateway DoIP Server (TC375용 헤더)
- [x] Zonal Gateway DoIP Client (VMG 연결용)
- [x] ECU DoIP Client (ZG 연결용)
- [x] Vehicle Discovery (UDP)
- [x] Routing Activation
- [x] Diagnostic Messages (UDS)
- [x] mTLS 인증
- [x] 인증서 생성 스크립트

### ❌ CAN/CAN-FD (구현 안 됨)

- [ ] TriCore MultiCAN 초기화
- [ ] CAN 송수신 함수
- [ ] CAN 메시지 프레임 정의
- [ ] DBC 파일 (메시지 정의)
- [ ] CAN 필터 설정
- [ ] ISO-TP (>8 bytes 메시지)
- [ ] SecOC (보안, optional)

## 사용 예제

### VMG → Zonal Gateway (DoIP)

```bash
# VMG 서버 시작
./vmg_doip_server \
    certs/vmg_server.crt \
    certs/vmg_server.key \
    certs/ca.crt \
    13400
```

### Zonal Gateway → VMG (DoIP Client)

```c
#include "doip_client_mbedtls.h"

mbedtls_doip_client* client;
doip_client_mbedtls_init(&client,
    "192.168.1.1",              // VMG IP
    13400,
    "certs/zg_client.crt",
    "certs/zg_client.key",
    "certs/ca.crt");

// Send DoIP message
unsigned char doip_msg[256];
doip_client_mbedtls_send(client, doip_msg, sizeof(doip_msg));
```

### Zonal Gateway → ECU (DoIP Server)

```c
// Zonal Gateway는 ECU들을 위한 DoIP 서버
mbedtls_doip_server zg_server;
mbedtls_doip_server_init(&zg_server,
    "certs/zg_server.crt",
    "certs/zg_server.key",
    "certs/ca.crt",
    13400);  // ECU들이 연결할 포트
```

### ECU → Zonal Gateway (DoIP Client)

```c
mbedtls_doip_client* ecu_client;
doip_client_mbedtls_init(&ecu_client,
    "192.168.1.10",             // Zonal Gateway IP
    13400,
    "certs/ecu_client.crt",
    "certs/ecu_client.key",
    "certs/ca.crt");

// Send diagnostic message
unsigned char uds_req[] = {0x22, 0xF1, 0x90};  // Read VIN
doip_client_mbedtls_send(ecu_client, uds_req, sizeof(uds_req));
```

## CAN 통신 구현 시 (미래 확장)

만약 CAN을 추가한다면:

```c
// CAN 초기화 (TC375)
#include <IfxMultican.h>

void can_init(void) {
    // MultiCAN 모듈 초기화
    IfxMultican_Can_initModuleConfig(&canConfig, &MODULE_CAN);
    IfxMultican_Can_initModule(&canModule, &canConfig);
    
    // CAN 노드 초기화 (500kbps)
    IfxMultican_Can_Node_initConfig(&canNodeConfig, &canModule);
    canNodeConfig.baudrate = 500000;  // 500kbps
    IfxMultican_Can_Node_init(&canNode, &canNodeConfig);
}

// CAN 송신
void can_send_message(uint32_t can_id, uint8_t* data, uint8_t len) {
    IfxMultican_Message msg;
    msg.id = can_id;
    msg.dataLength = len;
    memcpy(msg.data, data, len);
    IfxMultican_Can_MsgObj_sendMessage(&canNode, &msg);
}

// CAN 수신
void can_receive_handler(void) {
    IfxMultican_Message msg;
    if (IfxMultican_Can_MsgObj_readMessage(&canNode, &msg)) {
        // Process CAN message
        process_can_message(msg.id, msg.data, msg.dataLength);
    }
}
```

**하지만 이것은 DoIP와 완전히 별개입니다!**

## 문서 참조

- **DoIP 구현**: `docs/architecture/zg_ecu_communication.md`
- **mbedTLS 설정**: `MBEDTLS_CONFIGURATION_GUIDE.md`
- **PQC 설정**: `QUICK_PQC_CONFIG_GUIDE.md`
- **보안 요약**: `NETWORK_SECURITY_SUMMARY.md`

## 요약

### ✅ 현재 시스템

| 구간 | 프로토콜 | 기술 | 상태 |
|------|---------|------|------|
| Server ↔ VMG | HTTPS/MQTT | OpenSSL + PQC | ✅ |
| VMG ↔ ZG | DoIP | mbedTLS | ✅ |
| ZG ↔ ECU | DoIP | mbedTLS | ✅ |

**모두 Ethernet 기반 (DoIP)**

### ❌ CAN은 별개

- CAN/CAN-FD는 구현되지 않음
- DoIP와 완전히 다른 통신 방식
- 필요시 별도로 구현해야 함
- 동시 사용 가능 (Ethernet + CAN)

**정확히 이해하셨습니다!** 🎯
- ✅ DoIP만 구현됨
- ✅ mbedTLS 적용됨
- ❌ CAN은 별개 (구현 안 됨)

