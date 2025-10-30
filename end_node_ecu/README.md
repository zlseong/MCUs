# End Node ECU

Zone 내 최종 노드 ECU 구현 (TC375 전용)

## 📁 구조

```
end_node_ecu/
├── tc375/                # TC375 MCU 버전
│   ├── include/
│   │   └── ecu_node.h
│   ├── src/
│   │   ├── ecu_node.c
│   │   └── ecu_main.c
│   ├── bootloader/
│   │   ├── ssw_main.c       # Stage 1: Startup Software
│   │   ├── stage2_main.c    # Stage 2: Bootloader
│   │   ├── stage2a_linker.ld
│   │   └── stage2b_linker.ld
│   └── README_TC375.md
│
├── common/               # 공통 로직
│   ├── doip_client.h    # DoIP Client
│   ├── uds_services.h   # UDS 서비스
│   └── flash_manager.h  # Flash 관리
│
└── README.md            # 이 파일
```

## 🎯 역할

- **DoIP Client**: Zonal Gateway에 연결
- **UDS Services**: 진단 서비스 제공
- **OTA Target**: 펌웨어 수신 및 설치
- **Dual Bank**: 안전한 업데이트 및 롤백

## 🚀 빌드 및 실행

### TC375
```bash
cd tc375
./build_ecu_tc375.sh

# 출력 파일:
# - ecu_node.hex        (Application)
# - bootloader.hex      (Bootloader)
# - ssw.hex             (Startup Software)
```

### Flash 순서
```
1. SSW (0x80000000)        - Startup Software
2. Bootloader (0x80020000) - Stage 2 Bootloader
3. Application (0x80040000 or 0x82000000) - 실제 펌웨어
```

## 📊 동작 흐름

### 1. 부팅
```
[SSW] Power-On Reset
[SSW] → Jump to Bootloader

[BOOTLOADER] Check Boot Flag
[BOOTLOADER] Boot from Region A or B
[BOOTLOADER] → Jump to Application

[APPLICATION] ECU Node Start
```

### 2. ZG 연결
```
[ECU] Discover Zone Gateway (UDP)
[ECU] Connect to ZG (TCP 13400)
[ECU] Routing Activation
[ECU] Connected!
```

### 3. 정상 운영
```
[OPERATION] Heartbeat (10초)
[OPERATION] VCI Update (60초)
[OPERATION] UDS 요청 처리
```

### 4. OTA 업데이트
```
[OTA] Receive firmware → Region B (Inactive)
[OTA] Verify firmware
[OTA] Set boot flag → Region B
[OTA] Reboot
[OTA] Execute new firmware from Region B
[OTA] Copy to Region A (Background)
```

## 🔌 API 예제

```c
#include "ecu_node.h"

ECUNode_t ecu;

// 초기화
ecu_init(&ecu, "TC375-ECU-002-Zone1-ECU1", 
         0x0201, "192.168.1.10", 13400);

// 시작 (ZG 연결 포함)
ecu_start(&ecu);

// 정보 출력
ecu_print_info(&ecu);

// 메인 루프
while (1) {
    ecu_run(&ecu);  // Heartbeat, VCI, Message handling
    usleep(10000);
}
```

## 📋 ECU VCI 구조

```json
{
  "ecu_id": "TC375-ECU-002-Zone1-ECU1",
  "logical_address": "0x0201",
  "firmware_version": "1.0.0",
  "hardware_version": "TC375TP-LiteKit-v2.0",
  "is_online": true,
  "ota_capable": true,
  "delta_update_supported": true,
  "max_package_size": 10485760,
  "active_bank": "A",
  "boot_count": 5
}
```

## 🔧 UDS 서비스

### 지원 서비스
- **0x10**: Diagnostic Session Control
- **0x11**: ECU Reset
- **0x22**: Read Data By Identifier
- **0x27**: Security Access
- **0x2E**: Write Data By Identifier
- **0x31**: Routine Control
- **0x34**: Request Download
- **0x36**: Transfer Data
- **0x37**: Request Transfer Exit
- **0x3E**: Tester Present
- **0x19**: Read DTC Information

