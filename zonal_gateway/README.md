# Zonal Gateway (ZG)

Zone 내 ECU들을 관리하고 VMG(CCU)와 통신하는 Zonal Gateway 구현

## 📁 구조

```
zonal_gateway/
├── tc375/                # TC375 MCU 버전
│   ├── include/
│   │   └── zonal_gateway.h
│   ├── src/
│   │   ├── zonal_gateway.c
│   │   └── zonal_gateway_main.c
│   ├── bootloader/
│   └── README_TC375.md
│
├── linux/                # Linux x86 버전
│   ├── include/
│   │   └── zonal_gateway_linux.hpp
│   ├── src/
│   │   ├── zonal_gateway_linux.cpp
│   │   └── main.cpp
│   ├── CMakeLists.txt
│   └── README_LINUX.md
│
├── common/               # 공통 로직
│   ├── doip_message.h
│   ├── uds_handler.h
│   └── zone_manager.h
│
└── README.md            # 이 파일
```

## 🎯 역할

### Downstream (Zone 내부 ECU 대상)
- **DoIP Server** (TCP/UDP 13400)
  - Vehicle Discovery (UDP)
  - Diagnostic Communication (TCP)
- **JSON Server** (TCP 8765)
  - VCI 수집
  - 상태 모니터링

### Upstream (VMG 대상)
- **DoIP Client**
  - VMG 연결 (TCP 13400)
  - Routing Activation
  - Heartbeat (Tester Present)
- **Zone VCI 집계 및 전송**
- **OTA 조율**

## 🚀 빌드 및 실행

### TC375 버전
```bash
cd tc375
./build_zg_tc375.sh

# 출력 파일:
# - zonal_gateway.hex  (Flash용)
# - zonal_gateway.elf  (디버그용)
```

### Linux 버전
```bash
cd linux
mkdir build && cd build
cmake ..
make

# 실행
./zonal_gateway_linux <zone_id> [vmg_ip] [vmg_port]

# 예제
./zonal_gateway_linux 1 192.168.1.1 13400
```

## 📊 동작 흐름

### 1. 초기화
```
[INIT] Zone ID 설정
[INIT] VMG 연결 정보 설정
[INIT] DoIP Client/Server 초기화
```

### 2. Zone 내 ECU Discovery
```
[DISCOVERY] UDP 13400 리스닝
[DISCOVERY] ECU들로부터 Vehicle ID Request 수신
[DISCOVERY] Vehicle ID Response 전송
[DISCOVERY] ECU 등록
```

### 3. VMG 연결
```
[VMG] TCP 연결 (192.168.1.1:13400)
[VMG] Routing Activation
[VMG] Connected!
```

### 4. 정상 운영
```
[OPERATION] ECU 메시지 처리
[OPERATION] VMG Heartbeat (10초마다)
[OPERATION] Zone VCI 전송 (60초마다)
[OPERATION] OTA 조율
```

## 🔌 API 예제

### TC375 (C)
```c
#include "zonal_gateway.h"

ZonalGateway_t zg;

// 초기화
zg_init(&zg, 1, "192.168.1.1", 13400);

// 시작
zg_start(&zg);

// VMG 연결
zg_connect_to_vmg(&zg);

// Zone VCI 전송
zg_send_zone_vci_to_vmg(&zg);

// 메인 루프
while (1) {
    zg_run(&zg);
    usleep(10000);
}
```

### Linux (C++)
```cpp
#include "zonal_gateway_linux.hpp"

vmg::ZonalGatewayLinux zg(1, "192.168.1.1", 13400);

// 시작
zg.start();

// Zone VCI 전송
zg.sendZoneVCIToVMG();

// 실행
zg.run();  // Blocks until stop()
```

## 📋 Zone VCI 구조

```json
{
  "zone_id": 1,
  "ecu_count": 2,
  "ecus": [
    {
      "ecu_id": "TC375-SIM-002-Zone1-ECU1",
      "logical_address": "0x0201",
      "firmware_version": "1.0.0",
      "hardware_version": "TC375TP-LiteKit-v2.0",
      "is_online": true,
      "ota_capable": true,
      "delta_update_supported": true,
      "max_package_size": 10485760
    }
  ],
  "total_storage_mb": 512,
  "available_storage_mb": 256,
  "average_battery_level": 85
}
```

## 🔧 설정

### 포트
- **DoIP Server**: 13400 (TCP/UDP)
- **JSON Server**: 8765 (TCP)
- **VMG Client**: 13400 (TCP)

### Zone ID
- Zone #1: 0x0201
- Zone #2: 0x0202
- Zone #3: 0x0203

### 타이밍
- **Heartbeat**: 10초
- **VCI Update**: 60초
- **ECU Discovery**: 연속 (UDP)

## 🌐 네트워크 설정

### TC375
```c
// lwIP 설정
IP_ADDRESS:   192.168.1.10  (Zone 1)
              192.168.1.20  (Zone 2)
              192.168.1.30  (Zone 3)
NETMASK:      255.255.255.0
GATEWAY:      192.168.1.1   (VMG)
```

### Linux
```bash
# 네트워크 인터페이스 설정
sudo ifconfig eth0 192.168.1.10 netmask 255.255.255.0
```

## 📚 참고 문서

- [Zonal Gateway 아키텍처](../../docs/zonal_gateway_architecture.md)
- [DoIP ISO 13400](../../docs/protocols/ISO_13400_specification.md)
- [OTA 시나리오](../../docs/protocols/ota_scenario_detailed.md)

## ⚠️ 주의사항

### TC375
- `tricore-gcc` 툴체인 필요
- lwIP 또는 커스텀 네트워크 스택 필요
- Flash/EEPROM 드라이버 필요

### Linux
- 개발 및 시뮬레이션 용도
- 실제 하드웨어에서는 TC375 사용 권장
- 성능 제한으로 노트북 대체 가능

## 🐛 디버깅

### 로그 레벨
```c
// TC375
#define ZG_LOG_LEVEL_DEBUG
#define ZG_LOG_LEVEL_INFO
#define ZG_LOG_LEVEL_ERROR
```

### 네트워크 확인
```bash
# DoIP 포트 확인
netstat -an | grep 13400

# VMG 연결 테스트
telnet 192.168.1.1 13400
```

## 📊 성능 지표

- **ECU 처리**: 최대 8개 ECU/Zone
- **메시지 처리**: 100 msgs/sec
- **메모리**: 4KB VCI 버퍼
- **네트워크**: 100Mbps Ethernet

## 🔗 관련 파일

- `zonal_gateway.h` - TC375 헤더
- `zonal_gateway_linux.hpp` - Linux 헤더
- `doip_client.h` - DoIP Client (VMG 연결)
- `doip_message.h` - DoIP 프로토콜
- `uds_handler.h` - UDS 서비스

