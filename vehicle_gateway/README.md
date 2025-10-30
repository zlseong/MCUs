# Vehicle Management Gateway (VMG)

TC375 차량 ECU 시스템을 위한 중앙 게이트웨이.
Pure PQC TLS, DoIP, MQTT, HTTPS 지원.

## 보안

### PQC (Post-Quantum Cryptography)

양자 컴퓨터 공격에 대비한 암호화 적용:

- ML-KEM (FIPS 203): 키 교환 메커니즘
  - ML-KEM-512: 128-bit 보안 강도
  - ML-KEM-768: 192-bit 보안 강도 (권장)
  - ML-KEM-1024: 256-bit 보안 강도

- ML-DSA (FIPS 204): 전자서명
  - ML-DSA-44 (Dilithium2): 128-bit 보안
  - ML-DSA-65 (Dilithium3): 192-bit 보안 (권장)
  - ML-DSA-87 (Dilithium5): 256-bit 보안

### PQC 모드 (ML-KEM + ML-DSA/ECDSA)

Key Exchange는 ML-KEM만 사용 (양자 내성).
Signature는 ML-DSA 또는 ECDSA 선택 가능.

권장 조합:
```
ML-KEM-768 + ML-DSA-65 (Pure PQC, 192-bit 보안 강도)
ML-KEM-768 + ECDSA-P256 (가벼운 서명)
```

가용 조합:
- ML-KEM-512 + ECDSA-P256 (128-bit, lighter)
- ML-KEM-768 + ECDSA-P256 (192-bit, lighter)
- ML-KEM-1024 + ECDSA-P256 (256-bit, lighter)
- ML-KEM-512 + ML-DSA-44 (128-bit, pure PQC)
- ML-KEM-768 + ML-DSA-65 (192-bit, pure PQC) - **권장**
- ML-KEM-1024 + ML-DSA-87 (256-bit, pure PQC)

## 구성

VMG는 세 가지 통신 모드 지원:

1. DoIP Server (TC375 ← VMG)
   - TC375가 Client로 연결
   - Port 13400
   - PQC mTLS

2. HTTPS Client (VMG → External Server)
   - OTA 패키지 다운로드
   - Fleet 관리 API
   - PQC TLS

3. MQTT Client (VMG → MQTT Broker)
   - 텔레메트리 전송
   - 원격 제어 수신
   - PQC TLS over MQTT

## 요구사항

- OpenSSL 3.2+  (PQC 지원 필수)
- CMake 3.15+
- GCC/Clang
- Linux/macOS

OpenSSL 3.2 PQC 지원 확인:
```bash
openssl list -kem-algorithms | grep -i mlkem
openssl list -signature-algorithms | grep -i dilithium
```

## 빌드

```bash
# 인증서 생성
cd vehicle_gateway
chmod +x scripts/generate_pqc_certs.sh
./scripts/generate_pqc_certs.sh

# 빌드
mkdir build && cd build
cmake ..
make
```

출력:
- `vmg_doip_server`: DoIP 서버 (PQC)
- `vmg_https_client`: HTTPS 클라이언트 (PQC)
- `vmg_mqtt_client`: MQTT 클라이언트 (PQC)

## 실행

### DoIP Server

```bash
./vmg_doip_server \
    certs/mlkem768_mldsa65_server.crt \
    certs/mlkem768_mldsa65_server.key \
    certs/ca.crt \
    13400
```

TC375 클라이언트 연결:
```bash
./tc375_client \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca.crt \
    192.168.1.1 13400
```

### HTTPS Client

```bash
./vmg_https_client \
    https://ota.example.com/firmware.bin \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca_pqc.crt
```

### MQTT Client

```bash
./vmg_mqtt_client \
    mqtts://broker.example.com:8883 \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca_pqc.crt
```

## 성능

ML-KEM-768 + ML-DSA-65 기준 (Benchmark 결과):

- 핸드셰이크: ~15ms
- 인증서 크기: ~4KB
- CPU 오버헤드: 적절한 수준
- 메모리: +50KB (기존 대비)

성능 측정:
```bash
# 벤치마크 프로젝트 참조
git clone https://github.com/zlseong/Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-.git
cd Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-
make
./run_benchmark.sh
```

## 참고

- [Benchmark Project](https://github.com/zlseong/Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-.git)
- FIPS 203 (ML-KEM): https://csrc.nist.gov/pubs/fips/203/final
- FIPS 204 (ML-DSA): https://csrc.nist.gov/pubs/fips/204/final
- OpenSSL 3.2+ PQC: https://www.openssl.org/

## TODO

- [ ] MQTT QoS 2 with PQC
- [ ] Certificate rotation
- [ ] Hardware acceleration (AES-NI)
- [ ] Performance profiling

