# TLS Implementation Status

## 현재 상태 분석

### 문제: 두 가지 TLS 구현이 혼재됨

프로젝트에 **OpenSSL 기반**과 **mbedTLS 기반** 코드가 동시에 존재합니다.

## 구현 현황

### 1. OpenSSL 기반 (PQC용)

#### 위치
- `vehicle_gateway/common/pqc_config.h/c` - OpenSSL 사용
- `vehicle_gateway/src/pqc_tls_server.c` - OpenSSL 사용
- `vehicle_gateway/src/https_client.cpp` - OpenSSL 사용
- `vehicle_gateway/src/mqtt_client.cpp` - OpenSSL 사용

#### 특징
```cpp
#include <openssl/ssl.h>
#include <openssl/err.h>

SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
SSL_CTX_set_groups_list(ctx, "mlkem768");  // PQC support
```

**장점:**
- ✅ PQC 지원 (ML-KEM, ML-DSA)
- ✅ OpenSSL 3.2+에서 NIST PQC 표준 지원
- ✅ 외부 서버 통신에 적합

**단점:**
- ❌ 큰 메모리 사용량 (~500KB+)
- ❌ TC375 MCU에 부적합

### 2. mbedTLS 기반 (Standard TLS용)

#### 위치
- `vehicle_gateway/common/mbedtls_doip.h/c` - mbedTLS 사용
- `vehicle_gateway/src/doip_server_mbedtls.cpp` - mbedTLS 사용
- `tc375_simulator/src/doip_client_mbedtls.cpp` - mbedTLS 사용

#### 특징
```c
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>

mbedtls_ssl_config conf;
mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, 
                             MBEDTLS_SSL_TRANSPORT_STREAM,
                             MBEDTLS_SSL_PRESET_DEFAULT);
```

**장점:**
- ✅ 작은 메모리 사용량 (~50-100KB)
- ✅ TC375 MCU에 적합
- ✅ 임베디드 시스템 최적화

**단점:**
- ❌ PQC 미지원 (ML-KEM, ML-DSA 없음)
- ❌ 표준 TLS 1.2/1.3만 지원

## 권장 아키텍처

### 정답: 두 가지를 **용도별로** 사용

```
┌─────────────────────────────────────────────────────────────┐
│                   External Server                            │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      │ [OpenSSL with PQC]
                      │ • ML-KEM-768 + ECDSA-P256
                      │ • Large memory OK (MacBook)
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                     VMG (MacBook Air)                        │
│                                                              │
│  External Comms: OpenSSL + PQC                              │
│  Internal Comms: Plain DoIP (No TLS)                        │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      │ [Plain DoIP]
                      │ • No TLS (Physical isolation)
                      │ • Optional: mbedTLS (if needed)
                      │
┌─────────────────────▼───────────────────────────────────────┐
│               Zonal Gateway (TC375)                          │
│                                                              │
│  If TLS needed: mbedTLS (Standard TLS 1.3)                 │
│  Default: Plain DoIP                                        │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      │ [Plain DoIP]
                      │ • No TLS
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                End Node ECU (TC375)                          │
│                                                              │
│  Plain DoIP only                                            │
└──────────────────────────────────────────────────────────────┘
```

## 현재 코드 상태

### ✅ OpenSSL (PQC) - 구현 완료

**파일:**
```
vehicle_gateway/
├── common/
│   ├── pqc_config.h        ← OpenSSL + PQC 설정
│   └── pqc_config.c        ← ML-KEM/ML-DSA 구현
├── src/
│   ├── https_client.cpp    ← OpenSSL 사용 (External Server)
│   ├── mqtt_client.cpp     ← OpenSSL 사용 (External Broker)
│   └── pqc_tls_server.c    ← OpenSSL 예제 (참고용)
```

**사용처:**
- VMG → External Server (HTTPS)
- VMG → MQTT Broker
- **Platform**: MacBook Air (충분한 메모리)

### ✅ mbedTLS (Standard TLS) - 구현 완료

**파일:**
```
vehicle_gateway/
├── common/
│   ├── mbedtls_doip.h      ← mbedTLS DoIP 인터페이스
│   └── mbedtls_doip.c      ← mbedTLS DoIP 구현
├── src/
│   └── doip_server_mbedtls.cpp  ← mbedTLS DoIP 서버 (사용 안 함)

tc375_simulator/
└── src/
    └── doip_client_mbedtls.cpp  ← mbedTLS DoIP 클라이언트
```

