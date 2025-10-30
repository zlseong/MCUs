# Dual Bank OTA 핵심 정리

## 질문: 누가 Flash를 지우고 프로그램하는가?

### 답: **Application이 담당합니다!**

## Single Bank vs Dual Bank

### Single Bank (전통적인 방식)
```
┌─────────────────────────────────────┐
│ 0x80000000  Bootloader (불변)       │
│             ↓                        │
│ 0x80010000  Application (OTA 대상)  │
└─────────────────────────────────────┘

프로세스:
1. Application: OTA 패키지 다운로드
2. Reboot to Bootloader
3. Bootloader: Application Erase  ← Bootloader가!
4. Bootloader: Application Program ← Bootloader가!
5. Bootloader: Application 실행
```

**문제점:**
- Bootloader 모드 진입 필요 (차량 멈춤)
- 다운타임 발생
- 실패 시 복구 어려움

### Dual Bank (TC375 방식)
```
┌────────────────────────────────────────┐
│ Region A (Active)                      │
│  ├─ 0x800A1000  Bootloader A (실행)   │
│  └─ 0x800D3000  Application A (실행)  │
│                 ↓ Flash Write 가능     │
└────────────────────────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│ Region B (Inactive - OTA 대상)         │
│  ├─ 0x820A1000  Bootloader B ← Erase  │
│  └─ 0x820D3000  Application B ← Erase │
└────────────────────────────────────────┘

프로세스:
1. Application A: OTA 패키지 다운로드 (실행 중)
2. Application A: Region B Erase  ← Application이!
3. Application A: Region B Program ← Application이!
4. Application A: Boot Config 업데이트
5. Reboot
6. Bootloader B: Application B 검증 ← Bootloader는 검증만!
7. Bootloader B: Application B 실행
```

**장점:**
- 차량 정상 동작 유지 (백그라운드 업데이트)
- 최소 다운타임 (Reboot만)
- 안전한 롤백 (실패 시 Region A로)

## 왜 Application이 담당하는가?

### 1. 자기 자신을 지울 수 없다!

```c
// ❌ 불가능: Bootloader A가 자기 자신 Erase
void bootloader_a_erase_self(void) {
    Flash_Erase(0x800A1000, 196*1024);  // 실행 중인 코드!
    // → 즉시 크래시!
}

// ✅ 가능: Application A가 Region B Erase
void application_a_erase_region_b(void) {
    Flash_Erase(0x820A1000, 196*1024);  // 비활성 Region
    // → OK! Region B는 완전히 분리됨
}
```

### 2. TC375 하드웨어 보호

```
TC375 MPU (Memory Protection Unit):
├─ Region A 실행 중
│   ├─ Region A: Read/Execute Only (Write 금지)
│   └─ Region B: Read/Write 가능 (Inactive)
│
└─ Region B 실행 중
    ├─ Region A: Read/Write 가능 (Inactive)
    └─ Region B: Read/Execute Only (Write 금지)
```

**하드웨어가 실행 중인 Region 보호!**

## 코드 예제

### Application이 OTA 처리

```c
/**
 * end_node_ecu/tc375/src/ota_handler.c
 * 
 * Application A가 실행 중 (@ 0x800D3000, Region A)
 */

bool ota_install(void) {
    // 1. 현재 Region 감지
    BootBank current = detect_current_region();  // → BANK_A
    
    debug_print("[OTA] Current: Region A\n");
    debug_print("[OTA] Target:  Region B\n");
    
    // 2. Region B Bootloader Erase & Program
    Flash_Erase(REGION_B_BOOT_START, REGION_B_BOOT_SIZE);
    Flash_Write(REGION_B_BOOT_START, bootloader_data, bootloader_size);
    
    debug_print("[OTA] Bootloader B updated\n");
    
    // 3. Region B Application Erase & Program
    Flash_Erase(REGION_B_APP_START, REGION_B_APP_SIZE);
    Flash_Write(REGION_B_APP_START, app_data, app_size);
    
    debug_print("[OTA] Application B updated\n");
    
    // 4. Boot Config 업데이트 (Region B로 전환)
    BootConfig cfg;
    cfg.active_region = BANK_B;
    Flash_Write(BOOT_CFG_EEPROM, &cfg, sizeof(cfg));
    
    debug_print("[OTA] Boot config updated. Ready to reboot!\n");
    
    return true;
}
```

### Bootloader는 검증만

```c
/**
 * end_node_ecu/tc375/bootloader/stage2/stage2_main.c
 * 
 * Bootloader B가 실행됨 (@ 0x820A1000, Region B)
 */

void stage2_main(void) {
    debug_print("[Bootloader B] Started\n");
    
    // 1. Application B 검증 (Flash 작업 없음!)
    if (!verify_application(REGION_B_APP_META)) {
        debug_print("[Bootloader B] Verification failed!\n");
        switch_to_region_a();  // Rollback
        system_reset();
    }
    
    debug_print("[Bootloader B] Verification OK\n");
    
    // 2. Application B 실행
    jump_to_application(REGION_B_APP_START);
}
```

## 전체 OTA 흐름

### Phase 1: 다운로드 & 설치 (Application 담당)
```
[Application A 실행 중 @ Region A]

1. VMG → ZG → ECU: OTA 패키지 전송 (DoIP + mbedTLS)
2. Application A: OTA 패키지 수신
   └─ ota_receive_chunk()
3. Application A: Region B Bootloader Erase
   └─ Flash_Erase(0x820A1000, 196KB)
4. Application A: Region B Bootloader Program
   └─ Flash_Write(0x820A1000, bootloader_data, size)
5. Application A: Region B Application Erase
   └─ Flash_Erase(0x820D3000, 2.1MB)
6. Application A: Region B Application Program
   └─ Flash_Write(0x820D3000, app_data, size)
7. Application A: Boot Config 업데이트
   └─ cfg.active_region = BANK_B
8. Application A: Reboot 요청
   └─ system_reset()
```

