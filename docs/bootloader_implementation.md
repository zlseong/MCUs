# TC375 Bootloader 구현 가이드

## 🎯 부트로더의 역할

```
Power On → Reset Vector (0x00000000)
    ↓
Bootloader (0x00000000 - 0x0003FFFF)  ← 256 KB
    ↓
1. 하드웨어 최소 초기화 (Clock, Watchdog)
2. Bank Metadata 읽기
3. Bank 선택 (A or B)
4. CRC 검증
5. Signature 검증
6. Boot Count 체크
7. Application 점프 ───────────→ Bank A/B
                                      ↓
                              Application 실행
                              (UDS, OTA, TLS 등)
```

---

## 📁 **부트로더 프로젝트 구조**

### **파일 구성:**

```c
tc375_bootloader/
├── boot_main.c           // 부트로더 메인
├── flash_driver.c        // Flash 읽기/쓰기
├── crypto_verify.c       // CRC/서명 검증
├── bank_manager.c        // A/B 뱅크 관리
├── boot_config.h         // 설정
└── tc375_boot.ld         // 링커 스크립트 ★ 중요!
```

---

## 🔧 **1. 링커 스크립트 (메모리 맵 정의)**

### `tc375_boot.ld` - 부트로더 전용

```ld
/* TC375 Bootloader Linker Script */

MEMORY
{
    /* Bootloader는 Flash 시작부터 256KB */
    BOOT_FLASH (rx) : ORIGIN = 0x80000000, LENGTH = 256K
    
    /* Bootloader RAM */
    BOOT_RAM   (rw) : ORIGIN = 0x70000000, LENGTH = 64K
}

SECTIONS
{
    /* Reset Vector - 가장 먼저 실행되는 코드 */
    .boot_start : {
        KEEP(*(.boot_reset))
        . = ALIGN(4);
    } > BOOT_FLASH
    
    /* Bootloader 코드 */
    .text : {
        *(.text*)
    } > BOOT_FLASH
    
    /* 상수 데이터 */
    .rodata : {
        *(.rodata*)
    } > BOOT_FLASH
    
    /* 변수 */
    .data : {
        *(.data*)
    } > BOOT_RAM AT> BOOT_FLASH
    
    .bss : {
        *(.bss*)
    } > BOOT_RAM
}

/* Application Entry Points (나중에 점프할 주소) */
BANK_A_START = 0x80040000;  /* 256 KB 이후 */
BANK_B_START = 0x802C2000;  /* 2.5 MB 이후 */
```

### `tc375_app.ld` - 애플리케이션 전용

```ld
/* TC375 Application Linker Script */

MEMORY
{
    /* Bank A: 256KB ~ 2.75MB */
    APP_FLASH_A (rx) : ORIGIN = 0x80040000, LENGTH = 2560K
    
    /* Bank B: 2.75MB ~ 5.25MB */
    APP_FLASH_B (rx) : ORIGIN = 0x802C2000, LENGTH = 2560K
    
    /* Application RAM (부트로더 제외) */
    APP_RAM (rw) : ORIGIN = 0x70010000, LENGTH = 4M
}

SECTIONS
{
    /* Application Vector Table */
    .app_vectors : {
        KEEP(*(.app_vectors))
    } > APP_FLASH_A  /* or APP_FLASH_B */
    
    .text : { *(.text*) } > APP_FLASH_A
    .data : { *(.data*) } > APP_RAM
}
```

---

## 💻 **2. 부트로더 메인 코드**

### `boot_main.c` - 핵심 로직

