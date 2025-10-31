# Boot & Runtime Comparison: Server vs VMG vs ZG vs ECU

## 비교 요약

| 구분 | OTA Server | VMG | Zonal Gateway (ZG) | End Node ECU |
|------|-----------|-----|-------------------|--------------|
| **플랫폼** | Ubuntu/Linux (PC) | Linux (Laptop/SBC) | TC375 (MCU) | TC375 (MCU) |
| **부팅 방식** | OS 부팅 → 서비스 시작 | OS 부팅 → 앱 실행 | **Bootloader → App** | **Bootloader → App** |
| **실행 환경** | Python (인터프리터) | C++ (네이티브) | C (Bare Metal/RTOS) | C (Bare Metal/RTOS) |
| **시작 방법** | `./run_server.sh` | `./vmg_gateway` | **전원 ON → 자동** | **전원 ON → 자동** |
| **종료 방법** | Ctrl+C / systemctl stop | Ctrl+C / kill | **전원 OFF** | **전원 OFF** |
| **재시작** | 수동 재실행 | 수동 재실행 | **전원 재인가** | **전원 재인가** |

---

## 1. OTA Server (Ubuntu/Linux PC)

### 부팅 프로세스

```
[전원 ON]
    ↓
[BIOS/UEFI]
    ↓
[Linux Kernel 로드]
    ↓
[systemd 초기화]
    ↓
[사용자 로그인]
    ↓
[수동 실행: ./run_server.sh]
    ↓
[Python 인터프리터 시작]
    ↓
[server.main 모듈 로드]
    ↓
[MQTT/HTTPS 서버 시작]
    ↓
[대기 상태 - Event Loop]
```

### 특징
- ✅ **OS 레벨 실행** (프로세스)
- ✅ **수동 시작** (또는 systemd 서비스)
- ✅ **종료 가능** (Ctrl+C)
- ✅ **재시작 가능** (언제든지)

---

## 2. VMG (Linux Laptop/SBC)

### 부팅 프로세스

```
[전원 ON]
    ↓
[BIOS/UEFI]
    ↓
[Linux Kernel 로드]
    ↓
[systemd 초기화]
    ↓
[자동 실행: systemd service]
    ↓
[vmg_gateway 프로세스 시작]
    ↓
[DoIP Server 시작 (Port 13400)]
[HTTPS Client 초기화]
[MQTT Client 초기화]
    ↓
[대기 상태 - Event Loop]
```

### 특징
- ✅ **OS 레벨 실행** (프로세스)
- ✅ **자동 시작** (systemd 서비스)
- ✅ **종료 가능** (하지만 차량에서는 계속 실행)
- ✅ **재시작 가능**

---

## 3. Zonal Gateway (TC375 MCU) ⚡

### 부팅 프로세스 (핵심 차이!)

```
[전원 ON (IGN ON)]
    ↓
[Hardware Reset]
    ↓
┌─────────────────────────────────────────────┐
│ STAGE 1: SSW (Startup Software)            │
│  - BMI (Boot Mode Index) 읽기              │
│  - 하드웨어 초기화                          │
│  - 활성 Bank 결정 (Region A or B)          │
│  - Stage 2 Bootloader로 점프               │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ STAGE 2: Application Bootloader            │
│  - Metadata 검증 (CRC, Signature)          │
│  - Application 유효성 확인                  │
│  - OTA 플래그 확인                          │
│  - Application으로 점프                     │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ APPLICATION: Zonal Gateway Firmware         │
│  - FreeRTOS 시작 (또는 Bare Metal)         │
│  - 하드웨어 초기화 (Ethernet PHY, CAN, etc)│
│  - DoIP Server 시작 (Port 13400)           │
│  - DoIP Client 초기화 (VMG 연결)           │
│  - Task 스케줄링 시작                       │
└─────────────────────────────────────────────┘
    ↓
[대기 상태 - RTOS Scheduler]
    ↓
[VMG로부터 메시지 수신 대기]
[ECU로 메시지 전달]
    ↓
[전원 OFF까지 계속 실행]
```

### 특징
- ⚡ **Bare Metal 실행** (OS 없음, RTOS만)
- ⚡ **Bootloader가 시작** (자동)
- ⚡ **종료 불가** (전원 OFF만 가능)
- ⚡ **재시작 = 전원 재인가**
- ⚡ **Dual Bank** (OTA 업데이트 지원)

---

## 4. End Node ECU (TC375 MCU) ⚡

### 부팅 프로세스 (ZG와 동일)

