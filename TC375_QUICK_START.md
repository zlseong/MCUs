# TC375 Lite Kit - 빠른 시작 가이드

## ✅ 하드웨어 확인

### TC375 Lite Kit (KIT_A2G_TC375_LITE) 사양

```
MCU: Infineon AURIX TC375TP
- CPU: TriCore 1.6.2 @ 300 MHz (3코어)
- Flash: 6 MB PFLASH
- RAM: 824 KB (DSPR + PSPR)
- Ethernet: ✅ 내장 PHY (RGMII/RMII)
- CAN: 4x CAN-FD
- USB: 1x USB 2.0
- 디버거: On-board DAP debugger
```

**중요**: Ethernet PHY가 **내장**되어 있으므로 추가 하드웨어 없이 바로 네트워크 통신 가능!

---

## 🛠 개발 환경 설정

### 1. AURIX Development Studio (ADS) 설치

**다운로드**:
```
https://www.infineon.com/cms/en/tools/aurix-tools/
  → AURIX Development Studio (무료)
  → 버전: 1.9.x 이상
```

**설치 후 확인**:
- ✅ iLLD (Infineon Low-Level Driver) 포함
- ✅ Tasking/GCC 컴파일러
- ✅ DAP Debugger 지원

### 2. 프로젝트 임포트

**AURIX Development Studio에서**:
```
File → Import → Existing Projects into Workspace
  → Select root directory: /Users/jiseong/Desktop/MCUs/tc375_bootloader
  → Import
```

---

## 📡 Ethernet IP 주소 설정 (3가지 방법)

### 방법 1: Static IP (정적 IP) - 권장

**코드로 설정 (`tc375_ethernet_init.c`)**:
```c
/* Static IP Configuration */
#define TC375_IP_ADDR           "192.168.1.10"
#define TC375_NETMASK           "255.255.255.0"
#define TC375_GATEWAY           "192.168.1.1"

/* main.c */
int main(void) {
    tc375_ethernet_init_static();
    tc375_ethernet_print_config();
    // 출력: [Ethernet] IP: 192.168.1.10
}
```

### 방법 2: DHCP (자동 할당)

```c
int main(void) {
    tc375_ethernet_init_dhcp();
    
    /* Wait for IP */
    while (1) {
        tc375_ethernet_process();
        
        uint8_t ip[4];
        tc375_ethernet_get_ip(ip);
        if (ip[0] != 0) break;
        
        delay_ms(100);
    }
}
```

### 방법 3: Runtime 변경 (lwIP API)

```c
#include "lwip/netif.h"

void change_ip_at_runtime(void) {
    ip4_addr_t new_ip, new_netmask, new_gateway;
    
    IP4_ADDR(&new_ip, 192, 168, 1, 20);
    IP4_ADDR(&new_netmask, 255, 255, 255, 0);
    IP4_ADDR(&new_gateway, 192, 168, 1, 1);
    
    netif_set_addr(&tc375_netif, &new_ip, &new_netmask, &new_gateway);
}
```

---

## ⚙️ RTOS (Real-Time Operating System)

### RTOS란?

**정의**: 실시간 운영체제 - 정해진 시간 내에 작업 수행을 보장하는 OS

**기능**:
- ✅ 멀티태스킹 (여러 작업 동시 실행)
- ✅ 우선순위 스케줄링
- ✅ Task 간 동기화 (Mutex, Semaphore)
- ✅ 타이머 관리

### TC375에서 사용 가능한 RTOS

| RTOS | 라이선스 | ADS 지원 | 권장도 |
|------|---------|---------|--------|
| **FreeRTOS** | MIT (무료) | ✅ | ⭐⭐⭐⭐⭐ |
| Zephyr | Apache 2.0 | ⚠️ (포팅 필요) | ⭐⭐⭐ |
| AUTOSAR | 상용 | ✅ | ⭐⭐⭐⭐ (양산용) |
| ThreadX | MIT | ⚠️ | ⭐⭐⭐ |
| µC/OS-III | 상용 | ⚠️ | ⭐⭐ |

### FreeRTOS 설치 (권장)

