# Zonal Gateway ↔ ECU Communication

## 현재 구현 상태

### ✅ DoIP만 구현됨 (맞습니다!)

Zonal Gateway와 ECU 간의 통신은 **DoIP over mbedTLS**만 구현되어 있습니다.

```
Zonal Gateway (TC375)
        |
        | [mbedTLS] DoIP over TLS 1.3
        | Port 13400 (TCP/UDP)
        | • TCP: Diagnostic communication
        | • UDP: Vehicle discovery
        |
    End Node ECU (TC375)
```

## 구현 세부사항

### Zonal Gateway 역할

**파일**: `zonal_gateway/tc375/include/zonal_gateway.h`

```c
typedef struct {
    /* ========== Server 역할 (Zone 내부 ECU 대상) ========== */
    int doip_server_tcp_socket;     /* ECU들의 DoIP 연결 */
    int doip_server_udp_socket;     /* Vehicle Discovery */
    int json_server_socket;         /* JSON 메시지 (optional) */
    
    /* Zone 내 ECU 관리 */
    ZoneVCIData_t zone_vci;
    UDSHandler_t uds_handler;
    
    /* ========== Client 역할 (VMG 대상) ========== */
    DoIPClient_t vmg_client;        /* VMG에 연결 */
    bool vmg_connected;
    
} ZonalGateway_t;
```

**특징:**
- ✅ DoIP Server (Zone 내 ECU들에게)
- ✅ DoIP Client (VMG에게)
- ✅ Port 13400 사용
- ✅ mbedTLS 적용 가능

### End Node ECU

**파일**: `end_node_ecu/tc375/include/ecu_node.h`

```c
typedef struct {
    /* DoIP Client (Zonal Gateway에 연결) */
    DoIPClient_t doip_client;       /* ZG 연결용 */
    uint16_t logical_address;       /* DoIP 논리 주소 */
    
    /* 통신 버퍼 */
    uint8_t rx_buffer[4096];
    uint8_t tx_buffer[4096];
    
} ECUNode_t;
```

**특징:**
- ✅ DoIP Client only
- ✅ Zonal Gateway에만 연결
- ✅ Port 13400 사용
- ✅ mbedTLS 적용 가능

## mbedTLS 적용 상태

### Zonal Gateway → ECU (DoIP over mbedTLS)

```c
// Zonal Gateway Server (ECU들을 위한)
#include "mbedtls_doip.h"

mbedtls_doip_server zg_server;
mbedtls_doip_server_init(&zg_server, 
    "certs/zg_server.crt",
    "certs/zg_server.key",
    "certs/ca.crt",
    13400);  // Zone 내부 DoIP 포트
```

### ECU → Zonal Gateway (DoIP over mbedTLS)

```c
// ECU Client (Zonal Gateway에 연결)
#include "doip_client_mbedtls.h"

mbedtls_doip_client* ecu_client;
doip_client_mbedtls_init(&ecu_client,
    "192.168.1.10",              // Zonal Gateway IP
    13400,                        // DoIP port
    "certs/ecu_client.crt",      // ECU cert
    "certs/ecu_client.key",      // ECU key
    "certs/ca.crt");             // CA cert
```

## CAN 통신은 별개입니다! (정확함)

### CAN vs DoIP 비교

| Feature | DoIP (Ethernet) | CAN/CAN-FD |
|---------|----------------|------------|
| **매체** | Ethernet (TCP/IP) | CAN bus (serial) |
| **속도** | 100Mbps+ | 1Mbps (CAN), 5Mbps (CAN-FD) |
| **용도** | 진단, OTA, VCI | 실시간 제어, 센서 데이터 |
| **계층** | ISO 13400 (DoIP) | ISO 11898 (CAN) |
| **암호화** | TLS 가능 | SecOC (MAC only) |
| **구현** | ✅ 완료 (mbedTLS) | ❌ 별도 구현 필요 |

### CAN 통신 구현 시 (별개 작업)