### 예제: VIN 읽기
```
Request:  22 F1 90
Response: 62 F1 90 4B 4D 48 47 48 34 4A 48 31 4E 55 31 32 33 34 35 36
          (0x62 + DID + "KMHGH4JH1NU123456")
```

## 🏗️ 메모리 맵

```
TC375 Flash Layout:
┌──────────────────────────────────────┐
│ 0x80000000 - 0x80001FFF: SSW (8KB)   │ Stage 1
├──────────────────────────────────────┤
│ 0x80020000 - 0x8003FFFF: BL (128KB)  │ Stage 2
├──────────────────────────────────────┤
│ 0x80040000 - 0x8103FFFF: Region A    │ Bank A (Active)
│                          (16MB)       │
├──────────────────────────────────────┤
│ 0x82000000 - 0x82FFFFFF: Region B    │ Bank B (Inactive)
│                          (16MB)       │
└──────────────────────────────────────┘
```

## 📊 OTA 업데이트 흐름

### Phase 1: 펌웨어 수신
```
ZG → ECU: UDS 0x34 Request Download (Region B)
ZG → ECU: UDS 0x36 Transfer Data (blocks)
ZG → ECU: UDS 0x37 Request Transfer Exit
```

### Phase 2: 검증
```
ECU: Verify CRC/Hash
ECU: Check signature (optional)
ECU: Set firmware status = VALID
```

### Phase 3: 설치
```
ECU: UDS 0x31 Routine Control (Install)
ECU: Set boot flag → Region B
ECU: UDS 0x11 ECU Reset
```

### Phase 4: 실행 및 보고
```
[Boot] Execute from Region B
[App] Self-test
[App] Report result to ZG
[App] Copy to Region A (background)
```

## 🔐 보안

### Secure Boot (선택)
```c
// SSW에서 Bootloader 서명 검증
verify_signature(bootloader_image, public_key);

// Bootloader에서 Application 서명 검증
verify_signature(application_image, public_key);
```

### Security Access
```c
// UDS 0x27 Security Access
Request:  27 01           (Request Seed)
Response: 67 01 12 34 56 78 (Seed)

Request:  27 02 AB CD EF 90 (Send Key)
Response: 67 02           (Success)
```

## 🐛 디버깅

### UART 로그
```c
// Debug output via UART
printf("[ECU] Connecting to ZG...\n");
printf("[ECU] Firmware version: %s\n", ecu->firmware_version);
```

### LED 상태
- **Green**: 정상 동작
- **Yellow**: OTA 진행 중
- **Red**: 에러
- **Blinking**: Heartbeat

## ⚠️ 주의사항

### TC375 전용
- End Node ECU는 실제 하드웨어(TC375)에만 올라갑니다
- Linux 시뮬레이션 버전은 없습니다
- 개발/테스트는 Zonal Gateway Linux로 진행 가능

### 메모리
- Application 최대 크기: 16MB
- OTA 패키지 최대 크기: 10MB (설정 가능)
- RAM 버퍼: 4KB

### 타이밍
- Heartbeat: 10초
- VCI Update: 60초
- Flash Write: ~100ms/block

## 📚 참고 문서

- [TC375 Bootloader](bootloader/README.md)
- [UDS ISO 14229](../../common/protocol/uds_standard.h)
- [DoIP Client](../../common/protocol/doip_protocol.h)
- [OTA 시나리오](../../docs/protocols/ota_scenario_detailed.md)

## 🔗 관련 파일

- `ecu_node.h` - ECU Node API
- `doip_client.h` - DoIP Client
- `uds_handler.h` - UDS 서비스
- `flash_manager.h` - Flash 관리
- `boot_common.h` - Bootloader 공통

## 📊 성능 지표

- **Boot Time**: <1초
- **OTA Speed**: ~500KB/s
- **Heartbeat**: 10초
- **Flash Endurance**: 100K cycles
- **Power**: <100mA @ 5V