**AURIX Development Studio에서**:
```
Help → Install New Software
  → Add Repository
  → Location: https://www.freertos.org/eclipse
  → Install "FreeRTOS Kernel"
```

**사용 예제**:
```c
#include "FreeRTOS.h"
#include "task.h"

/* Task 1: Ethernet 처리 */
void task_ethernet(void *pvParameters) {
    while (1) {
        tc375_ethernet_process();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms
    }
}

/* Task 2: DoIP 통신 */
void task_doip(void *pvParameters) {
    while (1) {
        doip_handle_messages();
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms
    }
}

int main(void) {
    /* 하드웨어 초기화 */
    tc375_ethernet_init_static();
    
    /* Task 생성 */
    xTaskCreate(task_ethernet, "ETH", 512, NULL, 3, NULL);
    xTaskCreate(task_doip, "DoIP", 1024, NULL, 2, NULL);
    
    /* 스케줄러 시작 (여기서 멈춤) */
    vTaskStartScheduler();
    
    while (1);  // 도달하지 않음
}
```

---

## 🚀 빌드 및 실행

### 1. 빌드

```
Project → Build All
또는 Ctrl + B
```

### 2. TC375에 업로드

**하드웨어 연결**:
1. TC375 Lite Kit을 USB 케이블로 PC에 연결
2. On-board DAP debugger 자동 인식

**디버그 실행**:
```
Run → Debug As → AURIX C/C++ Application
  → Debugger: DAP Debugger
  → Load Program: ✓
  → Debug 버튼 클릭
```

### 3. 시리얼 콘솔 확인

```
Window → Show View → Terminal
  → Serial Terminal
  → Port: (자동 감지)
  → Baud Rate: 115200
```

**예상 출력**:
```
[TC375] System initialized
[Ethernet] MAC: 00:03:19:45:00:01
[Ethernet] IP:  192.168.1.10
[Ethernet] Link: UP
[DoIP] Ready on port 13400
```

---

## 🧪 테스트

### PC에서 Ping 테스트

```bash
# TC375 IP로 ping
$ ping 192.168.1.10

# 성공 예시:
# 64 bytes from 192.168.1.10: icmp_seq=1 ttl=64 time=0.5 ms
```

### DoIP 연결 테스트

```bash
# TCP 연결 테스트
$ telnet 192.168.1.10 13400

# 또는 netcat
$ nc 192.168.1.10 13400
```

---

## 📚 프로젝트별 IP 할당 예시

```
장치                IP 주소          역할
------------------------------------------------
VMG (MacBook)      192.168.1.1      Gateway/Server
Zonal Gateway #1   192.168.1.10     Zone Controller
End Node ECU #2    192.168.1.20     End ECU
End Node ECU #3    192.168.1.21     End ECU
```

**설정 방법**: `tc375_ethernet_init.c`의 `TC375_IP_ADDR` 수정

---

## ❓ FAQ

### Q1: Ethernet Link가 DOWN 상태입니다

```c
// PHY 초기화 대기 시간 추가
IfxStm_waitTicks(&MODULE_STM0, 300000000); // 300ms
```

### Q2: IP 주소가 0.0.0.0으로 나옵니다

- DHCP 사용 시: DHCP 서버(라우터) 확인
- Static IP 사용 시: `tc375_ethernet_init_static()` 호출 확인

### Q3: FreeRTOS가 설치되지 않습니다

- AURIX Development Studio가 인터넷에 연결되어 있는지 확인
- Eclipse Marketplace 대신 수동 다운로드 사용

### Q4: 빌드 시 lwIP 에러가 발생합니다

```
프로젝트 우클릭 → Configure → Add lwIP Library
```

---

## 📖 자세한 가이드

- **Ethernet 설정**: `AURIX_SETUP_GUIDE.md`
- **RTOS 통합**: `rtos/README.md`
- **DoIP 클라이언트**: `tc375_bootloader/README_DOIP.md`
- **전체 시스템**: `README.md`

---

## 🆘 문제 해결

**Infineon 공식 포럼**:
- https://community.infineon.com/

**프로젝트 Issue 리포트**:
- GitHub Issues: (사용자 제공 리포지토리)

**Contact**:
- 프로젝트 담당자에게 문의