```
Zonal Gateway (TC375)
        |
        +-- [DoIP/Ethernet] ← 현재 구현됨
        |   Port 13400
        |   ECU #1, #2, #3...
        |
        +-- [CAN Bus] ← 별도 구현 필요
            CAN ID: 0x100~0x7FF
            ECU #4, #5, #6...
```

**CAN 구현 시 필요한 것:**
1. TriCore CAN 드라이버 (MultiCAN 모듈)
2. CAN 메시지 프레임 정의
3. CAN 버스 초기화 (속도, 필터 등)
4. DBC 파일 (메시지 정의)
5. (Optional) SecOC (보안)

## 네트워크 토폴로지

### 현재 구현 (DoIP only)

```
┌──────────────────────────────────────────────────────────┐
│                       VMG (MacBook)                       │
│                    Port 13400 (Server)                    │
└─────────────────────────┬────────────────────────────────┘
                          │
                          │ [mbedTLS DoIP]
                          │
        ┌─────────────────┴──────────────────┐
        │                                    │
┌───────▼──────────┐              ┌─────────▼────────┐
│  Zonal Gateway 1 │              │ Zonal Gateway 2  │
│    (TC375 #1)    │              │    (TC375 #3)    │
│  Port 13400      │              │  Port 13400      │
│  (Server + Client)│              │ (Server + Client)│
└───────┬──────────┘              └─────────┬────────┘
        │                                   │
        │ [mbedTLS DoIP]                   │ [mbedTLS DoIP]
        │                                   │
  ┌─────┴──────┐                      ┌────┴──────┐
  │            │                      │           │
┌─▼──┐   ┌────▼─┐                ┌───▼┐    ┌────▼─┐
│ECU2│   │ ECU4 │                │ECU6│    │ ECU8 │
│TC375   │TC375 │                │TC375    │TC375 │
└────┘   └──────┘                └────┘    └──────┘

[현재 구현: DoIP over Ethernet only]
```

### 만약 CAN 추가 시 (미래 확장)

```
┌──────────────────────────────────────────────────────────┐
│                       VMG (MacBook)                       │
│                    Port 13400 (Server)                    │
└─────────────────────────┬────────────────────────────────┘
                          │
                          │ [mbedTLS DoIP]
                          │
┌─────────────────────────▼──────────────────────────────┐
│               Zonal Gateway (TC375)                     │
│                                                         │
│  [DoIP Server] Port 13400                              │
│  [CAN Controller] MultiCAN Module  ← 별도 구현 필요      │
└──────────┬──────────────────────┬─────────────────────┘
           │                      │
           │ [DoIP/Ethernet]     │ [CAN Bus]
           │                      │
    ┌──────▼──────┐        ┌─────▼──────┬────────┐
    │             │        │            │        │
┌───▼──┐   ┌─────▼─┐   ┌──▼─┐    ┌────▼─┐  ┌──▼──┐
│ECU #2│   │ECU #4 │   │ECU │    │ECU #6│  │ECU  │
│DoIP  │   │DoIP   │   │#5  │    │CAN   │  │#7   │
│Client│   │Client │   │CAN │    │      │  │CAN  │
└──────┘   └───────┘   └────┘    └──────┘  └─────┘
```

## 통신 프로토콜 상세

### DoIP Communication (현재 구현)

#### 1. Vehicle Discovery (UDP)
```c
// ECU → Zonal Gateway (UDP broadcast)
DoIP_VehicleIdentificationReq

// Zonal Gateway → ECU (UDP unicast)
DoIP_VehicleIdentificationRes {
    VIN: "WBA...",
    LogicalAddress: 0x0200,
    EID: [MAC address],
    GID: [Group ID]
}
```

#### 2. Routing Activation (TCP)
```c
// ECU → Zonal Gateway
DoIP_RoutingActivationReq {
    SourceAddress: 0x0E80,  // ECU address
    ActivationType: 0x00     // Default
}

// Zonal Gateway → ECU
DoIP_RoutingActivationRes {
    SourceAddress: 0x0200,   // ZG address
    Result: 0x10             // Success
}
```

