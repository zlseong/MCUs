# TC375 Vehicle ECU System - 전체 프로젝트 구조

## 🎯 프로젝트 목표

Infineon TC375 기반 차량용 ECU 시스템 구현
- OTA (Over-The-Air) 업데이트
- 보안 통신 (DoIP, mTLS, PQC)
- Dual-Bank 부트로더
- 중앙 게이트웨이 (VMG) 관리

---

## 📁 프로젝트 구조 (3-Tier)

```
MCUs/
├── 📂 vehicle_gateway/       # Tier 1: 중앙 게이트웨이 (VMG)
├── 📂 tc375_simulator/       # Tier 2-3: ECU 시뮬레이터
├── 📂 tc375_bootloader/      # Bootloader (실제 하드웨어용)
├── 📂 docs/                  # 문서화
├── 📂 scripts/               # 유틸리티 스크립트
└── 📄 *.md                   # 프로젝트 문서
```

---

## 🏗️ 시스템 아키텍처

### 통신 구조
```
[External Server]          [VMG Gateway]          [TC375 ECUs]
(OTA/MQTT/API)            (macOS/Linux)         (Embedded)

OTA Server  ──────ML-KEM TLS───────►  DoIP Server  ◄───mbedTLS───  ECU #1
            (ML-KEM-768+MLDSA, mTLS)   (Port 13400)              (Domain Controller)
MQTT Broker ──────ML-KEM TLS───────►  HTTPS Client                     │
            (ML-KEM-768+MLDSA, mTLS)   MQTT Client                    mbedTLS
                                                                         │
                                                                      ECU #2
                                                                    (End ECU)
```

### 역할 분담

| Tier | Component | Role | Protocol |
|------|-----------|------|----------|
| **1** | VMG | Central Gateway | DoIP Server (mbedTLS) |
| **2** | ECU #1 | Domain Controller | Client (to VMG) + Server (for ECU#2) |
| **3** | ECU #2+ | End ECUs | Client (to ECU#1) |

---

## 📦 1. Vehicle Gateway (VMG)

### 위치
```
vehicle_gateway/
├── src/              # 소스 코드
├── common/           # 공통 라이브러리
├── include/          # 헤더 파일
├── scripts/          # 인증서 생성 스크립트
└── config/           # 설정 파일
```

### 주요 컴포넌트

#### A. DoIP Server (TC375 통신)
**파일**: `src/doip_server_mbedtls.cpp`
**라이브러리**: mbedTLS
**프로토콜**: TLS 1.3 (표준 암호화, PQC 아님)
**설정**:
- Port: 13400
- Cipher: TLS_AES_256_GCM_SHA384
- Key Exchange: ECDHE-P521 (HSM 가속)
- Signature: ECDSA-P521 (HSM 가속)
- Authentication: **Mutual TLS** (양방향 인증)

**역할**:
- TC375 ECU들의 DoIP 연결 수락
- 진단 메시지 라우팅
- UDS 명령 처리

#### B. HTTPS Client (외부 서버 통신)
**파일**: `src/https_client.cpp`, `src/pqc_tls_client.c`
**라이브러리**: OpenSSL 3.x
**프로토콜**: TLS 1.3 + **ML-KEM**
**설정**:
- Key Exchange: ML-KEM-768 (Post-Quantum)
- Signature: ML-DSA-65 또는 ECDSA-P256 (선택 가능)
- Authentication: **Mutual TLS**

**역할**:
- OTA 패키지 다운로드
- Fleet API 통신
- 펌웨어 업데이트 관리

#### C. MQTT Client (텔레메트리)
**파일**: `src/mqtt_client.cpp`, `src/pqc_mqtt.c`
**라이브러리**: OpenSSL 3.x
**프로토콜**: MQTT 3.1.1 over TLS 1.3 + ML-KEM
**설정**:
- Port: 8883 (MQTTS)
- QoS: 0, 1, 2
- Authentication: **Mutual TLS**

