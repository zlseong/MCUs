# 문서 정리 계획

## 현재 상태 분석

**총 문서 수: 24개**

---

## 카테고리별 분류

### 1. Bootloader 관련 (3개 → 1개로 통합)
- ❌ **bootloader_implementation.md** (한글, 초안)
- ❌ **dual_bootloader_ota.md** (한글, 2-stage 설명)
- ✅ **tc375_infineon_bootloader_mapping.md** (최종본, 유지)
- **삭제 이유:** 내용이 중복되고, Infineon 공식 매핑 문서가 가장 정확함

### 2. 네트워크/통신 관련 (5개 → 2개로 통합)
- ✅ **ISO_13400_specification.md** (DoIP 표준, 유지)
- ❌ **doip_tls_architecture.md** (중복, ISO 13400에 통합 가능)
- ✅ **MBEDTLS_HANDSHAKE_TIMING.md** (상세 타이밍, 유지)
- ❌ **RECONNECTION_STRATEGY.md** (구현 완료, 아카이브)
- ❌ **ZG_DUAL_ROLE_RECONNECTION.md** (구현 완료, 아카이브)
- **통합 결과:** ISO_13400 + mbedTLS 타이밍만 유지

### 3. VMG 관련 (3개 → 1개로 통합)
- ✅ **VMG_DYNAMIC_IP_MANAGEMENT.md** (아키텍처, 유지)
- ❌ **VMG_DYNAMIC_IP_QUICK_START.md** (Quick Start는 Management에 통합)
- ❌ **vmg_pqc_implementation.md** (PQC는 별도 문서로 이미 존재)
- **통합 결과:** Dynamic IP Management 1개만 유지

### 4. TC375 MCU 관련 (4개 → 2개로 통합)
- ✅ **firmware_architecture.md** (전체 아키텍처, 유지)
- ❌ **tc375_memory_map_corrected.md** (firmware_architecture에 통합)
- ❌ **tc375_porting.md** (초안, 삭제)
- ✅ **TC375_HSM_INTEGRATION.md** (HSM 특화, 유지)
- **통합 결과:** Firmware Architecture + HSM Integration

### 5. OTA 관련 (3개 → 1개로 통합)
- ✅ **ota_scenario_detailed.md** (사용자 제공, 최종본, 유지)
- ❌ **safe_ota_strategy.md** (중복, ota_scenario에 통합)
- ❌ **can_vs_ethernet_ota.md** (특수 케이스, 아카이브)
- **통합 결과:** OTA Scenario 1개만 유지

### 6. 진단 관련 (3개 → 2개로 통합)
- ✅ **REMOTE_DIAGNOSTICS_ARCHITECTURE.md** (아키텍처, 유지)
- ✅ **REMOTE_DIAGNOSTICS_USAGE.md** (사용 가이드, 유지)
- ✅ **UDS_IMPLEMENTATION_STATUS.md** (구현 상태, 유지)
- **통합 결과:** 3개 모두 유지 (각각 역할이 다름)

### 7. 시스템 전체 (4개 → 2개로 통합)
- ✅ **BOOT_AND_RUNTIME_COMPARISON.md** (부팅 비교, 유지)
- ✅ **zonal_gateway_architecture.md** (ZG 아키텍처, 유지)
- ❌ **data_management.md** (한글, 초안, 삭제)
- ✅ **unified_message_format.md** (메시지 포맷, 유지)
- **통합 결과:** 3개 유지

### 8. 참고 문서 (1개)
- ✅ **QUICK_REFERENCE.md** (빠른 참조, 유지)

---

## 최종 문서 구조 (24개 → 12개)

