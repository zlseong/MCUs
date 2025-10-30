# Bootloader Build Process 설명

## 질문: Common 파일을 링커를 통해 A/B 메모리에 각각 넣는 건가?

**답: 맞습니다!**

## 빌드 프로세스

### 1. 소스 코드 (Common)

**동일한 코드 파일:**
```
tc375_bootloader/common/
├── boot_common.h       # 헤더 (제어 로직 정의)
├── boot_common.c       # 구현 (공통 함수)
├── doip_client.h       # DoIP 클라이언트
├── doip_client.c
├── uds_handler.h       # UDS 처리
└── uds_handler.c
```

**bootloader/ (메인 코드):**
```
tc375_bootloader/bootloader/
├── stage2_main.c       # Stage 2 메인 (동일한 소스)
├── stage2a_linker.ld   # A 메모리 링커 스크립트
└── stage2b_linker.ld   # B 메모리 링커 스크립트
```

### 2. 링커 스크립트 차이

#### Stage 2A Linker (0x800A1000)
```ld
MEMORY
{
    STAGE2_FLASH (rx) : ORIGIN = 0x800A1000, LENGTH = 196K  ← Region A
    STAGE2_RAM (rwx)  : ORIGIN = 0x70010000, LENGTH = 128K
}
```

#### Stage 2B Linker (0x820A1000)
```ld
MEMORY
{
    STAGE2_FLASH (rx) : ORIGIN = 0x820A1000, LENGTH = 196K  ← Region B
    STAGE2_RAM (rwx)  : ORIGIN = 0x70010000, LENGTH = 128K  ← RAM 공유
}
```

**차이점:**
- **FLASH 주소만 다름** (A: 0x800A1000, B: 0x820A1000)
- **RAM은 동일** (실행 시 하나만 활성)
- **소스 코드는 완전히 동일**

### 3. 빌드 명령

```bash
# Stage 2A 빌드 (Region A로 링크)
tricore-gcc \
    -c bootloader/stage2_main.c \
    -c common/boot_common.c \
    -c common/doip_client.c \
    -c common/uds_handler.c \
    -T bootloader/stage2a_linker.ld \
    -o stage2a.elf

# Stage 2B 빌드 (Region B로 링크)
tricore-gcc \
    -c bootloader/stage2_main.c \    # ← 동일한 소스!
    -c common/boot_common.c \         # ← 동일한 소스!
    -c common/doip_client.c \         # ← 동일한 소스!
    -c common/uds_handler.c \         # ← 동일한 소스!
    -T bootloader/stage2b_linker.ld \ # ← 링커만 다름!
    -o stage2b.elf
```

### 4. 결과

```
┌────────────────────────────────────────────────────┐
│              TC375 PFLASH (6 MB)                   │
├────────────────────────────────────────────────────┤
│ Region A (0x80000000 - 0x81FFFFFF)                │
│  ├─ 0x800A1000  Stage 2A ← stage2a.elf           │
│  │   (동일한 코드, Region A 주소로 빌드됨)          │
│  └─ 0x800D3000  Application A                     │
├────────────────────────────────────────────────────┤
│ Region B (0x82000000 - 0x83FFFFFF)                │
│  ├─ 0x820A1000  Stage 2B ← stage2b.elf           │
│  │   (동일한 코드, Region B 주소로 빌드됨)          │
│  └─ 0x820D3000  Application B                     │
└────────────────────────────────────────────────────┘
```

## 핵심 개념

### ✅ 동일한 것
- **소스 코드 파일** (stage2_main.c, boot_common.c 등)
- **기능 로직** (검증, 점프, OTA 처리)
- **알고리즘** (CRC32, Signature 검증)

### ❌ 다른 것
- **링커 스크립트** (stage2a_linker.ld vs stage2b_linker.ld)
- **배치 주소** (Region A: 0x80... vs Region B: 0x82...)
- **바이너리 파일** (stage2a.elf vs stage2b.elf)

## 왜 이렇게 하는가?

### 1. 코드 재사용
```c
// 동일한 소스 코드
// common/boot_common.c
void verify_application(uint32_t addr) {
    // Region A든 B든 동일한 검증 로직
    BootMetadata* meta = (BootMetadata*)addr;
    if (meta->magic != MAGIC_NUMBER) return false;
    // ...
}
```

### 2. OTA 안전성
```
Active: Stage 2A (Region A)
Backup: Stage 2B (Region B) ← 동일한 코드, 다른 위치

OTA 업데이트:
1. Stage 2B 업데이트 (Stage 2A 실행 중)
2. 검증 후 Region B로 전환
3. 실패 시 Region A로 롤백
```