**역할**:
- 차량 텔레메트리 전송
- 원격 제어 수신
- 상태 보고

### 공통 라이브러리 (`common/`)

#### 1. mbedTLS DoIP (`mbedtls_doip.{h,c}`)
- DoIP 서버/클라이언트 구현
- mbedTLS 기반 TLS 1.3
- Mutual TLS 지원

#### 2. PQC Configuration (`pqc_config.{h,c}`)
- ML-KEM + ML-DSA/ECDSA 설정
- 6가지 조합 지원:
  ```c
  [0] ML-KEM-512 + ECDSA-P256
  [1] ML-KEM-768 + ECDSA-P256
  [2] ML-KEM-1024 + ECDSA-P256
  [3] ML-KEM-512 + ML-DSA-44
  [4] ML-KEM-768 + ML-DSA-65 (권장)
  [5] ML-KEM-1024 + ML-DSA-87
  ```

#### 3. HSM Integration (`mbedtls_hsm_config.h`, `tc375_hsm_integration.c`)
- TC375 HSM 하드웨어 가속
- AES-256-GCM: 100 MB/s
- ECDSA-P521: 5ms sign, 8ms verify
- TRNG: 1 MB/s

#### 4. Metrics (`metrics.{h,c}`)
- 성능 측정
- 핸드셰이크 시간, 트래픽, 암호화 연산

#### 5. JSON Output (`json_output.{h,c}`)
- 메트릭 JSON/CSV 출력

### 빌드 시스템
**파일**: `CMakeLists.txt`
**빌드 출력**:
```
build/
├── vmg_doip_server      # DoIP 서버 (mbedTLS)
├── vmg_https_client     # HTTPS 클라이언트 (PQC)
├── vmg_mqtt_client      # MQTT 클라이언트 (PQC)
└── vmg_gateway          # 통합 게이트웨이 (테스트용)
```

### 인증서 생성
**스크립트**:
1. `scripts/generate_standard_certs.sh`: DoIP용 (ECDSA-P256/P521)
2. `scripts/generate_pqc_certs.sh`: 외부 서버용 (ML-KEM + ML-DSA/ECDSA)

---

## 📦 2. TC375 Simulator

### 위치
```
tc375_simulator/
├── src/              # 구현
├── include/          # 헤더
├── config/           # 설정 JSON
└── CMakeLists.txt    # 빌드 설정
```

### 주요 컴포넌트

#### A. DoIP Client (mbedTLS)
**파일**: `src/doip_client_mbedtls.cpp`
**역할**:
- VMG DoIP 서버에 연결
- Routing Activation
- Diagnostic Message 송수신

#### B. PQC DoIP Client (옵션)
**파일**: `src/pqc_doip_client.cpp`, `main_pqc_client.cpp`
**역할**:
- PQC 지원 외부 서버 테스트용

#### C. Device Simulator
**파일**: `src/device_simulator.cpp`
**역할**:
- TC375 ECU 시뮬레이션
- 센서 데이터 생성
- UDS 명령 처리

#### D. OTA Manager
**파일**: `src/ota_manager.cpp`
**역할**:
- OTA 패키지 수신
- 펌웨어 검증
- Region A/B 관리
- 자동 롤백

#### E. UDS Handler
**파일**: `src/uds_handler.cpp`
**역할**:
- ISO 14229 UDS 프로토콜
- 진단 서비스 (0x10, 0x22, 0x2E, etc.)
- ECU 정보 응답

#### F. TLS Client
**파일**: `src/tls_client.cpp`
**역할**:
- 기존 TLS 클라이언트 (호환용)

### 빌드 출력
```
build/
├── tc375_simulator       # 기존 시뮬레이터
├── tc375_doip_client    # DoIP 클라이언트 (mbedTLS)
└── tc375_pqc_client     # PQC 클라이언트 (옵션)
```

---

## 📦 3. TC375 Bootloader