```
docs/
├── architecture/
│   └── system_overview.md                    ✅ 유지
│
├── bootloader/
│   └── tc375_bootloader_guide.md            ✅ 통합 (3→1)
│
├── network/
│   ├── ISO_13400_doip_specification.md      ✅ 유지
│   └── mbedtls_handshake_timing.md          ✅ 유지
│
├── mcu/
│   ├── tc375_firmware_architecture.md       ✅ 통합 (4→2)
│   └── tc375_hsm_integration.md             ✅ 유지
│
├── ota/
│   └── ota_scenario_detailed.md             ✅ 유지
│
├── diagnostics/
│   ├── remote_diagnostics_architecture.md   ✅ 유지
│   ├── remote_diagnostics_usage.md          ✅ 유지
│   └── uds_implementation_status.md         ✅ 유지
│
├── system/
│   ├── boot_and_runtime_comparison.md       ✅ 유지
│   ├── zonal_gateway_architecture.md        ✅ 유지
│   ├── vmg_dynamic_ip_management.md         ✅ 통합 (3→1)
│   └── unified_message_format.md            ✅ 유지
│
└── QUICK_REFERENCE.md                        ✅ 유지
```

---

## 삭제할 문서 (12개)

### 즉시 삭제
1. ❌ bootloader_implementation.md (중복)
2. ❌ dual_bootloader_ota.md (중복)
3. ❌ doip_tls_architecture.md (ISO 13400에 통합)
4. ❌ RECONNECTION_STRATEGY.md (구현 완료)
5. ❌ ZG_DUAL_ROLE_RECONNECTION.md (구현 완료)
6. ❌ VMG_DYNAMIC_IP_QUICK_START.md (Management에 통합)
7. ❌ vmg_pqc_implementation.md (중복)
8. ❌ tc375_memory_map_corrected.md (firmware에 통합)
9. ❌ tc375_porting.md (초안)
10. ❌ safe_ota_strategy.md (ota_scenario에 통합)
11. ❌ can_vs_ethernet_ota.md (특수 케이스)
12. ❌ data_management.md (초안)

---

## 통합 작업 상세

### 1. Bootloader 통합
**파일명:** `tc375_bootloader_guide.md`

**통합 내용:**
- bootloader_implementation.md의 기본 구조
- dual_bootloader_ota.md의 2-stage 설명
- tc375_infineon_bootloader_mapping.md의 공식 매핑

**섹션:**
1. Infineon TC375 Memory Map (공식)
2. Dual-Bank Architecture
3. Stage 1 (SSW) + Stage 2 (Application Bootloader)
4. OTA Update Flow
5. Fail-safe Mechanisms

### 2. Network 통합
**파일명:** `ISO_13400_doip_specification.md` (기존 파일 확장)

**추가 내용:**
- doip_tls_architecture.md의 TLS over DoIP 섹션 추가

### 3. VMG 통합
**파일명:** `vmg_dynamic_ip_management.md` (기존 파일 확장)

**추가 내용:**
- VMG_DYNAMIC_IP_QUICK_START.md의 Quick Start 섹션 추가

### 4. TC375 통합
**파일명:** `tc375_firmware_architecture.md` (기존 파일 확장)

**추가 내용:**
- tc375_memory_map_corrected.md의 메모리 맵 통합

### 5. OTA 통합
**파일명:** `ota_scenario_detailed.md` (기존 파일 확장)

**추가 내용:**
- safe_ota_strategy.md의 안전 전략 섹션 추가

---

## 이점

### Before (24개)
- 중복 내용 많음
- 찾기 어려움
- 유지보수 어려움
- 일관성 부족

### After (12개)
- ✅ 50% 감소
- ✅ 명확한 카테고리
- ✅ 중복 제거
- ✅ 일관된 구조
- ✅ 찾기 쉬움

---

## 실행 순서

1. **백업 생성** (안전을 위해)
2. **통합 문서 생성** (5개)
3. **불필요한 문서 삭제** (12개)
4. **README.md 업데이트**
5. **Git 커밋**

---

## 예상 소요 시간

- 통합 작업: 30분
- 삭제 작업: 5분
- README 업데이트: 10분
- 검증: 10분

**총 예상 시간: 약 1시간**

---

승인하시면 바로 시작하겠습니다!