```c
#include <stdint.h>
#include "Ifx_Cfg.h"
#include "IfxCpu.h"
#include "IfxFlash.h"

// Bank 메타데이터 주소
#define META_A_ADDR  0x80040000
#define META_B_ADDR  0x802C1000

// Bank 시작 주소
#define BANK_A_START 0x80041000
#define BANK_B_START 0x802C2000

// EEPROM에 저장된 부트 설정
#define BOOT_CFG_ADDR 0xAF000000

typedef enum {
    BANK_A = 0,
    BANK_B = 1
} BootBank;

typedef struct {
    uint32_t magic;           // 0xA5A5A5A5
    uint32_t version;
    uint32_t size;
    uint32_t crc32;
    uint8_t  signature[256]; // PQC signature
    uint32_t boot_count;
    uint8_t  valid;          // 0=Invalid, 1=Valid
} __attribute__((packed)) BankMetadata;

// ============================================================================
// Bootloader Main Entry
// ============================================================================

void __attribute__((section(".boot_reset"))) _reset(void) {
    // 1. CPU 초기화 (최소한)
    IfxCpu_setCoreMode(&MODULE_CPU0, IfxCpu_CoreMode_run);
    
    // 2. Watchdog 비활성화 (부트로더 동안만)
    IfxScuWdt_clearCpuEndinit();
    
    // 3. Clock 초기화 (최소한)
    initSystemClock();
    
    // 4. 부트로더 메인
    bootloader_main();
}

void bootloader_main(void) {
    // ========================================
    // Phase 1: Bank 메타데이터 읽기
    // ========================================
    
    BankMetadata* meta_a = (BankMetadata*)META_A_ADDR;
    BankMetadata* meta_b = (BankMetadata*)META_B_ADDR;
    
    // EEPROM에서 저장된 active bank 읽기
    BootBank active_bank = readActiveBank();
    
    BankMetadata* active_meta = (active_bank == BANK_A) ? meta_a : meta_b;
    BankMetadata* backup_meta = (active_bank == BANK_A) ? meta_b : meta_a;
    
    // ========================================
    // Phase 2: Boot Count 증가 (Fail-safe)
    // ========================================
    
    active_meta->boot_count++;
    
    if (active_meta->boot_count >= 3) {
        // 3번 연속 실패 → 자동 롤백!
        debug_print("[Boot] Too many failures, rollback!\n");
        
        if (backup_meta->valid == 1 && backup_meta->magic == 0xA5A5A5A5) {
            // Fallback bank로 전환
            active_bank = (active_bank == BANK_A) ? BANK_B : BANK_A;
            writeActiveBank(active_bank);
            
            // Fallback metadata 갱신
            backup_meta->boot_count = 0;
            
            // 재부팅
            systemReset();
        } else {
            // Catastrophic failure!
            enterRecoveryMode();  // USB DFU
            while(1);  // Never return
        }
    }
    
    // ========================================
    // Phase 3: Firmware 검증
    // ========================================
    
    // Magic number 체크
    if (active_meta->magic != 0xA5A5A5A5) {
        debug_print("[Boot] Invalid magic\n");
        tryFallback();
    }
    
    // CRC32 검증
    uint32_t calc_crc = calculateCRC32(
        active_bank == BANK_A ? BANK_A_START : BANK_B_START,
        active_meta->size
    );
    
    if (calc_crc != active_meta->crc32) {
        debug_print("[Boot] CRC failed: calc=%08X, expect=%08X\n", 
                   calc_crc, active_meta->crc32);
        tryFallback();
    }
    
    // PQC 서명 검증 (Dilithium3)
    if (!verifyDilithium3Signature(
            active_bank == BANK_A ? BANK_A_START : BANK_B_START,
            active_meta->size,
            active_meta->signature)) {
        debug_print("[Boot] Signature verification failed\n");
        tryFallback();
    }
    
    // ========================================
    // Phase 4: Application 점프
    // ========================================
    
    debug_print("[Boot] Booting Bank %c v%d\n",
               active_bank == BANK_A ? 'A' : 'B',
               active_meta->version);
    
    // Boot count 리셋 (성공적 검증)
    active_meta->boot_count = 0;
    
    // Application 시작 주소
    uint32_t app_start = (active_bank == BANK_A) ? BANK_A_START : BANK_B_START;
    
    // Application으로 점프!
    jumpToApplication(app_start);
    
    // 여기는 도달하면 안 됨
    while(1);
}

// ============================================================================
// Helper Functions
// ============================================================================

void jumpToApplication(uint32_t app_addr) {
    // 1. Application의 Vector Table 설정
    uint32_t* vector_table = (uint32_t*)app_addr;
    uint32_t stack_pointer = vector_table[0];  // SP
    uint32_t reset_handler = vector_table[1];  // PC
    
    // 2. Watchdog 재활성화
    IfxScuWdt_setCpuEndinit();
    
    // 3. 스택 포인터 설정
    __asm volatile("mov.a SP, %0" : : "d"(stack_pointer));
    
    // 4. Application Reset Handler 실행
    void (*app_entry)(void) = (void(*)(void))reset_handler;
    app_entry();
    
    // Never return
}

void tryFallback(void) {
    BootBank current = readActiveBank();
    BootBank fallback = (current == BANK_A) ? BANK_B : BANK_A;
    
    BankMetadata* fallback_meta = (fallback == BANK_A) ? 
        (BankMetadata*)META_A_ADDR : (BankMetadata*)META_B_ADDR;
    
    // Fallback 유효성 확인
    if (fallback_meta->valid == 1 && fallback_meta->magic == 0xA5A5A5A5) {
        debug_print("[Boot] Switching to Bank %c\n", 
                   fallback == BANK_A ? 'A' : 'B');
        
        writeActiveBank(fallback);
        systemReset();  // 재부팅
    } else {
        // 양쪽 다 실패!
        enterRecoveryMode();
    }
}

void enterRecoveryMode(void) {
    debug_print("[Boot] RECOVERY MODE - Connect USB for firmware upload\n");
    
    // USB DFU (Device Firmware Update) 모드 진입
    // 사용자가 USB로 펌웨어를 업로드할 때까지 대기
    
    while(1) {
        handleUsbDfu();  // USB 연결 대기
    }
}
```