### 3. 메모리 효율
```
RAM (192 KB):
├─ 0x70000000  SSW 사용 (4 KB)
├─ 0x70010000  Stage 2 사용 (128 KB) ← A/B 공유
└─ 0x70030000  Application 사용 (60 KB)

FLASH (6 MB):
├─ Region A (3 MB) - Active
└─ Region B (3 MB) - Backup
```

## 빌드 흐름도

```
┌──────────────────────────────────────────────────┐
│           Source Files (Common)                   │
│  - stage2_main.c                                 │
│  - boot_common.c                                 │
│  - doip_client.c                                 │
│  - uds_handler.c                                 │
└───────────────┬──────────────────────────────────┘
                │
                ├─────────────────┬────────────────┐
                │                 │                │
        ┌───────▼───────┐ ┌──────▼──────┐        │
        │   Compile     │ │   Compile   │        │
        │   stage2a     │ │   stage2b   │        │
        └───────┬───────┘ └──────┬──────┘        │
                │                 │                │
        ┌───────▼───────┐ ┌──────▼──────┐        │
        │ Link with     │ │ Link with   │        │
        │ stage2a_      │ │ stage2b_    │        │
        │ linker.ld     │ │ linker.ld   │        │
        └───────┬───────┘ └──────┬──────┘        │
                │                 │                │
        ┌───────▼───────┐ ┌──────▼──────┐        │
        │  stage2a.elf  │ │ stage2b.elf │        │
        │  @ 0x800A1000 │ │ @ 0x820A1000│        │
        └───────┬───────┘ └──────┬──────┘        │
                │                 │                │
        ┌───────▼───────┐ ┌──────▼──────┐        │
        │  Flash to     │ │ Flash to    │        │
        │  Region A     │ │ Region B    │        │
        └───────────────┘ └─────────────┘        │
                                                  │
┌─────────────────────────────────────────────────▼─┐
│              TC375 PFLASH                          │
│  Region A (0x80...)     Region B (0x82...)        │
│  ├─ Stage 2A (196KB)   ├─ Stage 2B (196KB)       │
│  └─ App A (2.1MB)      └─ App B (2.1MB)          │
└────────────────────────────────────────────────────┘
```

## Application도 동일

Application도 같은 방식:

```bash
# Application A 빌드
tricore-gcc \
    -c ecu_node.c \
    -c doip_client.c \
    -T app_a_linker.ld \  # Region A 링커
    -o app_a.elf

# Application B 빌드
tricore-gcc \
    -c ecu_node.c \       # ← 동일한 소스!
    -c doip_client.c \    # ← 동일한 소스!
    -T app_b_linker.ld \  # ← 링커만 다름!
    -o app_b.elf
```

## 실제 예제

### Common 파일 (boot_common.c)
```c
// 동일한 소스 코드
#include "boot_common.h"

uint32_t calculate_crc32(const uint8_t* data, uint32_t length) {
    // CRC32 계산 (Region A/B 무관)
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

bool verify_metadata(uint32_t addr) {
    BootMetadata* meta = (BootMetadata*)addr;
    return (meta->magic == MAGIC_NUMBER);
}
```

### Stage 2A Linker
```ld
MEMORY {
    STAGE2_FLASH : ORIGIN = 0x800A1000  ← Region A
}
```

### Stage 2B Linker
```ld
MEMORY {
    STAGE2_FLASH : ORIGIN = 0x820A1000  ← Region B
}
```

### 결과
```
Region A: 0x800A1000에 calculate_crc32() 배치
Region B: 0x820A1000에 calculate_crc32() 배치 (동일한 코드)
```

## 정리

### ✅ Common 파일의 역할
- **제어 로직 구현** (boot_common.c, doip_client.c 등)
- **두 번 빌드됨** (Region A용, Region B용)
- **링커만 다름** (배치 주소만 변경)

### 🔧 링커 스크립트의 역할
- **메모리 주소 지정** (A: 0x80..., B: 0x82...)
- **RAM 영역 공유** (실행 시 하나만 사용)
- **바이너리 최종 배치**

### 📦 최종 결과
```
동일한 소스 → 두 개의 링커 → 두 개의 바이너리 → A/B 메모리
```

**네, 맞습니다! 동일한 Common 파일을 링커를 통해 A 메모리와 B 메모리에 각각 배치합니다!** ✅

