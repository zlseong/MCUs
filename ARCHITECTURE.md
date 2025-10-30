# System Architecture

TC375 ECU 시스템의 전체 아키텍처 및 보안 통신 구조

## Communication Architecture

```
External Server                    VMG Gateway                    TC375 ECUs
(OTA/MQTT/API)                    (macOS/Linux)                 (Embedded)

┌──────────────┐                ┌───────────────┐             ┌─────────────┐
│  OTA Server  │                │               │             │  TC375 #1   │
│              │◄───Hybrid──────┤  DoIP Server  │◄──mbedTLS───┤  (Domain    │
│  MQTT Broker │    PQC TLS     │  (mbedTLS)    │             │  Controller)│
│              │    mTLS        │               │             │             │
│  Fleet API   │                │  HTTPS Client │             └─────────────┘
│              │                │  (OpenSSL 3.x)│                    │
└──────────────┘                │               │                  mbedTLS
                               │  MQTT Client  │                    │
                               │  (OpenSSL 3.x)│             ┌─────────────┐
                               └───────────────┘             │  TC375 #2   │
                                                            │  (End ECU)  │
                                                            └─────────────┘
```

## Two TLS Stacks

### 1. DoIP Communication (VMG ↔ TC375)
**Library**: mbedTLS
**Purpose**: 경량화, 임베디드 친화적
**Configuration**:
- Protocol: TLS 1.3
- Authentication: Mutual TLS (양방향 인증)
- PQC: No (표준 암호화만 사용)
- Cipher: AES-GCM, ChaCha20-Poly1305
- Key Exchange: ECDHE (P-256)
- Signature: ECDSA (P-256)

**Why mbedTLS?**
- 작은 메모리 footprint (~100KB)
- TC375 RAM/Flash 제약 적합
- ISO 26262 automotive certification
- 실시간 성능 보장

### 2. External Server Communication (VMG → Server)
**Library**: OpenSSL 3.x
**Purpose**: 최신 PQC 알고리즘 지원
**Configuration**:
- Protocol: TLS 1.3
- Authentication: Mutual TLS (양방향 인증)
- Key Exchange: ML-KEM-768 (Post-Quantum)
- Signature: ML-DSA-65 또는 ECDSA-P256 (선택 가능)
- Cipher: AES-256-GCM

**Why ML-KEM?**
- 양자 컴퓨터 공격 대비 (Key Exchange)
- NIST 표준화 완료 (FIPS 203)
- 외부 서버와의 장거리 통신 보안
- Signature는 요구사항에 따라 선택:
  - ML-DSA: Pure PQC (큰 서명 크기)
  - ECDSA: 가벼운 서명 (작은 크기, 빠른 검증)

## Security Requirements

### Mutual TLS (mTLS)

모든 통신에 양방향 인증 적용:

1. **서버 인증**: 클라이언트가 서버 인증서 검증
2. **클라이언트 인증**: 서버가 클라이언트 인증서 검증
3. **CA 기반**: 모든 인증서는 CA로 서명
4. **인증서 검증**: 
   - Subject Name 확인
   - Validity Period 확인
   - Signature 검증
   - Revocation 확인 (옵션)

### Certificate Management

```
CA Hierarchy:
├── ca.crt (DoIP용, RSA 2048)
│   ├── vmg_server.crt (VMG DoIP 서버)
│   └── tc375_client.crt (TC375 클라이언트)
│
└── ca_pqc.crt (외부 서버용, RSA 2048)
    ├── mlkem768_ecdsa_secp256r1_sha256_*.{crt,key} (가벼운 서명)
    ├── mlkem768_mldsa65_*.{crt,key} (Pure PQC, 권장)
    ├── mlkem512_mldsa44_*.{crt,key}
    └── mlkem1024_mldsa87_*.{crt,key}
```

### Key Storage

- VMG (Gateway):
  - Private keys: 파일 시스템 (chmod 600)
  - Production: HSM 권장
  
- TC375 (ECU):
  - Private keys: HSM PCODE (하드웨어 보안 모듈)
  - Read-only access
  - Physical attack protection

## Protocol Details

### DoIP (ISO 13400)