```
[전원 ON (IGN ON)]
    ↓
[Hardware Reset]
    ↓
┌─────────────────────────────────────────────┐
│ STAGE 1: SSW (Startup Software)            │
│  - BMI 읽기                                 │
│  - 하드웨어 초기화                          │
│  - 활성 Bank 결정                           │
│  - Stage 2 Bootloader로 점프               │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ STAGE 2: Application Bootloader            │
│  - Metadata 검증                            │
│  - Application 유효성 확인                  │
│  - OTA 플래그 확인                          │
│  - Application으로 점프                     │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ APPLICATION: ECU Firmware                   │
│  - FreeRTOS 시작                            │
│  - 하드웨어 초기화 (센서, 액추에이터)      │
│  - DoIP Client 초기화 (ZG 연결)            │
│  - UDS 서비스 핸들러 등록                   │
│  - Task 스케줄링 시작                       │
└─────────────────────────────────────────────┘
    ↓
[대기 상태 - RTOS Scheduler]
    ↓
[ZG로부터 진단 메시지 수신 대기]
[센서 데이터 수집 및 전송]
    ↓
[전원 OFF까지 계속 실행]
```

### 특징
- ⚡ **Bare Metal 실행** (OS 없음, RTOS만)
- ⚡ **Bootloader가 시작** (자동)
- ⚡ **종료 불가** (전원 OFF만 가능)
- ⚡ **재시작 = 전원 재인가**
- ⚡ **Dual Bank** (OTA 업데이트 지원)

---

## 핵심 차이점

### A. 시작 방식

| 구분 | Server/VMG | ZG/ECU (TC375) |
|------|-----------|----------------|
| **시작** | 수동 실행 (또는 systemd) | **전원 인가 시 자동** |
| **부팅** | OS → 프로세스 | **Bootloader → App** |
| **제어** | 사용자가 시작/종료 | **하드웨어가 제어** |

### B. 실행 환경

| 구분 | Server/VMG | ZG/ECU (TC375) |
|------|-----------|----------------|
| **OS** | Linux (Ubuntu) | **없음 (Bare Metal)** |
| **런타임** | Python/C++ | **C + FreeRTOS** |
| **메모리** | GB 단위 (가상 메모리) | **MB 단위 (물리 메모리만)** |
| **파일 시스템** | ext4, 파일 읽기/쓰기 | **없음 (Flash 직접 접근)** |

### C. 종료 방식

| 구분 | Server/VMG | ZG/ECU (TC375) |
|------|-----------|----------------|
| **정상 종료** | Ctrl+C, systemctl stop | **전원 OFF** |
| **비정상 종료** | kill -9 | **전원 차단** |
| **재시작** | 프로그램 재실행 | **전원 재인가** |

---

## 상세 비교: 대기 상태

### OTA Server (Python)

```python
# server/main.py
async def start(self):
    # 서버 시작
    mqtt_task = asyncio.create_task(start_mqtt_server(...))
    https_task = asyncio.create_task(start_https_server(...))
    
    # 이벤트 대기 (여기서 계속 대기)
    await self.shutdown_event.wait()  # ← Ctrl+C까지 대기
```

**특징:**
- Python 이벤트 루프
- OS 스케줄러가 관리
- 언제든지 종료 가능

### VMG (C++)

```cpp
// vehicle_gateway/src/vmg_gateway.cpp
int main() {
    // 초기화
    setup_doip_server();
    setup_https_client();
    setup_mqtt_client();
    
    // 메인 루프 (여기서 계속 대기)
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

**특징:**
- C++ 메인 루프
- OS 스케줄러가 관리
- 시그널로 종료 가능

### Zonal Gateway (C + FreeRTOS)

```c
// zonal_gateway/tc375/src/zonal_gateway_main.c
int main(void) {
    // 하드웨어 초기화
    init_hardware();
    
    // FreeRTOS 태스크 생성
    xTaskCreate(doip_server_task, ...);
    xTaskCreate(doip_client_task, ...);
    xTaskCreate(can_handler_task, ...);
    
    // RTOS 스케줄러 시작 (여기서 제어권 넘김)
    vTaskStartScheduler();  // ← 전원 OFF까지 반환 안 됨!
    
    // 여기는 절대 도달하지 않음
    while(1);
}
```

**특징:**
- FreeRTOS 스케줄러
- 하드웨어가 직접 관리
- **전원 OFF만 종료 가능**

### End Node ECU (C + FreeRTOS)

```c
// end_node_ecu/tc375/src/ecu_main.c
int main(void) {
    // 하드웨어 초기화
    init_hardware();
    
    // FreeRTOS 태스크 생성
    xTaskCreate(doip_client_task, ...);
    xTaskCreate(uds_handler_task, ...);
    xTaskCreate(sensor_task, ...);
    
    // RTOS 스케줄러 시작
    vTaskStartScheduler();  // ← 전원 OFF까지 반환 안 됨!
    
    // 여기는 절대 도달하지 않음
    while(1);
}
```

**특징:**
- FreeRTOS 스케줄러
- 하드웨어가 직접 관리
- **전원 OFF만 종료 가능**

---

## Bootloader의 역할 (ZG/ECU만 해당)

### TC375 Dual Bank Bootloader

```
┌─────────────────────────────────────────────────────────┐
│           TC375 PFLASH (6MB)                            │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Region A (3MB) - 활성                                  │
│  ├─ BMI (256B)                                          │
│  ├─ SSW (64KB)        ← Stage 1 Bootloader             │
│  ├─ Bootloader (196KB) ← Stage 2 Bootloader            │
│  └─ Application (2.1MB) ← 실제 펌웨어                   │
│                                                         │
│  Region B (3MB) - 백업                                  │
│  ├─ BMI (256B)                                          │
│  ├─ SSW (64KB)                                          │
│  ├─ Bootloader (196KB)                                  │
│  └─ Application (2.1MB) ← OTA 업데이트 대상             │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 부팅 시퀀스

