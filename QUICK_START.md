# Quick Start Guide

TC375 ECU 시스템과 VMG Gateway를 빠르게 시작하는 방법.

## 필요 사항

- macOS/Linux
- OpenSSL 3.0+
- CMake 3.15+
- GCC/Clang

OpenSSL 버전 확인:
```bash
openssl version
# OpenSSL 3.x.x 필요
```

## 1. VMG Gateway 설정

### 인증서 생성

#### DoIP용 (mbedTLS)
```bash
cd vehicle_gateway
chmod +x scripts/generate_standard_certs.sh
./scripts/generate_standard_certs.sh
```

#### 외부 서버용 (Pure PQC)
```bash
chmod +x scripts/generate_pqc_certs.sh
./scripts/generate_pqc_certs.sh
```

12개 조합의 인증서가 생성됩니다:
- KEM: ML-KEM-512, 768, 1024
- SIG: ECDSA-P256, ML-DSA-44, 65, 87

권장:
- `mlkem768_mldsa65_*` (Pure PQC)
- `mlkem768_ecdsa_secp256r1_sha256_*` (가벼운 서명)

### VMG 빌드

```bash
chmod +x build.sh
./build.sh
```

생성되는 실행 파일:
- `build/vmg_doip_server`: DoIP 서버 (mbedTLS)
- `build/vmg_https_client`: HTTPS 클라이언트 (Pure PQC)
- `build/vmg_mqtt_client`: MQTT 클라이언트 (Pure PQC)

## 2. TC375 Simulator 빌드

```bash
cd ../tc375_simulator
mkdir build && cd build
cmake ..
make
```

생성되는 실행 파일:
- `tc375_simulator`: 기존 시뮬레이터
- `tc375_doip_client`: DoIP 클라이언트 (mbedTLS)

## 3. 실행

### Terminal 1: VMG DoIP Server 시작

```bash
cd vehicle_gateway
./build/vmg_doip_server \
    certs/vmg_server.crt \
    certs/vmg_server.key \
    certs/ca.crt \
    13400
```

출력 예:
```
========================================
VMG DoIP Server (mbedTLS)
========================================
Protocol:    TLS 1.3
Cipher:      TLS_AES_256_GCM_SHA384
Key Exchange: ECDHE-P521
========================================
[Server] Listening on port 13400
[VMG] Ready to accept TC375 clients...
```

### Terminal 2: TC375 Client 연결

```bash
cd tc375_simulator/build
./tc375_doip_client \
    127.0.0.1 13400 \
    ../../vehicle_gateway/certs/tc375_client.crt \
    ../../vehicle_gateway/certs/tc375_client.key \
    ../../vehicle_gateway/certs/ca.crt
```

출력 예:
```
========================================
TC375 DoIP Client (mbedTLS)
========================================
VMG: 127.0.0.1:13400
========================================
[TC375] TCP connected to VMG: 127.0.0.1:13400
[TC375] TLS handshake successful
[TC375] Cipher: TLS_AES_256_GCM_SHA384
[TC375] Protocol: TLSv1.3
[TC375] Sending routing activation request...
[TC375] Routing activated successfully
[TC375] Sending diagnostic messages...
```

## 4. 테스트

### HTTPS Client 테스트

```bash
cd vehicle_gateway
./build/vmg_https_client \
    https://example.com \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca.crt
```

### MQTT Client 테스트

```bash
cd vehicle_gateway
./build/vmg_mqtt_client \
    mqtts://test.mosquitto.org:8883 \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca.crt
```

## 트러블슈팅

### OpenSSL 3.0 미설치

```bash
# macOS (Homebrew)
brew install openssl@3
export PKG_CONFIG_PATH="/usr/local/opt/openssl@3/lib/pkgconfig"

# Ubuntu/Debian
sudo apt update
sudo apt install libssl-dev
```

### PQC 알고리즘 미지원

OpenSSL 3.2+ 필요:
```bash
openssl list -kem-algorithms | grep mlkem
openssl list -signature-algorithms | grep dilithium
```

출력이 없으면 PQC 지원이 없는 버전입니다.

### 인증서 오류

```bash
cd vehicle_gateway
rm -rf certs/*
./scripts/generate_standard_certs.sh
./scripts/generate_pqc_certs.sh
```

### 포트 충돌

```bash
# 포트 사용 확인
lsof -i :13400

# 다른 포트 사용
./build/vmg_doip_server ... 13401
./tc375_doip_client 127.0.0.1 13401 ...
```

## 다음 단계

1. 실제 TC375 하드웨어 연결
2. OTA 업데이트 테스트
3. 부트로더 통합
4. 외부 MQTT 브로커 연동
5. OTA 서버 구축

자세한 내용은 각 디렉토리의 README.md 참조.