---

## 🔗 **지금까지 만든 것들과의 관계**

### **완전한 시스템 흐름:**

```
┌─────────────────────────────────────────────┐
│  1. Bootloader (새로 만들 것)               │
│     - TC375에서만 실행                       │
│     - 가장 먼저 실행                         │
│     - Application 검증 & 점프                │
└─────────────────┬───────────────────────────┘
                  │ 점프
                  ▼
┌─────────────────────────────────────────────┐
│  2. Application (tc375_simulator 기반)      │
│                                             │
│     ┌─────────────────┐                    │
│     │  UDS Handler    │ ← OTA 명령 수신    │
│     └────────┬────────┘                    │
│              ▼                              │
│     ┌─────────────────┐                    │
│     │  OTA Manager    │ ← Bank B에 쓰기    │
│     └────────┬────────┘                    │
│              ▼                              │
│     ┌─────────────────┐                    │
│     │  TLS Client     │ ← Gateway 통신     │
│     └────────┬────────┘                    │
└──────────────┼─────────────────────────────┘
               │ TLS
               ▼
┌─────────────────────────────────────────────┐
│  3. Gateway (vehicle_gateway) ✅            │
│     - OTA 파일 전송                         │
│     - UDS 명령 전송                         │
└─────────────────────────────────────────────┘
```

### **OTA 전체 시나리오:**

```
[Gateway] OTA 시작 명령
    ↓
[Application] UDS RequestDownload 수신
    ↓
[Application] Bank B 영역 erase
    ↓
[Gateway] 펌웨어 블록 전송 (UDS TransferData)
    ↓
[Application] Bank B에 Flash 쓰기
    ↓
[Application] CRC/Signature 검증
    ↓
[Application] Active Bank를 B로 변경
    ↓
[Application] 재부팅 명령 (UDS ECU Reset)
    ↓
[Bootloader] 다시 실행됨!
    ↓
[Bootloader] Bank B 검증 → OK
    ↓
[Bootloader] Bank B로 점프
    ↓
[New Application] 실행!
```

---

