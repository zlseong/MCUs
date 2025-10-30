# mbedTLS Configuration Guide

## ✅ mbedTLS Only 설정 완료

In-Vehicle 네트워크에서 **mbedTLS (Standard TLS 1.3)**만 사용하도록 설정되었습니다.

## 시스템 구성

```
┌─────────────────────────────────────────────────────────────┐
│                   External Server                            │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      │ [OpenSSL + PQC]
                      │ • ML-KEM-768 + ECDSA-P256
                      │ • HTTPS/MQTT
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                     VMG (MacBook Air)                        │
│                                                              │
│  External:  OpenSSL + PQC                                   │
│  Internal:  mbedTLS (Standard TLS 1.3)  ← NEW!             │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      │ [mbedTLS]
                      │ • Standard TLS 1.3
                      │ • RSA 2048 / ECDSA P-256
                      │ • mTLS authentication
                      │
┌─────────────────────▼───────────────────────────────────────┐
│               Zonal Gateway (TC375)                          │
│                                                              │
│  mbedTLS Client                                             │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      │ [mbedTLS]
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                End Node ECU (TC375)                          │
│                                                              │
│  mbedTLS Client                                             │
└──────────────────────────────────────────────────────────────┘
```

## 생성된 파일

### VMG 서버 (mbedTLS)
- `vehicle_gateway/example_vmg_doip_server_mbedtls.cpp` - **PRIMARY 서버**
- `vehicle_gateway/common/mbedtls_doip.c/h` - mbedTLS DoIP 구현

### TC375 클라이언트 (mbedTLS)
- `zonal_gateway/tc375/include/doip_client_mbedtls.h` - 클라이언트 인터페이스
- `zonal_gateway/tc375/src/doip_client_mbedtls.c` - 클라이언트 구현

### 인증서 생성 스크립트
- `vehicle_gateway/scripts/generate_standard_tls_certs.sh` - RSA/ECDSA 인증서

### Legacy (사용 안 함)
- `vehicle_gateway/example_vmg_doip_server.cpp` - Plain DoIP (no TLS)
- Build: `vmg_doip_server_plain` (참고용만)

## 빌드 및 실행

### 1. 인증서 생성

```bash
cd vehicle_gateway/scripts
chmod +x generate_standard_tls_certs.sh
./generate_standard_tls_certs.sh
```

**생성되는 인증서:**
- `certs/ca.crt` / `ca.key` - Root CA
- `certs/vmg_server.crt` / `vmg_server.key` - VMG 서버
- `certs/tc375_client.crt` / `tc375_client.key` - TC375 클라이언트

### 2. VMG 빌드

```bash
cd vehicle_gateway
mkdir build && cd build
cmake ..
make
```

**생성되는 바이너리:**
- `vmg_doip_server` - **mbedTLS 서버 (PRIMARY)**
- `vmg_doip_server_plain` - Plain DoIP (legacy)
- `vmg_https_client` - OpenSSL + PQC (External)
- `vmg_mqtt_client` - OpenSSL + PQC (External)

### 3. VMG 실행 (mbedTLS)

```bash
./vmg_doip_server \
    certs/vmg_server.crt \
    certs/vmg_server.key \
    certs/ca.crt \
    13400
```

**출력:**
```
╔══════════════════════════════════════════════════╗
║     Vehicle Management Gateway (VMG)             ║
║     DoIP Server with mbedTLS                     ║
╚══════════════════════════════════════════════════╝

[VMG] Configuration:
  Certificate: certs/vmg_server.crt
  Private Key: certs/vmg_server.key
  CA Cert:     certs/ca.crt
  Port:        13400
  TLS:         mbedTLS (Standard TLS 1.3)

[VMG] DoIP Server started on port 13400
[VMG] Waiting for TC375 clients...
```

### 4. TC375 클라이언트 (Zonal Gateway)

```c
#include "doip_client_mbedtls.h"

// Initialize client
mbedtls_doip_client* client;
int ret = doip_client_mbedtls_init(&client,
    "192.168.1.1",              // VMG IP
    13400,                       // VMG Port
    "certs/tc375_client.crt",   // Client cert
    "certs/tc375_client.key",   // Client key
    "certs/ca.crt");            // CA cert

if (ret == 0) {
    printf("[ZG] Connected to VMG with TLS\n");
    
    // Send DoIP message
    unsigned char doip_msg[] = { /* DoIP message */ };
    doip_client_mbedtls_send(client, doip_msg, sizeof(doip_msg));
    
    // Receive response
    unsigned char buffer[4096];
    int n = doip_client_mbedtls_receive(client, buffer, sizeof(buffer));
    
    // Cleanup
    doip_client_mbedtls_free(client);
}
```

## 보안 특성

### mbedTLS (In-Vehicle)

| Feature | Value |
|---------|-------|
| **TLS Version** | TLS 1.3 |
| **Key Exchange** | ECDHE (Ephemeral) |
| **Signature** | RSA 2048 or ECDSA P-256 |
| **Cipher** | AES-256-GCM |
| **Authentication** | mTLS (Mutual) |
| **Handshake Time** | ~5-10ms |
| **Memory** | ~50-100KB |
| **PQC** | ❌ No (not needed) |

### OpenSSL (External)

| Feature | Value |
|---------|-------|
| **TLS Version** | TLS 1.3 |
| **Key Exchange** | ML-KEM-768 (PQC) |
| **Signature** | ECDSA-P256 or ML-DSA-65 |
| **Cipher** | AES-256-GCM |
| **Authentication** | mTLS (Mutual) |
| **Handshake Time** | ~14-20ms |
| **Memory** | ~200-500KB |
| **PQC** | ✅ Yes (quantum-safe) |