### Phase 2: 부팅 & 검증 (Bootloader 담당)
```
[Reboot]

9. SSW: Boot Config 읽기
   └─ cfg.active_region = BANK_B
10. SSW: Bootloader B 검증
11. SSW: Bootloader B로 점프 (@ 0x820A1000)

[Bootloader B 실행 중 @ Region B]

12. Bootloader B: Application B Metadata 읽기
13. Bootloader B: CRC32 검증
14. Bootloader B: Signature 검증 (Dilithium3)
15. Bootloader B: Application B로 점프 (@ 0x820D3000)

[Application B 실행 중 @ Region B]

16. Application B: 정상 동작 확인
17. Application B: Boot Count 리셋
18. Application B: OTA 결과 보고 (VMG → Server)
```

## 역할 비교

| 작업 | Single Bank | Dual Bank (TC375) |
|------|-------------|-------------------|
| **OTA 다운로드** | Application | Application |
| **Flash Erase** | **Bootloader** | **Application** |
| **Flash Program** | **Bootloader** | **Application** |
| **검증** | Bootloader | Bootloader |
| **실행** | Bootloader | Bootloader |
| **대상** | 자기 자신 (위험) | 다른 Region (안전) |

## 파일 구조

### Application (Flash 작업 담당)
```
end_node_ecu/tc375/
├── include/
│   ├── ota_handler.h       # OTA 핸들러 인터페이스
│   └── flash_driver.h      # Flash 하드웨어 제어
└── src/
    ├── ecu_node.c          # Main Application
    ├── ota_handler.c       # OTA 처리 (Flash Erase/Program) ← 여기!
    └── flash_driver.c      # Flash 드라이버 (TC375 하드웨어)
```

### Bootloader (검증만 담당)
```
end_node_ecu/tc375/bootloader/
├── stage2/
│   └── stage2_main.c       # 검증 & 점프만 ← Flash 작업 없음!
└── common/
    └── boot_common.h       # 검증 함수 (CRC32, Signature)
```

## 메모리 맵

### 전체 구조
```
TC375 PFLASH (6 MB)
├─────────────────────────────────────────────────┐
│ Region A (3 MB) @ 0x80000000                    │
│   ├─ 0x80000100  SSW (64 KB)                    │
│   ├─ 0x800A1000  Bootloader A (196 KB)          │
│   │   └─ stage2_main.c (검증 & 점프)            │
│   └─ 0x800D3000  Application A (2.1 MB)         │
│       └─ ota_handler.c (Flash Erase/Program)    │
│           ↓                                      │
│           └─ Region B로 Flash Write 가능!       │
├─────────────────────────────────────────────────┤
│ Region B (3 MB) @ 0x82000000                    │
│   ├─ 0x82000100  SSW (64 KB)                    │
│   ├─ 0x820A1000  Bootloader B (196 KB) ← OTA   │
│   └─ 0x820D3000  Application B (2.1 MB) ← OTA  │
└─────────────────────────────────────────────────┘
```

## 안전성

### 1. 하드웨어 보호
```
TC375 MPU:
- Region A 실행 중 → Region A Write 불가 (Hardware Exception)
- Region A 실행 중 → Region B Write 가능
```

### 2. 롤백 메커니즘
```
OTA 실패 시:
1. Bootloader B: Application B 검증 실패
2. Boot Count 증가 (region_b_boot_cnt++)
3. MAX_BOOT_ATTEMPTS (3회) 초과 시:
   └─ Boot Config 업데이트 (Region A로 전환)
   └─ system_reset()
4. Bootloader A: Application A 실행 (안전한 버전)
```

### 3. 원자성 보장
```
Metadata 순서:
1. Bootloader Program
2. Application Program
3. Metadata Write (valid = 1) ← 마지막!
4. Boot Config Update

→ Metadata가 없으면 Bootloader가 무시
→ 부분 업데이트는 무효화됨
```

## 핵심 정리

### ✅ Dual Bank OTA
- **Application이 Flash Erase & Program**
- **Bootloader는 검증 & 실행만**
- **활성 Region → 비활성 Region 업데이트**
- **TC375 하드웨어가 Region 분리 지원**

### ✅ Single Bank OTA
- **Bootloader가 Flash Erase & Program**
- **Application은 다운로드만**
- **Bootloader 모드 진입 필요**

### 🎯 결론

**Dual Bank에서는 Application이 Flash를 지우고 프로그램합니다!**

이유:
1. 자기 자신은 실행 중이므로 수정 불가
2. 다른 Region은 완전히 분리되어 안전하게 수정 가능
3. TC375 하드웨어가 MPU로 보호
4. Bootloader는 검증 및 실행만 담당

**Single Bank는 Bootloader가, Dual Bank는 Application이 Flash 작업 담당!**

## 참고 문서

- `docs/dual_bank_ota_detailed.md` - 상세 설명
- `docs/bootloader_build_process.md` - 빌드 프로세스
- `end_node_ecu/tc375/src/ota_handler.c` - Application OTA 구현
- `end_node_ecu/tc375/bootloader/stage2/stage2_main.c` - Bootloader 검증 구현

