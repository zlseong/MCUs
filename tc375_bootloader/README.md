# TC375 2-Stage Dual Bootloader

## 🎯 개요

부트로더도 OTA로 업데이트 가능한 2단계 부트 시스템입니다.

## 🏗️ 아키텍처

### 3-Tier Boot System

```
┌──────────────────────────────────┐
│  Stage 1 (Primary Bootloader)   │  64 KB
│  - 절대 불변 (ROM-like)          │  ← 절대 업데이트 안 함
│  - Stage 2 검증 & 선택           │
│  - Minimal 기능                  │
└─────────────┬────────────────────┘
              │ 점프
       ┌──────┴──────┐
       ▼              ▼
┌──────────┐    ┌──────────┐
│ Stage 2A │    │ Stage 2B │        188 KB each
│ (Active) │    │ (Backup) │        ← OTA 업데이트 가능!
└─────┬────┘    └─────┬────┘
      │ 점프          │ 점프
      ▼              ▼
┌──────────┐    ┌──────────┐
│  App A   │    │  App B   │        2.5 MB each
│ (Active) │    │ (Backup) │        ← OTA 업데이트 가능!
└──────────┘    └──────────┘
```

## 📊 메모리 맵

```
0x80000000  Stage 1 Bootloader     64 KB   [불변]
0x80010000  Stage 2A Metadata       4 KB
0x80011000  Stage 2A Bootloader   188 KB   [OTA 가능]
0x80040000  Stage 2B Metadata       4 KB
0x80041000  Stage 2B Bootloader   188 KB   [OTA 가능]
0x80070000  App A Metadata          4 KB
0x80071000  App A                 2.4 MB   [OTA 가능]
0x802F1000  App B Metadata          4 KB
0x802F2000  App B                 2.4 MB   [OTA 가능]
0x80572000  Config + Logs         568 KB
```

## 🔄 부팅 흐름

### 정상 부팅

```
Power On
  ↓
Stage 1 (항상)
  ├─ Stage 2A/2B 검증
  ├─ Boot count 체크
  └─ 점프 → Stage 2A
       ↓
Stage 2A
  ├─ App A/B 검증
  ├─ CRC + Signature 검증
  ├─ Boot count 체크
  └─ 점프 → App A
       ↓
Application A 실행!
```

### Fail-safe (3단계)

```
Power On → Stage 1
  ↓ (Stage 2A CRC 실패)
Stage 1 → Stage 2B
  ↓ (정상)
Stage 2B
  ↓ (App A 부팅 3회 실패)
Stage 2B → App B
  ↓
Application B 실행 (복구!)
```

## 🚀 OTA 업데이트 시나리오

### 시나리오 1: Application OTA (일반적)

```
현재: Stage 2A + App A
목표: App B로 업데이트

1. App A (실행 중)
   └─ OTA Manager: Bank B에 새 펌웨어 다운로드

2. App A
   └─ 검증 후 active_app = B로 변경

3. Reboot

4. Stage 1 → Stage 2A (동일)

5. Stage 2A
   └─ App B 검증 → 점프

6. App B 실행! (업데이트 완료)
```

### 시나리오 2: Stage 2 Bootloader OTA (고급)

```
현재: Stage 2A + App A
목표: Stage 2B로 업데이트

1. App A (실행 중)
   └─ 특수 OTA: Stage 2B 영역에 새 부트로더 다운로드

2. App A
   └─ 검증 후 stage2_active = B로 변경

3. Reboot

4. Stage 1
   └─ Stage 2B 검증 → 점프

5. Stage 2B (새 부트로더!)
   └─ App A/B 선택

6. 정상 동작 (부트로더 업데이트 완료!)
```

### 시나리오 3: 동시 업데이트 (풀 업데이트)

```
Stage 2B + App B 동시 업데이트:

1. 현재: Stage 2A + App A
2. Stage 2B 다운로드 → 검증
3. App B 다운로드 → 검증
4. EEPROM: stage2_active=B, app_active=B
5. Reboot
6. Stage 1 → Stage 2B → App B
7. 완전히 새로운 시스템!
```

## 🛡️ Fail-Safe 메커니즘

### Level 1: Stage 1 (절대 안전)

```c
if (stage2_boot_count >= 3) {
    // Stage 2A 실패 → Stage 2B
    switch_to_stage2_fallback();
}
```