```
TCP Connection (TLS 1.3 over TCP)
    ↓
DoIP Header (8 bytes)
    ├── Protocol Version: 0x02
    ├── Inverse Version: 0xFD
    ├── Payload Type: 0x0005 (Routing Activation)
    └── Payload Length: variable
    ↓
DoIP Payload
    ├── Source Address: 0x0E80 (TC375)
    ├── Activation Type: 0x00
    └── Data: ...
```

### HTTPS (VMG → OTA Server)

```
HTTP/1.1 GET /firmware.bin
Host: ota.example.com
Authorization: Bearer <token>
User-Agent: VMG/1.0

↓ (TLS 1.3 + PQC-Hybrid)

Response: firmware.bin (encrypted)
```

### MQTT (VMG → Broker)

```
MQTT CONNECT
    ├── Client ID: VMG-001
    ├── Username: vmg_user
    └── Password: <hashed>
    ↓ (TLS 1.3 + PQC-Hybrid)
    
MQTT PUBLISH
    ├── Topic: vmg/telemetry
    ├── QoS: 1
    └── Payload: { "speed": 60, "battery": 80 }
```

## Performance Considerations

### mbedTLS (DoIP)
- Handshake latency: ~10ms (ECDHE-P256)
- Certificate size: ~1KB (ECDSA-P256)
- Memory usage: ~100KB (code) + 50KB (heap)
- CPU overhead: ~5% on TC375 (300MHz TriCore)

### OpenSSL 3.x PQC-Hybrid (External)
- Handshake latency: ~15ms (X25519+ML-KEM-768)
- Certificate size: ~2KB (ECDSA-P256)
- Memory usage: ~500KB (code) + 100KB (heap)
- CPU overhead: ~10% on VMG (Intel/ARM)

## Implementation Status

- [x] mbedTLS DoIP Server (VMG)
- [x] mbedTLS DoIP Client (TC375)
- [x] OpenSSL PQC HTTPS Client (VMG)
- [x] OpenSSL PQC MQTT Client (VMG)
- [x] Mutual TLS 인증
- [x] Certificate generation scripts
- [ ] HSM integration (TC375)
- [ ] Certificate rotation
- [ ] OCSP/CRL

## Build Requirements

### VMG Gateway
```bash
# Required
- OpenSSL 3.2+ (for PQC support)
- mbedTLS 3.0+
- CMake 3.15+
- GCC/Clang

# Build
cd vehicle_gateway
./build.sh
```

### TC375 Simulator
```bash
# Required
- mbedTLS 3.0+
- CMake 3.15+
- GCC/Clang

# Build
cd tc375_simulator
mkdir build && cd build
cmake ..
make
```

## Testing

### Generate Certificates

```bash
# DoIP certificates (mbedTLS)
cd vehicle_gateway
./scripts/generate_standard_certs.sh

# PQC certificates (External servers)
./scripts/generate_pqc_certs.sh
```

### Run DoIP Communication

```bash
# Terminal 1: VMG DoIP Server
cd vehicle_gateway
./build/vmg_doip_server \
    certs/vmg_server.crt \
    certs/vmg_server.key \
    certs/ca.crt \
    13400

# Terminal 2: TC375 Client
cd tc375_simulator/build
./tc375_doip_client \
    127.0.0.1 13400 \
    ../../vehicle_gateway/certs/tc375_client.crt \
    ../../vehicle_gateway/certs/tc375_client.key \
    ../../vehicle_gateway/certs/ca.crt
```

### Run External Server Communication

```bash
# HTTPS Client
cd vehicle_gateway
./build/vmg_https_client \
    https://ota.example.com \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca_pqc.crt

# MQTT Client
./build/vmg_mqtt_client \
    mqtts://broker.example.com:8883 \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca_pqc.crt
```

## References

- ISO 13400 (DoIP): https://www.iso.org/standard/68519.html
- ISO 14229 (UDS): https://www.iso.org/standard/72439.html
- NIST FIPS 203 (ML-KEM): https://csrc.nist.gov/pubs/fips/203/final
- NIST FIPS 204 (ML-DSA): https://csrc.nist.gov/pubs/fips/204/final
- mbedTLS: https://www.trustedfirmware.org/projects/mbed-tls/
- OpenSSL 3.x: https://www.openssl.org/