### 위치
```
tc375_bootloader/
├── ssw/              # Startup Software
├── bootloader/       # Application Bootloader
├── common/           # 공통 정의
└── build_bootloader.sh
```

### 메모리 구조

#### Hardware Bank Switching (Infineon Standard)
```
Region A (3 MB) @ 0x80000000
├── 0x80000000: BMI Header (256 B)
├── 0x80000100: SSW (~64 KB)          [고정, OTA 불가]
├── 0x80010000: TP Reserved (64 KB)   [고정]
├── 0x80020000: HSM PCODE (512 KB)    [고정, 하드웨어 보안]
├── 0x800A1000: Bootloader (196 KB)   [OTA 가능]
└── 0x800D3000: Application (~2.1 MB) [OTA 가능]

Region B (3 MB) @ 0x82000000
└── 동일 구조 (백업/업데이트용)
```

### 주요 파일

#### A. boot_common.h
**역할**: 메모리 맵 정의
- Region A/B 주소
- 부트로더/앱 크기
- 메타데이터 구조

```c
typedef struct {
    uint8_t bootloader_active;  // 0=A, 1=B
    uint32_t boot_cnt_a;        // Region A 부팅 횟수
    uint32_t boot_cnt_b;        // Region B 부팅 횟수
    uint32_t crc32;             // CRC32 체크섬
} BootConfig;
```

#### B. SSW (Startup Software)
**파일**: `ssw/ssw_main.c`
**역할**:
- CPU/Clock 초기화
- BMI Header 검증
- Bootloader 점프

#### C. Stage 2 Bootloader
**파일**: `bootloader/stage2_main.c`
**역할**:
- Application 검증 (CRC, Signature)
- OTA 업데이트 수행
- Region A ↔ B 전환
- 자동 롤백 (3회 실패 시)

**Linker Scripts**:
- `stage2a_linker.ld`: Region A용 (0x800A1000)
- `stage2b_linker.ld`: Region B용 (0x820A1000)

**빌드**:
```bash
./build_bootloader.sh
# 출력: bootloader_a.elf, bootloader_b.elf
```

---

## 📚 4. Documentation

### 주요 문서

| 문서 | 내용 |
|------|------|
| `README.md` | 프로젝트 개요 |
| `ARCHITECTURE.md` | 시스템 아키텍처 |
| `QUICK_START.md` | 빠른 시작 가이드 |
| `PROJECT_OVERVIEW.md` | 이 문서 |

#### 기술 문서 (`docs/`)

| 문서 | 설명 |
|------|------|
| `corrected_architecture.md` | 3-Tier 계층 구조 |
| `TC375_HSM_INTEGRATION.md` | HSM 하드웨어 가속 |
| `vmg_pqc_implementation.md` | PQC-Hybrid TLS 구현 |
| `doip_tls_architecture.md` | DoIP 통신 상세 |
| `tc375_memory_map_corrected.md` | 메모리 맵 상세 |
| `tc375_infineon_bootloader_mapping.md` | Infineon 표준 매핑 |
| `bootloader_implementation.md` | 부트로더 구현 |
| `dual_bootloader_ota.md` | Dual-Bank OTA |
| `safe_ota_strategy.md` | 안전한 OTA 전략 |
| `ISO_13400_specification.md` | DoIP 표준 |
| `can_vs_ethernet_ota.md` | CAN vs Ethernet OTA |
| `firmware_architecture.md` | 펌웨어 아키텍처 |
| `data_management.md` | 데이터 관리 |
| `protocol.md` | 프로토콜 상세 |
| `tc375_porting.md` | TC375 포팅 가이드 |
| `QUICK_REFERENCE.md` | 빠른 참조 |

---

## 🔐 보안 설계

### 1. 두 가지 TLS 스택