### Level 2: Stage 2 (애플리케이션 보호)

```c
if (app_boot_count >= 3) {
    // App A 실패 → App B
    switch_to_app_fallback();
}
```

### Level 3: 긴급 복구

```c
if (stage2_a_invalid && stage2_b_invalid) {
    // 양쪽 Stage 2 다 실패
    enter_usb_dfu_mode();  // USB로 복구
}
```

## 🔨 빌드 방법

### Stage 1 빌드 (한 번만!)

```bash
cd tc375_bootloader/stage1
tricore-gcc -c stage1_main.c -o stage1_main.o
tricore-ld -T stage1_linker.ld -o stage1_boot.elf stage1_main.o
tricore-objcopy -O ihex stage1_boot.elf stage1_boot.hex

# Flash to 0x80000000 (한 번만!)
```

### Stage 2 빌드 (A와 B 따로)

```bash
# Stage 2A
cd tc375_bootloader/stage2
tricore-gcc -DSTAGE2_A -c stage2_main.c -o stage2a_main.o
tricore-ld -T stage2_linker.ld -o stage2a_boot.elf stage2a_main.o
tricore-objcopy -O ihex stage2a_boot.elf stage2a_boot.hex

# Stage 2B
tricore-gcc -DSTAGE2_B -c stage2_main.c -o stage2b_main.o  
tricore-ld -T stage2_linker.ld -o stage2b_boot.elf stage2b_main.o
tricore-objcopy -O ihex stage2b_boot.elf stage2b_boot.hex
```

## 📝 플래싱 순서

### 최초 설치 (Factory)

```bash
# 1. Stage 1 (한 번만!)
flash_tool write 0x80000000 stage1_boot.hex

# 2. Stage 2A (초기)
flash_tool write 0x80011000 stage2a_boot.hex

# 3. Stage 2B (백업, 동일)
flash_tool write 0x80041000 stage2a_boot.hex  # 처음엔 같은 것

# 4. Application A
flash_tool write 0x80071000 application.hex

# 5. EEPROM 초기화
flash_tool write_eeprom 0xAF000000 boot_config_init.bin
```

### OTA로 Stage 2 업데이트

```
Gateway → Application → UDS RequestDownload(Stage2B 영역)
                     → TransferData (Stage 2B 코드)
                     → Verify
                     → Set stage2_active = B
                     → Reboot
                     → Stage 1 → Stage 2B (새 버전!)
```

## 🔐 보안

### Stage 1 Protection

```c
// Stage 1 영역을 Write-protect
IfxFlash_setProtection(
    STAGE1_START, 
    STAGE1_START + STAGE1_SIZE,
    IfxFlash_Protection_write  // 쓰기 금지!
);
```

### Stage 2 & App PQC Signature

```
모든 펌웨어는 Dilithium3 서명 필수:
  - Stage 2A/2B
  - App A/B
  
서명 검증 실패 → Fallback
```

## ⚠️ 주의사항

### 절대 하면 안 되는 것

```
❌ Stage 1 업데이트 시도
❌ Stage 2A, 2B 동시 업데이트
❌ 검증 없이 뱅크 전환
```

### 안전한 업데이트 순서

```
✅ Step 1: App B 업데이트 → 테스트
✅ Step 2: Stage 2B 업데이트 → 테스트
✅ Step 3: App A 업데이트
✅ Step 4: Stage 2A 업데이트
```

→ 항상 한쪽 백업 유지!

## 📈 장점 정리

### vs 단일 부트로더

| 기능 | 단일 Boot | 2-Stage Boot |
|------|-----------|--------------|
| 부트로더 업데이트 | ❌ 불가능 | ✅ 가능 |
| Fail-safe 단계 | 2단계 | 3단계 |
| 복잡도 | 낮음 | 높음 |
| 메모리 오버헤드 | 256 KB | 448 KB |
| 안전성 | 높음 | 매우 높음 |
| 유연성 | 보통 | 매우 높음 |

## 🎯 사용 사례

### 2-Stage가 필요한 경우:

- ✅ 10년+ 장기 필드 운영
- ✅ 부트로더 버그 발견 가능성
- ✅ 새로운 보안 요구사항 (PQC 업그레이드 등)
- ✅ 하드웨어 리비전 대응
- ✅ 최대 안전성 요구

→ **당신의 경우: PQC 업데이트가 필요할 수 있으므로 좋은 선택!** 👍