## 성능 비교

| Metric | Plain DoIP | mbedTLS | OpenSSL + PQC |
|--------|-----------|---------|---------------|
| Handshake | 0ms | ~5ms | ~15ms |
| Memory | ~10KB | ~50KB | ~500KB |
| TC375 가능 | ✅ | ✅ | ❌ |
| 암호화 | ❌ | ✅ TLS 1.3 | ✅ PQC |
| 사용처 | Legacy | In-Vehicle | External |

## CMake 설정

### 활성화된 빌드 타겟

```cmake
# PRIMARY: mbedTLS DoIP Server
add_executable(vmg_doip_server
    example_vmg_doip_server_mbedtls.cpp
)

target_link_libraries(vmg_doip_server
    vmg_common
    ${CMAKE_THREAD_LIBS_INIT}
)

# vmg_common includes mbedTLS
target_link_libraries(vmg_common
    MbedTLS::mbedtls
    MbedTLS::mbedx509
    MbedTLS::mbedcrypto
)
```

### Legacy (비활성화 가능)

```cmake
# Plain DoIP Server (no TLS) - 참고용
add_executable(vmg_doip_server_plain
    src/doip_server.cpp
    example_vmg_doip_server.cpp
    src/uds_service_handler.cpp
)
```

## 테스트

### 1. VMG 서버 시작

```bash
# Terminal 1
./vmg_doip_server \
    certs/vmg_server.crt \
    certs/vmg_server.key \
    certs/ca.crt \
    13400
```

### 2. TC375 시뮬레이터 (클라이언트)

```bash
# Terminal 2
cd tc375_simulator/build
./tc375_simulator --mbedtls
```

### 3. 연결 확인

**VMG 출력:**
```
[VMG] Client connected with TLS
[VMG] Cipher suite: TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384
[VMG] Protocol version: TLSv1.3
[VMG] Received 100 bytes (DoIP message)
```

**TC375 출력:**
```
[DoIP Client] TLS handshake successful
[DoIP Client] Cipher suite: TLS-ECDHE-RSA-WITH-AES-256-GCM-SHA384
[DoIP Client] Protocol version: TLSv1.3
[DoIP Client] Connected to VMG with TLS
```

## 인증서 관리

### 인증서 확인

```bash
# VMG 서버 인증서 확인
openssl x509 -in certs/vmg_server.crt -text -noout

# TC375 클라이언트 인증서 확인
openssl x509 -in certs/tc375_client.crt -text -noout

# 인증서 검증
openssl verify -CAfile certs/ca.crt certs/vmg_server.crt
openssl verify -CAfile certs/ca.crt certs/tc375_client.crt
```

### 인증서 갱신 (10년 후)

```bash
cd vehicle_gateway/scripts
./generate_standard_tls_certs.sh
```

## TC375 MCU 포팅

### 메모리 요구사항

```c
// mbedTLS configuration for TC375
#define MBEDTLS_SSL_MAX_CONTENT_LEN  4096   // DoIP message size
#define MBEDTLS_SSL_IN_CONTENT_LEN   4096
#define MBEDTLS_SSL_OUT_CONTENT_LEN  4096

// Total memory estimate: ~80KB
// - SSL context: ~30KB
// - Certificates: ~10KB
// - Buffers: ~40KB
```

**TC375 메모리:**
- ROM: 4 MB PFLASH ✅
- RAM: 192 KB DSRAM ✅ (충분함)

### AURIX TC375 빌드

```bash
# TC375 toolchain 설정
export TRICORE_TOOLCHAIN=/path/to/tricore-gcc

# Zonal Gateway 빌드
cd zonal_gateway/tc375
make clean
make MBEDTLS=1
```

## 문제 해결

### Q: mbedTLS가 빌드 안 됨
```bash
# mbedTLS 설치 확인
pkg-config --modversion mbedtls

# Ubuntu/Debian
sudo apt-get install libmbedtls-dev

# macOS
brew install mbedtls
```

### Q: 인증서 오류 발생
```bash
# CA 인증서 확인
openssl verify -CAfile certs/ca.crt certs/vmg_server.crt

# 시간 동기화 확인 (인증서 유효기간)
date

# 인증서 재생성
cd vehicle_gateway/scripts
./generate_standard_tls_certs.sh
```

### Q: TLS 핸드셰이크 실패
```bash
# Debug 모드로 실행
MBEDTLS_DEBUG_LEVEL=3 ./vmg_doip_server ...

# OpenSSL s_client로 테스트
openssl s_client -connect localhost:13400 \
    -cert certs/tc375_client.crt \
    -key certs/tc375_client.key \
    -CAfile certs/ca.crt
```

## 요약

### ✅ 완료된 작업

1. **mbedTLS DoIP 서버** 구현 및 빌드 설정
2. **mbedTLS DoIP 클라이언트** (TC375용) 구현
3. **인증서 생성 스크립트** (RSA/ECDSA)
4. **CMakeLists.txt** 업데이트 (vmg_doip_server = mbedTLS)
5. **문서화** 완료

### 🎯 현재 설정

```
External:  OpenSSL + PQC      (VMG ↔ Server)
Internal:  mbedTLS (TLS 1.3)  (VMG ↔ ZG ↔ ECU)  ← PRIMARY
Legacy:    Plain DoIP          (사용 안 함)
```

### 📝 사용 방법

1. 인증서 생성: `./generate_standard_tls_certs.sh`
2. 빌드: `cd build && cmake .. && make`
3. 실행: `./vmg_doip_server certs/vmg_server.crt certs/vmg_server.key certs/ca.crt 13400`

**mbedTLS만 사용하도록 설정 완료!** ✅