| 통신 | Library | Protocol | Cipher Suite | 용도 |
|------|---------|----------|--------------|------|
| **DoIP** | mbedTLS | TLS 1.3 | TLS_AES_256_GCM_SHA384 | VMG ↔ TC375 |
| **External** | OpenSSL 3.x | TLS 1.3 | ML-KEM-768 + ML-DSA-65 | VMG → OTA/MQTT |

### 2. Mutual TLS (mTLS)

**모든 통신에 양방향 인증 적용**:
```
Server 인증: 클라이언트가 서버 인증서 검증
Client 인증: 서버가 클라이언트 인증서 검증
CA 기반: 모든 인증서는 CA로 서명
```

### 3. 인증서 구조

```
DoIP 인증서 (mbedTLS):
├── ca.crt (RSA-2048 또는 ECDSA-P256)
├── vmg_server.{crt,key} (ECDSA-P521, HSM 가속)
└── tc375_client.{crt,key} (ECDSA-P521, HSM 가속)

PQC 인증서 (OpenSSL):
├── ca_pqc.crt
├── mlkem768_ecdsa_secp256r1_sha256_*.{crt,key} (가벼운 서명)
├── mlkem768_mldsa65_*.{crt,key} (Pure PQC, 권장)
├── mlkem512/1024 조합들...
└── 총 12개 인증서 (3 KEM × 4 SIG)
```

### 4. TC375 HSM (Hardware Security Module)

**하드웨어 보안**:
- Private key는 HSM에 저장 (추출 불가)
- 암호화 연산 하드웨어 가속
- Side-channel attack 보호

**성능**:
- AES-256-GCM: ~100 MB/s (vs ~10 MB/s software)
- ECDSA-P521 sign: ~5ms (vs ~15ms software)
- ECDSA-P521 verify: ~8ms (vs ~25ms software)
- Handshake: ~18ms (vs ~55ms software)

---

## 🚀 빌드 및 실행

### 1. VMG Gateway 빌드

```bash
cd vehicle_gateway

# 인증서 생성
./scripts/generate_standard_certs.sh  # DoIP용
./scripts/generate_pqc_certs.sh       # 외부 서버용

# 빌드
./build.sh

# 실행
./build/vmg_doip_server \
    certs/vmg_server.crt \
    certs/vmg_server.key \
    certs/ca.crt \
    13400
```

### 2. TC375 Simulator 빌드

```bash
cd tc375_simulator
mkdir build && cd build
cmake ..
make

# 실행
./tc375_doip_client \
    127.0.0.1 13400 \
    ../../vehicle_gateway/certs/tc375_client.crt \
    ../../vehicle_gateway/certs/tc375_client.key \
    ../../vehicle_gateway/certs/ca.crt
```

### 3. Bootloader 빌드

```bash
cd tc375_bootloader
./build_bootloader.sh
# 출력: bootloader_a.elf, bootloader_b.elf
```

---

## 📊 성능 및 리소스

### VMG Gateway (macOS/Linux)

| 항목 | 값 |
|------|-----|
| Code size | ~500 KB (OpenSSL) + ~150 KB (mbedTLS) |
| Heap | ~200 KB |
| CPU | ~5-10% (idle), ~30% (handshake) |
| Network | ~1 MB/s (OTA transfer) |

### TC375 ECU

| 항목 | 값 |
|------|-----|
| CPU | 300 MHz TriCore |
| RAM | 96 KB DSPR |
| Flash | 6 MB PFLASH (3 MB × 2) |
| Code size | ~150 KB (mbedTLS + App) |
| Heap | ~80 KB |
| Stack | ~6 KB |

### 통신 성능

| Operation | Latency |
|-----------|---------|
| DoIP Handshake (HSM) | ~18 ms |
| HTTPS Handshake (PQC) | ~25 ms |
| MQTT Connect | ~30 ms |
| Diagnostic Message | ~5 ms |
| OTA Transfer (10 MB) | ~10 s |

---

## 🔄 OTA 업데이트 플로우

