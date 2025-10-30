# Dual Bank OTA: 누가 Flash를 지우고 프로그램하는가?

## 질문

**A 메모리로 구동 중일 때, B 메모리의 부트로더B/제어SWB를 지우고 프로그램하는 것은?**
- Stage 2 부트로더?
- 제어 SW (Application)?

## 답: **제어 SW (Application)가 담당합니다!**

## 시나리오 비교

### Single Bank (부트로더가 담당)
```
┌─────────────────────────────────────────────────┐
│ Single Memory                                    │
├─────────────────────────────────────────────────┤
│ 0x80000000  Bootloader (불변)                   │
│             ↓ Flash Write 가능                   │
│ 0x80010000  Application (OTA 대상)              │
└─────────────────────────────────────────────────┘

프로세스:
1. OTA 패키지 수신 (Application)
2. Reboot
3. Bootloader 진입 ← Bootloader가 Flash 작업
4. Bootloader가 Application 영역 Erase
5. Bootloader가 Application 영역 Program
6. Bootloader가 Application 실행
```

### Dual Bank (Application이 담당)
```
┌─────────────────────────────────────────────────┐
│ Region A (Active)                                │
├─────────────────────────────────────────────────┤
│ 0x800A1000  Bootloader A (실행 중)              │
│ 0x800D3000  Application A (실행 중) ← 여기서!   │
│             ↓ Flash Write 가능                   │
│             ↓ Region B를 지우고 프로그램        │
└─────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────┐
│ Region B (Inactive - OTA 대상)                  │
├─────────────────────────────────────────────────┤
│ 0x820A1000  Bootloader B ← Erase & Program      │
│ 0x820D3000  Application B ← Erase & Program     │
└─────────────────────────────────────────────────┘

프로세스:
1. OTA 패키지 수신 (Application A가 실행 중)
2. Application A가 Region B Erase ← Application이!
3. Application A가 Region B Program ← Application이!
4. Application A가 Metadata 업데이트
5. Reboot
6. SSW가 Region B로 전환
7. Bootloader B 실행 → Application B 실행
```

## 왜 Application이 담당하는가?

### 1. 자기 자신을 지울 수 없다!

```c
// Bootloader A가 실행 중 (@ 0x800A1000)
void bootloader_erase_self(void) {
    Flash_Erase(0x800A1000, 196*1024);  // ❌ 자기 자신을 지우려고 시도
    // → 즉시 크래시! (실행 중인 코드를 지움)
}
```

**실행 중인 메모리는 지울 수 없습니다!**

### 2. Dual Bank의 핵심 장점

```
Region A 실행 중:
├─ Bootloader A: 실행 중 (Read-only)
├─ Application A: 실행 중
└─ Region B: 완전히 분리됨 ← 자유롭게 Erase/Program 가능!

Region B 실행 중:
├─ Bootloader B: 실행 중 (Read-only)
├─ Application B: 실행 중
└─ Region A: 완전히 분리됨 ← 자유롭게 Erase/Program 가능!
```

**TC375 하드웨어가 Region A/B를 완전히 분리하므로, 활성 Region에서 비활성 Region을 자유롭게 수정 가능!**

## 상세 코드 예제

### Application이 OTA를 처리하는 코드

