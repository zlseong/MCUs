# 🚗 완전한 시스템 개요

## 🎯 전체 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│  Gateway (Linux/Mac)                                        │
│  - vehicle_gateway (C++)                                    │
│  - HTTP/WebSocket/MQTT Client                               │
│  - OTA Controller                                           │
└───────────────────────────┬─────────────────────────────────┘
                            │ TLS 1.3 + PQC
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  Server (Python)                                            │
│  - mqtt_protocol (FastAPI)                                  │
│  - WebSocket/HTTP API                                       │
└───────────────────────────┬─────────────────────────────────┘
                            │ TLS 1.3 + PQC
                            ▼
┌─────────────────────────────────────────────────────────────┐
│  TC375 MCU (TriCore)                                        │
│                                                             │
│  ┌───────────────────────────────────────────────┐         │
│  │ Stage 1 Bootloader (64 KB) [불변]            │         │
│  │ - Stage 2 검증 & 선택                         │         │
│  └────────────────┬──────────────────────────────┘         │
│                   │                                         │
│         ┌─────────┴─────────┐                               │
│         ▼                   ▼                               │
│  ┌─────────────┐     ┌─────────────┐                       │
│  │ Stage 2A    │     │ Stage 2B    │  [OTA 가능]           │
│  │ 188 KB      │     │ 188 KB      │                       │
│  └──────┬──────┘     └──────┬──────┘                       │
│         │                   │                               │
│    ┌────┴────┐         ┌────┴────┐                         │
│    ▼         ▼         ▼         ▼                         │
│  ┌────┐   ┌────┐   ┌────┐   ┌────┐                       │
│  │App │   │App │   │App │   │App │  [OTA 가능]           │
│  │ A  │   │ B  │   │ A  │   │ B  │                       │
│  │2.4M│   │2.4M│   │2.4M│   │2.4M│                       │
│  └────┘   └────┘   └────┘   └────┘                       │
│                                                             │
│  Application 기능:                                          │
│  - UDS Handler (ISO 14229)                                 │
│  - OTA Manager (Stage 2 + App 업데이트)                    │
│  - TLS Client (Gateway 통신)                               │
│  - CAN Gateway, Sensors, etc.                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 📋 **3가지 질문에 대한 답변**

### ✅ **1. UDS 메시지 구현**

**파일:**
- `tc375_simulator/include/uds_handler.hpp`
- `tc375_simulator/src/uds_handler.cpp`

**주요 서비스:**
```cpp
0x10  Diagnostic Session Control
0x27  Security Access (Seed/Key)
0x34  Request Download (OTA 시작)
0x36  Transfer Data (펌웨어 전송)
0x37  Request Transfer Exit
0x22  Read Data By ID
0x11  ECU Reset
```

**사용 예시:**
```cpp
UdsHandler uds;
UdsMessage request = receiveFromGateway();
UdsResponse response = uds.handleRequest(request);
sendToGateway(response);
```

---

### ✅ **2. 에러 핸들링 & 롤백 설계**

**파일:**
- `tc375_simulator/include/ota_manager.hpp`
- `tc375_simulator/src/ota_manager.cpp`

**3단계 Fail-safe:**

```
Level 1: Download 실패
  → rollback() → 다운로드 중단, 이전 상태 유지

Level 2: Verification 실패
  → eraseBank(target) → 잘못된 펌웨어 삭제

Level 3: Boot 실패 (3회)
  → autoRollback() → 이전 뱅크로 자동 전환
```

**트랜잭션 패턴:**
```cpp
OtaTransaction txn;
txn.begin();            // 체크포인트 생성

if (download_success) {
    txn.commit();       // 변경 확정
} else {
    txn.rollback();     // 원상복구
}
```

---

### ✅ **3. 메모리 A/B 분할**

**파일:**
- `tc375_bootloader/common/boot_common.h` (메모리 맵)
- `tc375_bootloader/stage1/stage1_linker.ld` (Stage 1 링커)
- `tc375_bootloader/stage2/stage2_linker.ld` (Stage 2 링커)

**메모리 구조:**

```c
// Stage 1 (불변)
0x80000000 - 0x8000FFFF   (64 KB)

// Stage 2A/B (OTA 가능)
0x80011000 - 0x8003FFFF   (188 KB) Stage 2A
0x80041000 - 0x8006FFFF   (188 KB) Stage 2B

// Application A/B (OTA 가능)
0x80071000 - 0x802F0FFF   (2.4 MB) App A
0x802F2000 - 0x80571FFF   (2.4 MB) App B
```

**뱅크 전환:**
```cpp
// EEPROM에 저장
BootConfig cfg;
cfg.stage2_active = B;  // Stage 2B 선택
cfg.app_active = B;     // App B 선택

// Reboot → 새 뱅크에서 부팅!
```

---

## 🎊 **생성된 파일들 (MCUs 프로젝트)**

```
MCUs/
├── tc375_bootloader/
│   ├── README.md                      ✅ 부트로더 개요
│   ├── build_bootloader.sh            ✅ 빌드 스크립트
│   ├── common/
│   │   └── boot_common.h              ✅ 공통 정의
│   ├── stage1/
│   │   ├── stage1_main.c              ✅ Stage 1 코드
│   │   └── stage1_linker.ld           ✅ 링커 스크립트
│   └── stage2/
│       ├── stage2_main.c              ✅ Stage 2 코드
│       └── stage2_linker.ld           ✅ 링커 스크립트
│
├── tc375_simulator/
│   ├── include/
│   │   ├── uds_handler.hpp            ✅ UDS 구현
│   │   └── ota_manager.hpp            ✅ OTA + 롤백
│   └── src/
│       ├── uds_handler.cpp            ✅
│       └── ota_manager.cpp            ✅
│
└── docs/
    ├── dual_bootloader_ota.md         ✅ 완전한 OTA 가이드
    ├── firmware_architecture.md       ✅ UDS/롤백/A-B 상세
    └── bootloader_implementation.md   ✅ 구현 가이드
```

---

## 🎯 **당신의 질문 3개 - 모두 구현 완료!**

### **1. UDS 메시지 ✅**
- ISO 14229 표준 구현
- 진단, OTA, 보안 접근
- Gateway ↔ TC375 통신

### **2. 에러 핸들링 & 롤백 ✅**
- 3단계 Fail-safe
- 트랜잭션 패턴
- 자동 복구

### **3. A/B 메모리 분할 ✅**
- Stage 2: A/B 듀얼
- Application: A/B 듀얼  
- 부트로더도 업데이트 가능!

---

## 📊 **최종 시스템 비교**

```
단순 구조 (옵션 A):
  부트로더 1개 → App A/B
  
당신의 선택 (옵션 B):
  Stage 1 → Stage 2 A/B → App A/B
  
장점:
✅ 부트로더 버그 수정 가능
✅ PQC 알고리즘 업그레이드 가능
✅ 3단계 롤백
✅ 최대 안전성
```

---

## 🚀 **다음 단계:**

Git에 커밋하고 푸시할까요?

```bash
cd /Users/jiseong/Desktop/MCUs
git add -A
git commit -m "feat: Add 2-stage bootloader and OTA capabilities"
git push origin main
```

진행하시겠습니까? 👍