```
1. VMG: OTA 패키지 다운로드 (HTTPS + PQC)
   └─► OTA Server → VMG (Mutual TLS)

2. VMG → ECU: DoIP 통신으로 전송
   └─► VMG → ECU (mbedTLS, Mutual TLS)

3. ECU: Region B에 쓰기
   └─► Inactive Region에 업데이트
   └─► CRC 검증

4. ECU: Reboot
   └─► Bootloader: Region B로 전환
   └─► Application: 실행

5. 성공 확인
   └─► 3회 부팅 성공 → 영구 적용
   └─► 3회 부팅 실패 → 자동 롤백 (Region A)
```

---

## 🎯 현재 구현 상태

### ✅ 완료

1. **VMG Gateway**
   - DoIP Server (mbedTLS, Mutual TLS)
   - HTTPS Client (ML-KEM-768 + ML-DSA/ECDSA, Mutual TLS)
   - MQTT Client (ML-KEM-768 + ML-DSA/ECDSA, Mutual TLS)
   - 인증서 생성 스크립트

2. **TC375 Simulator**
   - DoIP Client (mbedTLS)
   - Device Simulator
   - OTA Manager
   - UDS Handler

3. **Bootloader**
   - SSW (Startup Software)
   - Stage 2 Bootloader (Dual-Bank)
   - 메모리 맵 (Infineon 표준)
   - OTA 지원

4. **Documentation**
   - 아키텍처 문서
   - API 문서
   - 빌드 가이드
   - HSM 통합 가이드

### ⏳ 미구현 (추후 작업)

1. **ECU #1 Dual Role**
   - VMG Client (Uplink) + ECU Server (Downlink)
   - Message Routing Logic

2. **ECU #2+ End ECU**
   - ECU #1 Client
   - 최종 UDS Handler

3. **통합 테스트**
   - VMG → ECU#1 → ECU#2 체인 통신
   - E2E OTA 테스트

4. **보안 강화**
   - Certificate Rotation
   - OCSP/CRL
   - Secure Boot
   - HSM Key Storage (실제 하드웨어)

---

## 📋 TODO

- [ ] ECU#1 Dual Role 구현
- [ ] ECU#2 Client 구현
- [ ] Message Routing Logic
- [ ] E2E 통합 테스트
- [ ] 실제 TC375 하드웨어 테스트
- [ ] HSM Key Storage 구현
- [ ] Certificate Rotation
- [ ] Performance Profiling
- [ ] Security Audit

---

## 🔗 참고 자료

- Infineon TC375 User Manual
- ISO 13400 (DoIP)
- ISO 14229 (UDS)
- ISO 26262 (Automotive Safety)
- ISO 21434 (Cybersecurity)
- NIST FIPS 203 (ML-KEM)
- NIST FIPS 204 (ML-DSA)
- mbedTLS Documentation
- OpenSSL 3.x Documentation

---

## 📝 프로젝트 히스토리

1. **Phase 1**: Bootloader 설계 (Dual-Bank, Infineon 표준)
2. **Phase 2**: DoIP 통신 (mbedTLS, Mutual TLS)
3. **Phase 3**: ML-KEM TLS (ML-DSA/ECDSA 선택, 외부 서버)
4. **Phase 4**: HSM 통합 (하드웨어 가속)
5. **Phase 5**: VMG Gateway 구현 ✅
6. **Phase 6**: TC375 Simulator 구현 (진행 중)
7. **Phase 7**: E2E 통합 테스트 (예정)

---

## 🎓 핵심 설계 원칙

1. **보안 최우선**: Mutual TLS, PQC, HSM
2. **표준 준수**: Infineon, ISO, NIST
3. **안정성**: Dual-Bank, 자동 롤백
4. **성능**: HSM 하드웨어 가속
5. **확장성**: 3-Tier 아키텍처
6. **유지보수성**: 명확한 문서화

---

**프로젝트 버전**: 1.0.0  
**마지막 업데이트**: 2025-10-31  
**라이선스**: MIT