```c
/**
 * Application A가 실행 중 (@ 0x800D3000, Region A)
 * Region B를 업데이트
 */

// 1. OTA 패키지 수신 (DoIP/Ethernet)
void app_receive_ota_package(void) {
    uint8_t ota_buffer[OTA_BUFFER_SIZE];
    
    // VMG로부터 OTA 패키지 수신
    doip_receive_diagnostic_message(ota_buffer, &ota_size);
    
    printf("[App A] Received OTA package: %u bytes\n", ota_size);
}

// 2. Region B Bootloader 영역 Erase & Program
void app_update_bootloader_b(uint8_t* bootloader_data, uint32_t size) {
    printf("[App A] Updating Bootloader B...\n");
    
    // Region B Bootloader 영역 Erase
    Flash_Erase(REGION_B_BOOT_START, REGION_B_BOOT_SIZE);
    
    // Region B Bootloader Program
    Flash_Write(REGION_B_BOOT_START, bootloader_data, size);
    
    // Bootloader Metadata 작성
    BootMetadata boot_meta;
    boot_meta.magic = MAGIC_NUMBER;
    boot_meta.size = size;
    boot_meta.crc32 = calculate_crc32(bootloader_data, size);
    // ... signature ...
    
    Flash_Write(REGION_B_BOOT_META, &boot_meta, sizeof(boot_meta));
    
    printf("[App A] Bootloader B updated successfully\n");
}

// 3. Region B Application 영역 Erase & Program
void app_update_application_b(uint8_t* app_data, uint32_t size) {
    printf("[App A] Updating Application B...\n");
    
    // Region B Application 영역 Erase
    Flash_Erase(REGION_B_APP_START, REGION_B_APP_SIZE);
    
    // Region B Application Program
    Flash_Write(REGION_B_APP_START, app_data, size);
    
    // Application Metadata 작성
    BootMetadata app_meta;
    app_meta.magic = MAGIC_NUMBER;
    app_meta.size = size;
    app_meta.crc32 = calculate_crc32(app_data, size);
    // ... signature ...
    
    Flash_Write(REGION_B_APP_META, &app_meta, sizeof(app_meta));
    
    printf("[App A] Application B updated successfully\n");
}

// 4. Boot Configuration 업데이트 (Region B로 전환 예약)
void app_switch_to_region_b(void) {
    BootConfig* cfg = (BootConfig*)BOOT_CFG_EEPROM;
    
    cfg->stage2_active = 1;  // Region B Bootloader 선택
    cfg->app_active = 1;     // Region B Application 선택
    cfg->ota_pending = 0;    // OTA 완료
    
    cfg->crc = calculate_crc32((uint8_t*)cfg, sizeof(BootConfig) - 4);
    
    printf("[App A] Boot config updated, will boot from Region B\n");
}

// 5. OTA 메인 프로세스
void app_process_ota(void) {
    printf("========================================\n");
    printf("Application A: Processing OTA Update\n");
    printf("Current: Region A (0x800D3000)\n");
    printf("Target:  Region B (0x820D3000)\n");
    printf("========================================\n");
    
    // OTA 패키지 수신
    app_receive_ota_package();
    
    // OTA 패키지 파싱
    parse_ota_package(&bootloader_data, &app_data);
    
    // Region B 업데이트 (자기 자신은 실행 중, Region B는 비활성)
    app_update_bootloader_b(bootloader_data, bootloader_size);
    app_update_application_b(app_data, app_size);
    
    // Boot config 업데이트
    app_switch_to_region_b();
    
    printf("[App A] OTA complete! Rebooting to Region B...\n");
    
    // Reboot
    system_reset();
}
```

### Bootloader의 역할 (OTA 시)

```c
/**
 * Bootloader B가 실행됨 (@ 0x820A1000, Region B)
 * Application A가 미리 작성해 놓음
 */

void stage2_main(void) {
    printf("[Bootloader B] Started @ 0x820A1000\n");
    
    // 1. Application B 검증
    if (!verify_application(REGION_B_APP_META)) {
        printf("[Bootloader B] Application B verification failed!\n");
        // Fallback to Region A
        switch_to_region_a();
        system_reset();
    }
    
    printf("[Bootloader B] Application B verified\n");
    
    // 2. Application B 실행
    jump_to_application(REGION_B_APP_START);
    
    // Never return
}
```

**Bootloader는 검증만 하고, Flash 작업은 하지 않음!**

## 전체 OTA 흐름

