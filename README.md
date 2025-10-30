# TC375 Vehicle ECU System

AURIX TC375 기반 차량 ECU 시스템 구현.
OTA(Over-The-Air) 업데이트, 보안 부팅, DoIP 통신, Pure PQC TLS 지원.

## 시스템 구성

```
VMG (Gateway)
    |
    +-- TC375 #1 (Domain Controller)
            |
            +-- TC375 #2 (ECU)
            +-- TC375 #3 (ECU)
```

- VMG: macOS/Linux, DoIP Server, OTA 관리
- TC375 #1: Domain Controller, 중계 역할
- TC375 #2+: 말단 ECU

## 주요 기능

### 부트로더
- 2-Stage 부트로더 (SSW + Application Bootloader)
- Dual Bank 구조 (Region A/B)
- 안전한 OTA 업데이트 지원
- 자동 롤백 기능

### 통신
- DoIP (Diagnostics over IP) / ISO 13400
- TLS 1.3 암호화
- UDS (Unified Diagnostic Services) / ISO 14229
- Persistent TCP 연결

### 보안
- PQC TLS (ML-KEM + ML-DSA/ECDSA)
- ML-KEM-768 키 교환 (양자 내성)
- ML-DSA-65 또는 ECDSA-P256 전자서명
- X.509 인증서 기반 mTLS
- Key Exchange는 ML-KEM만 사용 (X25519 미사용)

## 메모리 구조

TC375 6MB PFLASH, Region A/B 각 3MB:

```
Region A (0x80000000):
  0x80000100  SSW (64KB)
  0x80020000  HSM PCODE (512KB)
  0x800A1000  Bootloader (200KB)
  0x800D3000  Application (2.1MB)

Region B (0x82000000):
  동일 구조, OTA 백업용
```

## 빌드

### 요구사항
- AURIX Development Studio
- TriCore GCC
- CMake 3.15+

### 부트로더 빌드
```bash
cd tc375_bootloader
./build_bootloader.sh
```

출력:
- ssw/ssw_boot.hex
- bootloader/bootloader_a_boot.hex
- bootloader/bootloader_b_boot.hex

### 시뮬레이터 빌드
```bash
cd tc375_simulator
mkdir build && cd build
cmake ..
make
```

## 네트워크 설정

```
VMG:    192.168.1.1:13400
MCU#1:  192.168.1.10:13401
MCU#2:  192.168.1.11
```

시뮬레이터는 localhost의 다른 포트 사용 (13401, 13402...).

## 실행

```bash
# Terminal 1: VMG
cd vehicle_gateway/build
./vmg_gateway

# Terminal 2: TC375 Simulator #1
cd tc375_simulator/build
./tc375_simulator --port 13401

# Terminal 3: TC375 Simulator #2
./tc375_simulator --port 13402
```

## OTA 업데이트

```
vmg> ota TC375_Engine firmware.bin
```

1. VMG가 MCU#1로 펌웨어 전송
2. MCU#1이 Region B에 쓰기
3. 검증 후 Region 전환
4. 재부팅

실패 시 자동으로 이전 Region으로 롤백.

## 문서

- `docs/corrected_architecture.md`: 시스템 아키텍처
- `docs/tc375_memory_map_corrected.md`: 메모리 맵
- `docs/doip_tls_architecture.md`: DoIP 통신
- `docs/ISO_13400_specification.md`: DoIP 표준
- `docs/bootloader_implementation.md`: 부트로더 구현

## 참고

- TC375 Lite Kit 기준 (6MB PFLASH)
- ISO 13400 (DoIP), ISO 14229 (UDS) 준수
- Infineon AURIX 표준 메모리 맵 사용

## 라이선스

MIT

## 추가 문서

- `docs/vmg_pqc_implementation.md`: VMG Pure PQC TLS 구현 상세
- `vehicle_gateway/README.md`: VMG 빌드 및 사용법
- Benchmark 참조: https://github.com/zlseong/Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-.git

## TODO

- [x] VMG Gateway PQC 구현
- [x] TC375 PQC DoIP Client 구현
- [x] TLS 인증서 생성 자동화
- [ ] MQTT QoS 2 최적화
- [ ] OTA 진행률 표시
- [ ] 실제 TC375 하드웨어 테스트
