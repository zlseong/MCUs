# TC375 RTOS Support

TC375 Lite Kit에서 사용 가능한 RTOS 옵션들을 정리합니다.

## 지원 가능한 RTOS

### 1. FreeRTOS (추천) ⭐
```
장점:
- 오픈소스 (MIT 라이선스)
- 가장 널리 사용되는 RTOS
- 메모리 풋프린트 작음 (~10KB)
- AWS IoT Core 통합 지원
- Infineon 공식 지원

단점:
- AUTOSAR 미준수

사용 케이스:
- 우리 프로젝트 (OTA, DoIP, 멀티태스킹)
- IoT 디바이스
- 프로토타입 개발
```

**설치 방법 (AURIX Development Studio):**
```
Option 1: ADS에서 자동 설치 (권장)
------------------------------------
Help → Install New Software
  → Add Repository
  → Location: https://www.freertos.org/eclipse
  → Install "FreeRTOS Kernel"

Option 2: 수동 다운로드
------------------------------------
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
cd FreeRTOS-Kernel
git checkout V10.5.1

프로젝트 우클릭 → Properties → C/C++ Build → Settings
  → Include Paths에 FreeRTOS/Source/include 추가
```

**기본 구성 (FreeRTOSConfig.h):**
```c
#define configCPU_CLOCK_HZ              300000000  // TC375: 300 MHz
#define configTICK_RATE_HZ              1000       // 1ms tick
#define configMAX_PRIORITIES            8
#define configMINIMAL_STACK_SIZE        256
#define configTOTAL_HEAP_SIZE           (64 * 1024)  // 64KB

#define configUSE_PREEMPTION            1
#define configUSE_IDLE_HOOK             1
#define configUSE_TICK_HOOK             0
#define configUSE_TIMERS                1
#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     1
#define configUSE_COUNTING_SEMAPHORES   1
#define configUSE_QUEUE_SETS            1
```

---

### 2. Zephyr RTOS
```
장점:
- Linux Foundation 프로젝트
- 모던한 아키텍처 (Device Tree 기반)
- 네트워킹 스택 내장 (lwIP, OpenThread)
- Bluetooth, Thread, Matter 지원

단점:
- TC375 공식 지원 없음 (포팅 필요)
- 학습 곡선 높음

사용 케이스:
- 최신 IoT 프로토콜 필요 시
- Linux-like 개발 환경 선호 시
```

---

### 3. AUTOSAR Adaptive/Classic
```
장점:
- 자동차 산업 표준
- Infineon 공식 지원
- 안전 인증 (ISO 26262)
- 생산 차량 적용 가능

단점:
- 상용 라이선스 필요 (매우 비쌈)
- 복잡한 설정
- 개발 속도 느림

사용 케이스:
- 양산 차량 ECU
- 기능안전(ASIL) 요구사항 있을 때
- OEM 요구사항
```

---

### 4. Micrium µC/OS-III
```
장점:
- 안정성 검증됨
- 코드 가독성 좋음
- 상세한 문서

단점:
- 상용 라이선스 (Weston Embedded)
- 커뮤니티 작음

사용 케이스:
- 의료기기, 항공우주
- 안전 인증 필요 시
```

---

### 5. ThreadX (Azure RTOS)
```
장점:
- Microsoft 공식 지원
- Azure IoT 통합
- 안전 인증 (IEC 61508, ISO 26262)
- 무료 (MIT 라이선스, 2019년부터)

단점:
- TC375 포팅 레이어 직접 작성 필요

사용 케이스:
- Azure Cloud 연동
- 산업용 IoT
```

---

### 6. No RTOS (Bare Metal)
```
장점:
- 가장 빠른 응답 시간
- 메모리 오버헤드 없음
- 디버깅 간단

단점:
- 복잡한 멀티태스킹 어려움
- 수동으로 스케줄링 구현 필요

사용 케이스:
- 단순한 Bootloader (우리의 Stage 1/2)
- 초고속 응답 필요 시
```

