# VMG Pure PQC TLS Implementation

VMG (Vehicle Management Gateway) with Pure Post-Quantum Cryptography support.

## Overview

VMG는 TC375 ECU들을 관리하는 중앙 게이트웨이로, 양자 내성 암호화(PQC)를 적용한 보안 통신을 제공합니다.

## Architecture

```
External Server (OTA/API)
        |
        | HTTPS (PQC)
        v
      [VMG]
        |
        | DoIP (PQC mTLS)
        v
    [TC375 #1] -----> [TC375 #2]
   (Domain/Zonal)      (End ECU)
```

## PQC Algorithms

### ML-KEM (Key Encapsulation Mechanism)

NIST FIPS 203 표준. Pure PQC 키 교환.

- ML-KEM-512: 128-bit 보안 강도
- ML-KEM-768: 192-bit 보안 강도 (권장)
- ML-KEM-1024: 256-bit 보안 강도

### ML-DSA (Digital Signature Algorithm)

NIST FIPS 204 표준. Pure PQC 전자서명.

- ML-DSA-44 (Dilithium2): 128-bit 보안
- ML-DSA-65 (Dilithium3): 192-bit 보안 (권장)
- ML-DSA-87 (Dilithium5): 256-bit 보안

## Communication Protocols

### 1. DoIP Server (VMG ← TC375)

VMG는 DoIP 서버 역할. TC375가 클라이언트로 연결.

Port: 13400
Protocol: DoIP over Pure PQC mTLS
TLS Version: 1.3 only

Features:
- Routing activation
- Diagnostic message forwarding
- UDS support
- mTLS mutual authentication

### 2. HTTPS Client (VMG → External Server)

OTA 패키지 다운로드, Fleet API 호출.

Protocol: HTTPS with PQC TLS
Methods: GET, POST

Use cases:
- Firmware download
- Vehicle telemetry upload
- Remote diagnostics

### 3. MQTT Client (VMG → MQTT Broker)

실시간 텔레메트리 및 원격 제어.

Protocol: MQTT 3.1.1 over PQC TLS
Port: 8883 (MQTTS)
QoS: 0, 1, 2

Topics:
- vmg/telemetry: Vehicle data
- vmg/command: Remote control
- vmg/status: System status

## Implementation

### VMG Components

```
vehicle_gateway/
├── src/
│   ├── doip_server.cpp         # DoIP server main
│   ├── https_client.cpp        # HTTPS client
│   ├── mqtt_client.cpp         # MQTT client
│   ├── pqc_tls_server.c        # PQC TLS server wrapper
│   ├── pqc_tls_client.c        # PQC TLS client wrapper
│   └── pqc_mqtt.c              # MQTT over PQC TLS
├── common/
│   ├── pqc_config.{h,c}        # PQC algorithm configuration
│   ├── metrics.{h,c}           # Performance metrics
│   └── json_output.{h,c}       # JSON/CSV output
└── scripts/
    └── generate_pqc_certs.sh   # Certificate generation
```

### TC375 Client

```
tc375_simulator/
├── include/
│   └── pqc_doip_client.hpp     # DoIP client with PQC
└── src/
    └── pqc_doip_client.cpp     # Implementation
```

## Build

### Prerequisites

```bash
# OpenSSL 3.2+ with PQC support
openssl version  # Must be 3.x

# Verify PQC support
openssl list -kem-algorithms | grep mlkem
openssl list -signature-algorithms | grep dilithium
```

### VMG

```bash
cd vehicle_gateway
./scripts/generate_pqc_certs.sh
./build.sh

# Output:
#   build/vmg_doip_server
#   build/vmg_https_client
#   build/vmg_mqtt_client
```

### TC375 Simulator

```bash
cd tc375_simulator
mkdir build && cd build
cmake ..
make

# Output:
#   build/tc375_pqc_client
```

## Usage

### 1. Start VMG DoIP Server

```bash
cd vehicle_gateway
./build/vmg_doip_server \
    certs/mlkem768_mldsa65_server.crt \
    certs/mlkem768_mldsa65_server.key \
    certs/ca.crt \
    13400
```

### 2. Connect TC375 Client

```bash
cd tc375_simulator/build
./tc375_pqc_client \
    192.168.1.1 13400 \
    ../certs/mlkem768_mldsa65_client.crt \
    ../certs/mlkem768_mldsa65_client.key \
    ../certs/ca.crt
```

### 3. HTTPS Client Example

```bash
cd vehicle_gateway
./build/vmg_https_client \
    https://ota.example.com/firmware.bin \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca.crt
```

### 4. MQTT Client Example

```bash
cd vehicle_gateway
./build/vmg_mqtt_client \
    mqtts://broker.example.com:8883 \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca.crt
```

## Performance

Based on Benchmark_mTLS_with_PQC project results.

### ML-KEM-768 + ML-DSA-65

- Handshake latency: ~15ms (vs ~10ms for X25519+ECDSA)
- Certificate size: ~4KB (vs ~2KB for ECDSA)
- CPU overhead: ~10% increase
- RAM usage: +50KB

### Comparison

| Algorithm          | Handshake | Cert Size | Security Level |
|-------------------|-----------|-----------|----------------|
| ML-KEM-512 + ML-DSA-44 | 12ms | 3KB   | PQC (128-bit)  |
| ML-KEM-768 + ML-DSA-65 | 15ms | 4KB   | PQC (192-bit)  |
| ML-KEM-1024 + ML-DSA-87| 18ms | 5KB   | PQC (256-bit)  |

### TC375 Considerations

TC375 Lite Kit:
- CPU: 300 MHz TriCore
- RAM: 96 KB DSPR
- Flash: 6 MB PFLASH

PQC overhead is acceptable for:
- Initial handshake (one-time cost)
- OTA updates (low frequency)
- Diagnostic sessions

Not recommended for:
- High-frequency CAN messages
- Real-time control loops
- Safety-critical paths

## Testing

### Benchmark

```bash
# Based on reference implementation
git clone https://github.com/zlseong/Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-.git
cd Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-
make
./run_benchmark.sh
```

### Integration Test

```bash
# Terminal 1: Start VMG
cd vehicle_gateway
./build/vmg_doip_server certs/mlkem768_mldsa65_server.{crt,key} certs/ca.crt 13400

# Terminal 2: Connect TC375
cd tc375_simulator/build
./tc375_pqc_client 127.0.0.1 13400 certs/mlkem768_mldsa65_client.{crt,key} certs/ca.crt
```

## References

- [Benchmark Project](https://github.com/zlseong/Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-.git)
- NIST FIPS 203 (ML-KEM): https://csrc.nist.gov/pubs/fips/203/final
- NIST FIPS 204 (ML-DSA): https://csrc.nist.gov/pubs/fips/204/final
- ISO 13400 (DoIP): https://www.iso.org/standard/68519.html
- OpenSSL 3.2 PQC: https://www.openssl.org/docs/man3.2/

## Security Considerations

### Certificate Management

- Certificate rotation: Every 90 days
- CA management: Offline root CA
- Key storage: Hardware Security Module (HSM) for production

### TLS Configuration

- TLS 1.3 only (no downgrade)
- Perfect Forward Secrecy (PFS)
- Strong cipher suites only
- Client certificate validation (mTLS)

### DoIP Security

- Routing activation required
- Source address validation
- Diagnostic message filtering
- Rate limiting

## Future Work

- Hardware acceleration for ML-KEM/ML-DSA
- Session resumption with 0-RTT
- MQTT QoS 2 optimization
- Certificate auto-renewal
- HSM integration