```
[전원 ON]
    ↓
[SSW 실행]
    ↓
    ├─ Region A 메타데이터 읽기
    │   ├─ Magic: 0xA5A5A5A5?
    │   ├─ CRC32 검증
    │   └─ Valid 플래그 확인
    │
    ├─ Region B 메타데이터 읽기
    │   ├─ Magic: 0xA5A5A5A5?
    │   ├─ CRC32 검증
    │   └─ Valid 플래그 확인
    │
    └─ 활성 Region 선택
        ├─ OTA 플래그 확인
        ├─ Boot Count 확인
        └─ Region A or B 결정
    ↓
[선택된 Region의 Bootloader 실행]
    ↓
    ├─ Application 메타데이터 검증
    ├─ PQC 서명 검증 (선택사항)
    └─ Application으로 점프
    ↓
[Application 실행]
    ↓
[FreeRTOS 시작]
    ↓
[Task 실행 - 무한 루프]
```

---

## 재시작 비교

### OTA Server

```bash
# 종료
Ctrl+C

# 재시작
./run_server.sh
```

**소요 시간:** ~1-2초

### VMG

```bash
# 종료
sudo systemctl stop vmg-gateway

# 재시작
sudo systemctl start vmg-gateway
```

**소요 시간:** ~1-2초

### Zonal Gateway (TC375)

```
[전원 OFF]
    ↓
[대기...]
    ↓
[전원 ON]
    ↓
[SSW 실행 - 10ms]
    ↓
[Bootloader 실행 - 50ms]
    ↓
[Application 시작 - 100ms]
    ↓
[FreeRTOS 초기화 - 50ms]
    ↓
[DoIP 서버 시작 - 100ms]
```

**소요 시간:** ~300ms (매우 빠름!)

### End Node ECU (TC375)

```
[전원 OFF]
    ↓
[대기...]
    ↓
[전원 ON]
    ↓
[SSW 실행 - 10ms]
    ↓
[Bootloader 실행 - 50ms]
    ↓
[Application 시작 - 100ms]
    ↓
[FreeRTOS 초기화 - 50ms]
    ↓
[DoIP 클라이언트 시작 - 50ms]
```

**소요 시간:** ~260ms (매우 빠름!)

---

## 요약

### 질문: "ZG나 ECU도 마찬가지인거지? 차이점이 있다면 얘네들은 전원 인가하면 부트로더가 키는거고"

### 답변: **정확합니다!**

| 항목 | Server/VMG | ZG/ECU (TC375) |
|------|-----------|----------------|
| **동작 방식** | 대기 → 요청 처리 → 대기 | **동일** |
| **이벤트 기반** | ✅ | ✅ |
| **비동기 처리** | ✅ | ✅ (RTOS 태스크) |
| **시작 방식** | 수동 실행 | **전원 ON → Bootloader → App** |
| **종료 방식** | Ctrl+C | **전원 OFF** |
| **재시작** | 프로그램 재실행 | **전원 재인가** |
| **OS** | Linux | **없음 (Bare Metal)** |
| **부팅 시간** | ~1-2초 | **~300ms** |

### 핵심 차이:

1. **Server/VMG**: OS 위에서 실행, 언제든지 시작/종료 가능
2. **ZG/ECU**: Bare Metal, **전원 인가 시 Bootloader가 자동으로 시작**, 전원 OFF만 종료 가능

**하지만 "대기 → 요청 처리 → 대기" 패턴은 모두 동일합니다!** ✅