---

## 우리 프로젝트 권장사항

### Bootloader (Stage 1/2)
```
권장: Bare Metal (No RTOS)
이유: 
- 빠른 부팅 속도 필요
- 단순한 로직 (Flash 검증, 점프)
- 메모리 절약 (128KB만 사용)
```

### Application Firmware
```
권장: FreeRTOS
이유:
- 멀티태스킹 필요 (DoIP, UDS, OTA, CAN)
- 오픈소스 (무료)
- lwIP 통합 용이
- 프로토타입 → 양산 전환 가능
```

---

## FreeRTOS 통합 예시

### Task 구성 (우리 프로젝트 기준)

```c
/* Priority Levels */
#define PRIORITY_SAFETY         (configMAX_PRIORITIES - 1)  // 7: Safety monitoring
#define PRIORITY_COMMUNICATION  (configMAX_PRIORITIES - 2)  // 6: DoIP/CAN
#define PRIORITY_OTA            (configMAX_PRIORITIES - 3)  // 5: OTA manager
#define PRIORITY_APPLICATION    (configMAX_PRIORITIES - 4)  // 4: User app
#define PRIORITY_IDLE           0

/* Task Definitions */
TaskHandle_t task_doip_server;      // DoIP communication
TaskHandle_t task_uds_handler;      // UDS diagnostic
TaskHandle_t task_ota_manager;      // OTA update
TaskHandle_t task_can_gateway;      // CAN routing
TaskHandle_t task_heartbeat;        // Tester present

/* Task Stack Sizes */
#define STACK_SIZE_DOIP         (4 * 1024)  // 4KB
#define STACK_SIZE_UDS          (2 * 1024)  // 2KB
#define STACK_SIZE_OTA          (8 * 1024)  // 8KB (Flash ops)
#define STACK_SIZE_CAN          (2 * 1024)  // 2KB
#define STACK_SIZE_HEARTBEAT    (512)       // 512B
```

### 초기화 코드

```c
void app_main(void) {
    /* Initialize hardware */
    board_init();
    
    /* Create tasks */
    xTaskCreate(task_doip_server_fn, 
                "DoIP", 
                STACK_SIZE_DOIP, 
                NULL, 
                PRIORITY_COMMUNICATION, 
                &task_doip_server);
    
    xTaskCreate(task_ota_manager_fn, 
                "OTA", 
                STACK_SIZE_OTA, 
                NULL, 
                PRIORITY_OTA, 
                &task_ota_manager);
    
    /* Start scheduler */
    vTaskStartScheduler();  // Never returns
    
    /* Should never reach here */
    while (1);
}
```

---

## 메모리 요구사항

| Component      | Bare Metal | FreeRTOS | AUTOSAR |
|----------------|------------|----------|---------|
| Kernel         | 0 KB       | ~10 KB   | ~200 KB |
| Per Task Stack | N/A        | 1-8 KB   | 2-16 KB |
| Heap           | 0 KB       | 64 KB    | 256 KB  |
| **Total**      | **0 KB**   | **~100 KB** | **~500 KB** |

TC375 Lite Kit: **6 MB PFLASH** → FreeRTOS 충분히 가능!

---

## 참고자료

- [FreeRTOS Official](https://www.freertos.org/)
- [Infineon AURIX Development Studio](https://www.infineon.com/cms/en/tools/aurix-tools/aurix-development-studio/)
- [lwIP TCP/IP Stack](https://savannah.nongnu.org/projects/lwip/)
- [AUTOSAR Adaptive](https://www.autosar.org/)

---

## 다음 단계

1. **하드웨어 확인**: TC375 Lite Kit에 Ethernet 모듈 연결
2. **RTOS 선택**: FreeRTOS 추천 (프로토타입 → 양산)
3. **lwIP 통합**: TCP/IP 스택 포팅
4. **DoIP 클라이언트 구현**: VMG와 통신
5. **OTA Manager 통합**: RTOS Task로 구현