### Phase 1: OTA 다운로드 (Application이 담당)
```
[Running: Application A @ Region A]

1. VMG → ZG → ECU: OTA 패키지 전송 (DoIP)
2. Application A: OTA 패키지 수신
3. Application A: Region B Erase  ← Application이!
4. Application A: Region B Program ← Application이!
   ├─ Bootloader B 업데이트
   └─ Application B 업데이트
5. Application A: Boot Config 업데이트 (Region B 선택)
6. Application A: Reboot 요청
```

### Phase 2: 부팅 (Bootloader가 담당)
```
[Reboot]

7. SSW: Boot Config 읽기 → Region B 선택
8. SSW: Bootloader B 검증
9. SSW: Bootloader B로 점프 (@ 0x820A1000)

[Running: Bootloader B @ Region B]

10. Bootloader B: Application B 검증  ← Bootloader가!
11. Bootloader B: Application B로 점프 ← Bootloader가!

[Running: Application B @ Region B]

12. Application B: 정상 동작 확인
13. Application B: Boot Count 리셋 (성공 표시)
```

## 비교표

| 작업 | Single Bank | Dual Bank (TC375) |
|------|-------------|-------------------|
| **Flash Erase** | Bootloader | **Application** |
| **Flash Program** | Bootloader | **Application** |
| **검증** | Bootloader | **Bootloader** |
| **실행** | Bootloader | **Bootloader** |
| **대상** | 자기 자신 (위험!) | 다른 Region (안전!) |

## 왜 이런 차이가 있나?

### Single Bank의 한계
```
[Memory]
├─ Bootloader (불변)
└─ Application (OTA 대상)

문제:
- Application이 실행 중일 때는 Flash 작업 불가
- Bootloader 모드로 진입해야 함
- 다운타임 발생 (차량 멈춤)
```

### Dual Bank의 장점
```
[Memory]
├─ Region A (Active)
│   ├─ Bootloader A (실행 중)
│   └─ Application A (실행 중)
│
└─ Region B (Inactive)
    ├─ Bootloader B ← Application A가 업데이트!
    └─ Application B ← Application A가 업데이트!

장점:
- Application 실행 중 백그라운드 업데이트
- 차량 정상 동작 유지
- 다운타임 최소화 (Reboot만)
```

## 메모리 보호

### TC375 하드웨어 지원

```c
// Region A 실행 중
Flash_Erase(0x800A1000, 196*1024);  // ❌ 자기 자신 (Region A)
// → Hardware Exception! (MPU 보호)

Flash_Erase(0x820A1000, 196*1024);  // ✅ 다른 Region (Region B)
// → OK! Region B는 비활성이므로 안전
```

**TC375 MPU (Memory Protection Unit)가 실행 중인 Region 보호**

## 코드 위치

### Application (OTA 담당)
```
end_node_ecu/tc375/src/
├── ecu_node.c           # OTA 수신
├── ota_handler.c        # Flash Erase/Program ← 여기서!
└── flash_driver.c       # Flash 하드웨어 제어
```

### Bootloader (검증만)
```
end_node_ecu/tc375/bootloader/
├── stage2/stage2_main.c # 검증 & 점프만
└── common/boot_common.h # 검증 함수
```

## 핵심 정리

### ✅ Dual Bank (TC375)
- **Application이 Flash 작업** (Erase/Program)
- **Bootloader는 검증 & 실행**
- **활성 Region → 비활성 Region 업데이트**
- **안전함** (자기 자신 건드리지 않음)

### ✅ Single Bank
- **Bootloader가 Flash 작업**
- **Application은 다운로드만**
- **Bootloader 모드 진입 필요**
- **위험함** (잘못하면 brick)

## 결론

**Dual Bank에서는 Application이 Flash를 지우고 프로그램합니다!**

이유:
1. 자기 자신(Region A)은 실행 중이므로 건드릴 수 없음
2. 다른 Region(Region B)은 완전히 분리되어 안전하게 수정 가능
3. TC375 하드웨어가 Region 분리를 지원
4. Bootloader는 검증 및 실행만 담당

**Single Bank는 Bootloader가, Dual Bank는 Application이 Flash 작업을 담당합니다!**