#### 3. Diagnostic Message (TCP)
```c
// ECU → Zonal Gateway (UDS Request)
DoIP_DiagnosticMessage {
    SourceAddress: 0x0E80,
    TargetAddress: 0x0200,
    UDS_Data: [0x22, 0xF1, 0x90]  // Read DID 0xF190
}

// Zonal Gateway → ECU (UDS Response)
DoIP_DiagnosticMessage {
    SourceAddress: 0x0200,
    TargetAddress: 0x0E80,
    UDS_Data: [0x62, 0xF1, 0x90, ...]  // Response
}
```

### CAN Communication (구현 안 됨)

만약 CAN을 추가한다면:

```c
// CAN 메시지 구조 (예시)
typedef struct {
    uint32_t can_id;        // 0x100~0x7FF
    uint8_t dlc;            // Data length (0-8)
    uint8_t data[8];        // Payload
    bool is_extended;       // 29-bit ID 사용 여부
} CAN_Message_t;

// CAN 송신 (Zonal Gateway → ECU)
can_transmit(0x100, [0x01, 0x02, 0x03]);

// CAN 수신 (ECU → Zonal Gateway)
can_receive(&msg);
```

**별도 작업 필요:**
- MultiCAN 초기화
- CAN 버스 설정 (500kbps / 1Mbps)
- CAN 메시지 프레임 정의
- Interrupt 처리

## 설정 파일

### Zonal Gateway Configuration

```c
// zonal_gateway.h
#define ZG_MAX_ECUS             8       /* Zone당 최대 ECU 수 */
#define ZG_DOIP_SERVER_PORT     13400   /* DoIP 포트 */
#define ZG_JSON_SERVER_PORT     8765    /* JSON 포트 (optional) */
```

**특징:**
- ✅ DoIP 설정만 존재
- ❌ CAN 설정 없음 (구현 안 됨)

### ECU Configuration

```c
// ecu_node.h
#define ECU_DOIP_TARGET_PORT    13400   /* Zonal Gateway DoIP 포트 */
#define ECU_LOGICAL_ADDRESS     0x0E80  /* ECU DoIP 주소 */
```

**특징:**
- ✅ DoIP 클라이언트 설정만
- ❌ CAN 설정 없음

## 요약

### ✅ 현재 구현 (DoIP only)

| 구간 | 프로토콜 | 포트 | 암호화 | 상태 |
|------|---------|------|--------|------|
| VMG ↔ ZG | DoIP/TCP | 13400 | mbedTLS | ✅ 완료 |
| ZG ↔ ECU | DoIP/TCP | 13400 | mbedTLS | ✅ 완료 |
| ZG → ECU | DoIP/UDP | 13400 | None | ✅ 완료 (Discovery) |

### ❌ CAN 통신 (구현 안 됨)

| 구간 | 프로토콜 | 상태 | 이유 |
|------|---------|------|------|
| ZG ↔ ECU | CAN/CAN-FD | ❌ 없음 | 별도 구현 필요 |

**CAN은 완전히 별개의 통신 방식입니다:**
- ✅ DoIP: Ethernet 기반 (진단, OTA, VCI)
- ❌ CAN: Serial bus (실시간 제어, 센서)
- 두 가지는 동시에 존재 가능하지만, 현재는 DoIP만 구현됨

### 정리

1. **Zonal Gateway ↔ ECU**: DoIP over mbedTLS only ✅
2. **CAN 통신**: 구현 안 됨 (별도 작업) ❌
3. **설정**: DoIP 설정만 존재 ✅
4. **확장 가능**: CAN 추가 가능하지만 별개 구현 필요 ✅

**정확히 파악하셨습니다!** 현재는 DoIP만 구현되어 있고, CAN은 완전히 별개의 이야기입니다.