## 🔨 **부트로더 빌드 방법**

### **AURIX Development Studio (ADS)에서:**

```bash
# 1. 새 프로젝트 생성
File → New → AURIX Project
  - Name: TC375_Bootloader
  - Target: TC375TP
  - Template: Empty Project

# 2. 소스 파일 추가
boot_main.c
flash_driver.c
crypto_verify.c

# 3. 링커 스크립트 설정
Project → Properties → C/C++ Build → Settings
  → TriCore C Linker → General
    → Linker Script: tc375_boot.ld

# 4. 빌드
Build Project
  → 출력: bootloader.elf, bootloader.hex

# 5. Flash 프로그래밍
Run → Debug Configurations
  → Flash bootloader.hex to 0x80000000
```

---

## 📊 **메모리 분할 (최종)**

```
TC375 Flash (6 MB):

0x80000000  ┌─────────────────────────┐
            │  Bootloader             │  256 KB
            │  - boot_main.c          │  ← 별도 프로젝트
            │  - flash_driver.c       │
0x80040000  ├─────────────────────────┤
            │  Bank A Metadata        │  4 KB
0x80041000  ├─────────────────────────┤
            │  Bank A Application     │  2.5 MB
            │  - main.cpp             │  ← tc375_simulator 기반
            │  - uds_handler.cpp      │  ✅ 이미 만듦
            │  - ota_manager.cpp      │  ✅ 이미 만듦
            │  - tls_client.cpp       │  ✅ 이미 만듦
0x802C1000  ├─────────────────────────┤
            │  Bank B Metadata        │  4 KB
0x802C2000  ├─────────────────────────┤
            │  Bank B Application     │  2.5 MB
            │  (Same code, new version)│
0x80542000  ├─────────────────────────┤
            │  Config / Logs          │  768 KB
0x80600000  └─────────────────────────┘
```

---

## 🔄 **프로젝트 간 의존성**

### **1. Bootloader 프로젝트**
```c
// 독립적으로 빌드
// Application과 분리됨
// 출력: bootloader.hex (256 KB)
```

### **2. Application 프로젝트**
```cpp
// tc375_simulator 코드 기반
// Bootloader에 의존하지 않음
// 출력: application_a.hex (2.5 MB)
```

### **3. 빌드 & 플래싱 순서**
```bash
# Step 1: 부트로더 빌드 & 플래시 (한 번만!)
cd tc375_bootloader
./build_boot.sh
flash_tool --addr 0x80000000 bootloader.hex

# Step 2: Application 빌드 & 플래시
cd tc375_application
./build.sh
flash_tool --addr 0x80041000 application.hex  # Bank A

# 이후 OTA는 Gateway를 통해!
```

---

## 💡 **Mac 시뮬레이터와의 관계**

### **현재 tc375_simulator는:**

```cpp
✅ Application 부분만 시뮬레이션
  - UDS Handler
  - OTA Manager
  - TLS Client
  - Protocol

❌ Bootloader는 시뮬레이션 불필요
  (MCU 하드웨어에만 필요)
```

### **테스트 방법:**

```
Mac에서:
  tc375_simulator 실행
  → Gateway와 통신 테스트
  → OTA 프로토콜 검증
  → UDS 메시지 테스트

TC375에서:
  Bootloader + Application 플래싱
  → 실제 A/B 뱅크 부팅 테스트
  → 롤백 시나리오 테스트
```

---

## 🎯 **다음 단계**

### **지금 할 수 있는 것:**

1. ✅ **tc375_simulator** - Gateway 통신 프로토콜 검증 (Mac)
2. ⏳ **tc375_bootloader** - 별도 프로젝트로 생성 (ADS 필요)
3. ⏳ **tc375_application** - simulator 코드를 TC375로 포팅

### **만들어드릴까요?**

부트로더 템플릿 프로젝트를 지금 만들어드릴까요? 
(실제 빌드는 Windows + ADS에서 해야 하지만, 코드는 미리 준비 가능!)