**사용처 (현재는 사용 안 함):**
- TC375 시뮬레이터 (x86 Linux 테스트용)
- 필요시 TC375 MCU에서 TLS 사용 가능

### ✅ Plain DoIP - 현재 사용 중

**파일:**
```
vehicle_gateway/
├── src/
│   └── doip_server.cpp     ← Plain TCP/UDP (현재 사용)
└── example_vmg_doip_server.cpp
    └── config.enable_tls = false;  ← TLS 비활성화
```

**사용처:**
- VMG → Zonal Gateway (Port 13400)
- Zonal Gateway → ECU (Port 13400)
- **Reason**: 물리적 격리, 성능 요구사항

## 구현 상태 정리

### OpenSSL (PQC)

| 컴포넌트 | 파일 | 상태 | 사용 여부 |
|---------|------|------|----------|
| PQC 설정 | `pqc_config.h/c` | ✅ 완료 | ✅ 사용 중 |
| HTTPS 클라이언트 | `https_client.cpp` | ✅ 완료 | ✅ 사용 중 |
| MQTT 클라이언트 | `mqtt_client.cpp` | ✅ 완료 | ✅ 사용 중 |
| PQC TLS 서버 | `pqc_tls_server.c` | ✅ 완료 | ❌ 예제만 |

### mbedTLS (Standard TLS)

| 컴포넌트 | 파일 | 상태 | 사용 여부 |
|---------|------|------|----------|
| mbedTLS DoIP | `mbedtls_doip.h/c` | ✅ 완료 | ❌ 대기 |
| mbedTLS 서버 | `doip_server_mbedtls.cpp` | ✅ 완료 | ❌ 대기 |
| mbedTLS 클라이언트 | `tc375_simulator/doip_client_mbedtls.cpp` | ✅ 완료 | ✅ 시뮬레이터 |

### Plain DoIP

| 컴포넌트 | 파일 | 상태 | 사용 여부 |
|---------|------|------|----------|
| DoIP 서버 | `doip_server.cpp` | ✅ 완료 | ✅ 사용 중 |
| VMG 예제 | `example_vmg_doip_server.cpp` | ✅ 완료 | ✅ 사용 중 |

## 의사결정: 어떤 것을 사용할까?

### 시나리오 1: 현재 (권장) ✅

```cpp
// VMG → External Server: OpenSSL + PQC
// vehicle_gateway/src/https_client.cpp
#include <openssl/ssl.h>
const PQC_Config* config = &PQC_CONFIGS[1];  // ML-KEM-768

// VMG → ZG/ECU: Plain DoIP (No TLS)
// vehicle_gateway/example_vmg_doip_server.cpp
config.enable_tls = false;
```

**이유:**
- VMG는 MacBook (메모리 충분) → OpenSSL + PQC 가능
- In-vehicle은 물리적 격리 → TLS 불필요

### 시나리오 2: In-Vehicle TLS 필요 시

```cpp
// VMG → ZG: mbedTLS (Standard TLS)
// vehicle_gateway/src/doip_server_mbedtls.cpp 사용
mbedtls_doip_server server;
mbedtls_doip_server_init(&server, cert, key, ca, 13400);

// ZG/ECU: mbedTLS 포팅
// TC375에서 mbedtls_doip 사용
```

**이유:**
- mbedTLS는 메모리 효율적 (TC375 MCU 가능)
- PQC 불필요 (차량 내부)

### 시나리오 3: 모든 곳에 PQC (권장 안 함) ❌

```cpp
// 모든 곳에 OpenSSL + PQC 사용
```

**문제:**
- TC375 MCU 메모리 부족 (OpenSSL ~500KB)
- 성능 저하 (핸드셰이크 ~15ms)
- 불필요한 오버헤드

## 현재 설정 확인

### VMG External Communication (OpenSSL + PQC)

```cpp
// vehicle_gateway/src/vmg_gateway.cpp Line 22
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  1  // ML-KEM-768 + ECDSA-P256

// vehicle_gateway/src/https_client.cpp Line 138
const int PQC_CONFIG_ID = 1;
const PQC_Config* config = &PQC_CONFIGS[PQC_CONFIG_ID];

// OpenSSL 사용
SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
```

### VMG In-Vehicle Communication (Plain DoIP)

```cpp
// vehicle_gateway/example_vmg_doip_server.cpp Line 42
config.enable_tls = false;  // ← Plain TCP (mbedTLS도 사용 안 함)

// vehicle_gateway/src/doip_server.cpp
// Plain TCP socket (No TLS wrapper)
tcp_socket_ = socket(AF_INET, SOCK_STREAM, 0);
```

## mbedTLS 사용이 필요한 경우

### 언제 mbedTLS를 사용해야 하나?

1. **TC375 MCU에서 TLS 필요 시**
   - OpenSSL은 너무 무거움
   - mbedTLS는 임베디드 최적화

2. **표준 TLS로 충분한 경우**
   - PQC 불필요 (차량 내부)
   - 메모리 효율 중요

3. **시뮬레이션/테스트**
   - TC375 시뮬레이터에서 TLS 테스트
   - x86 Linux에서 mbedTLS 동작 확인

### mbedTLS 활성화 방법

#### 옵션 1: VMG DoIP 서버에서 mbedTLS 사용

```bash
# 빌드
cd vehicle_gateway
mkdir build && cd build
cmake -DUSE_MBEDTLS_DOIP=ON ..
make

# 실행
./doip_server_mbedtls \
    certs/server.crt \
    certs/server.key \
    certs/ca.crt \
    13400
```

#### 옵션 2: TC375에 mbedTLS 포팅

```c
// zonal_gateway/tc375/src/zonal_gateway_tls.c
#include "mbedtls_doip.h"

mbedtls_doip_server server;
mbedtls_doip_server_init(&server, 
    "certs/zg.crt", 
    "certs/zg.key", 
    "certs/ca.crt", 
    13400);
```

## 메모리 비교

| 구현 | 코드 크기 | RAM 사용 | TC375 가능 | PQC 지원 |
|-----|----------|---------|-----------|---------|
| OpenSSL | ~500KB | ~200KB | ❌ 불가 | ✅ 지원 |
| mbedTLS | ~50KB | ~50KB | ✅ 가능 | ❌ 미지원 |
| Plain DoIP | ~10KB | ~10KB | ✅ 가능 | ❌ 없음 |

**TC375 메모리:**
- ROM: 4 MB PFLASH
- RAM: 192 KB DSRAM

## 최종 권장 사항

### ✅ 현재 구성 유지 (권장)

```
External:  OpenSSL + PQC    (VMG → Server)
Internal:  Plain DoIP       (VMG → ZG → ECU)
Fallback:  mbedTLS ready    (필요시 사용 가능)
```

**이유:**
1. ✅ VMG는 MacBook → OpenSSL + PQC 가능
2. ✅ In-vehicle은 물리적 격리 → TLS 불필요
3. ✅ mbedTLS 코드는 준비됨 (필요시 활성화)
4. ✅ 최적의 성능/보안 균형

### 📝 변경 불필요

**mbedTLS 구현은 이미 완료되어 대기 중:**
- `mbedtls_doip.h/c` - 구현 완료
- `doip_server_mbedtls.cpp` - 구현 완료
- 필요시 즉시 사용 가능

**현재 사용 중인 것:**
- External: OpenSSL + PQC ✅
- Internal: Plain DoIP ✅

## 요약

### Q: mbedTLS 구현 안 된 건가요?
**A: 아닙니다. mbedTLS 구현은 완료되었습니다.**

### Q: 그럼 왜 사용 안 하나요?
**A: In-vehicle network는 TLS 자체가 불필요하기 때문입니다.**
- 물리적 격리
- 성능 요구사항 (<1ms)
- Plain DoIP로 충분

### Q: OpenSSL과 mbedTLS 중 무엇을 쓰나요?
**A: 용도별로 다릅니다:**
- **External (VMG ↔ Server)**: OpenSSL + PQC (양자 내성 필요)
- **Internal (VMG ↔ ZG ↔ ECU)**: Plain DoIP (TLS 불필요)
- **TC375 TLS 필요시**: mbedTLS (이미 준비됨)

### 코드 상태
- ✅ OpenSSL + PQC: 구현 완료, 사용 중
- ✅ mbedTLS: 구현 완료, 대기 중
- ✅ Plain DoIP: 구현 완료, 사용 중

**모든 구현이 완료되었으며, 현재 설정이 최적입니다!**

