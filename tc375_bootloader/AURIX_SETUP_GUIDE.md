# TC375 Lite Kit - AURIX Development Studio 설정 가이드

## 1. AURIX Development Studio (ADS) 프로젝트 생성

### 1.1 새 프로젝트 만들기

```
File → New → AURIX C/C++ Project
  → Board: KIT_AURIX_TC375_LITE
  → Processor: TC375TP
  → Toolchain: Tasking/GCC
```

### 1.2 iLLD (Infineon Low-Level Driver) 추가

```
프로젝트 우클릭 → Configure → Add iLLD to Project
  ✓ ETH (Ethernet MAC)
  ✓ PORT (GPIO)
  ✓ SCU (System Control Unit)
  ✓ STM (System Timer)
```

---

## 2. Ethernet IP 주소 설정 (실제 사용 방법)

### 2.1 정적 IP 설정 (Static IP)

**main.c**:
```c
#include "Ifx_Types.h"
#include "tc375_ethernet_init.h"
#include "doip_client.h"

int main(void) {
    /* Initialize system */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    
    /* Initialize Ethernet with Static IP */
    // IP: 192.168.1.10
    // Netmask: 255.255.255.0
    // Gateway: 192.168.1.1
    tc375_ethernet_init_static();
    
    /* Print network configuration */
    tc375_ethernet_print_config();
    
    /* Initialize DoIP client */
    DoIPClient_t doip_client;
    doip_client_init(&doip_client, "192.168.1.1", 13400, 0x0100, 0x0100);
    doip_client_connect(&doip_client);
    doip_client_routing_activation(&doip_client, 0x00);
    
    /* Main loop */
    while (1) {
        /* Process lwIP timers */
        tc375_ethernet_process();
        
        /* Your application code */
        IfxStm_waitTicks(&MODULE_STM0, 1000000); // 1ms delay
    }
    
    return 0;
}
```

### 2.2 DHCP 설정 (동적 IP)

```c
int main(void) {
    /* Initialize Ethernet with DHCP */
    tc375_ethernet_init_dhcp();
    
    /* Wait for DHCP to assign IP */
    while (1) {
        tc375_ethernet_process();
        
        uint8_t ip[4];
        tc375_ethernet_get_ip(ip);
        
        if (ip[0] != 0) {
            printf("[DHCP] IP assigned: %d.%d.%d.%d\n", 
                   ip[0], ip[1], ip[2], ip[3]);
            break;
        }
        
        IfxStm_waitTicks(&MODULE_STM0, 100000000); // 100ms
    }
    
    /* Continue with application */
    while (1) {
        tc375_ethernet_process();
    }
}
```

### 2.3 IP 주소 커스터마이징

**tc375_ethernet_init.c** 파일에서 수정:

```c
/* Static IP Configuration - 여기를 수정하세요! */
#define TC375_IP_ADDR           "192.168.1.10"   // ECU IP
#define TC375_NETMASK           "255.255.255.0"  // Subnet
#define TC375_GATEWAY           "192.168.1.1"    // VMG Gateway IP

/* MAC Address - 각 ECU마다 다르게! */
#define TC375_MAC_ADDR_0        0x00
#define TC375_MAC_ADDR_1        0x03
#define TC375_MAC_ADDR_2        0x19
#define TC375_MAC_ADDR_3        0x45
#define TC375_MAC_ADDR_4        0x00
#define TC375_MAC_ADDR_5        0x01  // ECU#1 = 0x01, ECU#2 = 0x02
```

---

## 3. lwIP TCP/IP 스택 통합

### 3.1 lwIP 라이브러리 추가

**Option 1: AURIX Development Studio에서 자동 추가**
```
프로젝트 우클릭 → Configure → Add lwIP Library
  Version: 2.1.3 (최신 버전)
```

**Option 2: 수동 다운로드**
```bash
cd Libraries/
git clone https://git.savannah.nongnu.org/git/lwip.git
cd lwip
git checkout STABLE-2_1_3_RELEASE
```

### 3.2 lwipopts.h 설정 (프로젝트 루트)

```c
/* lwipopts.h - lwIP Configuration */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* Platform-specific */
#define NO_SYS                  1    // 0 = with RTOS, 1 = bare metal
#define SYS_LIGHTWEIGHT_PROT    1

/* Memory options */
#define MEM_ALIGNMENT           4
#define MEM_SIZE                (64*1024)  // 64KB heap

/* MEMP options */
#define MEMP_NUM_PBUF           16
#define MEMP_NUM_TCP_PCB        16
#define MEMP_NUM_TCP_SEG        32
#define MEMP_NUM_UDP_PCB        8

/* Pbuf options */
#define PBUF_POOL_SIZE          16
#define PBUF_POOL_BUFSIZE       1536

/* TCP options */
#define LWIP_TCP                1
#define TCP_MSS                 1460
#define TCP_WND                 (4 * TCP_MSS)
#define TCP_SND_BUF             (4 * TCP_MSS)

/* UDP options */
#define LWIP_UDP                1

/* DHCP options */
#define LWIP_DHCP               1     // Enable DHCP
#define LWIP_NETIF_STATUS_CALLBACK 1

/* DNS options */
#define LWIP_DNS                0     // Not needed for DoIP

/* Statistics */
#define LWIP_STATS              1
#define LWIP_STATS_DISPLAY      1

/* Debugging */
#define LWIP_DEBUG              1
#define ETHARP_DEBUG            LWIP_DBG_ON
#define NETIF_DEBUG             LWIP_DBG_ON
#define IP_DEBUG                LWIP_DBG_ON
#define TCP_DEBUG               LWIP_DBG_ON

#endif /* LWIPOPTS_H */
```

---

## 4. FreeRTOS 통합 (권장)

### 4.1 FreeRTOS 라이브러리 추가

**AURIX Development Studio**:
```
Help → Install New Software
  → Add → Location: https://www.freertos.org/eclipse
  → Install "FreeRTOS Kernel"
```

### 4.2 FreeRTOSConfig.h 설정

```c
/* FreeRTOSConfig.h */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* TC375 specific */
#define configCPU_CLOCK_HZ              300000000UL  // 300 MHz
#define configTICK_RATE_HZ              1000         // 1ms tick
#define configUSE_PREEMPTION            1
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0

/* Memory allocation */
#define configTOTAL_HEAP_SIZE           (64 * 1024)  // 64KB
#define configMINIMAL_STACK_SIZE        256

/* Task priorities */
#define configMAX_PRIORITIES            8
#define configMAX_TASK_NAME_LEN         16

/* Synchronization */
#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     1
#define configUSE_COUNTING_SEMAPHORES   1
#define configUSE_QUEUE_SETS            1

/* Software timers */
#define configUSE_TIMERS                1
#define configTIMER_TASK_PRIORITY       (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH        10
#define configTIMER_TASK_STACK_DEPTH    512

/* Co-routines */
#define configUSE_CO_ROUTINES           0

/* lwIP integration */
#define configUSE_COUNTING_SEMAPHORES   1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_vTaskDelayUntil         1

#endif /* FREERTOS_CONFIG_H */
```

### 4.3 FreeRTOS + lwIP 통합 예제

```c
#include "FreeRTOS.h"
#include "task.h"
#include "tc375_ethernet_init.h"
#include "doip_client.h"

/* Task priorities */
#define PRIORITY_ETHERNET   (tskIDLE_PRIORITY + 3)
#define PRIORITY_DOIP       (tskIDLE_PRIORITY + 2)
#define PRIORITY_APP        (tskIDLE_PRIORITY + 1)

/* Task handles */
TaskHandle_t task_ethernet_handle;
TaskHandle_t task_doip_handle;

/* Ethernet task (lwIP timer processing) */
void task_ethernet(void *pvParameters) {
    while (1) {
        tc375_ethernet_process();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms
    }
}

/* DoIP communication task */
void task_doip(void *pvParameters) {
    DoIPClient_t doip_client;
    
    /* Wait for network initialization */
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    /* Connect to VMG */
    doip_client_init(&doip_client, "192.168.1.1", 13400, 0x0100, 0x0100);
    doip_client_connect(&doip_client);
    doip_client_routing_activation(&doip_client, 0x00);
    
    while (1) {
        /* Send Tester Present (heartbeat) */
        uint8_t uds_tester_present[] = {0x3E, 0x00};
        doip_client_send_diagnostic(&doip_client, uds_tester_present, 2);
        
        vTaskDelay(pdMS_TO_TICKS(2000));  // Every 2 seconds
    }
}

int main(void) {
    /* Disable watchdog */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    
    /* Initialize Ethernet */
    tc375_ethernet_init_static();
    tc375_ethernet_print_config();
    
    /* Create tasks */
    xTaskCreate(task_ethernet, "ETH", 512, NULL, PRIORITY_ETHERNET, &task_ethernet_handle);
    xTaskCreate(task_doip, "DoIP", 1024, NULL, PRIORITY_DOIP, &task_doip_handle);
    
    /* Start scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    while (1);
    
    return 0;
}
```

---

## 5. 빌드 및 플래시

### 5.1 빌드

```
Project → Build Project
또는
Ctrl + B
```

### 5.2 플래시 (TC375 Lite Kit에 업로드)

**디버거 연결**:
- TC375 Lite Kit을 USB로 PC에 연결
- On-board debugger 자동 인식됨

**플래시 실행**:
```
Run → Debug As → AURIX C/C++ Application
  → Debugger: DAP Debugger (on-board)
  → Load 체크
  → Debug 실행
```

### 5.3 시리얼 콘솔 확인

```
Window → Show View → Terminal
  → Settings:
    Connection Type: Serial
    Port: (자동 감지된 포트)
    Baud Rate: 115200
    Data Bits: 8
    Parity: None
    Stop Bits: 1
```

**예상 출력**:
```
[Ethernet] MAC: 00:03:19:45:00:01
[Ethernet] IP:  192.168.1.10
[Ethernet] Link: UP
[DoIP] Connecting to VMG (192.168.1.1:13400)...
[DoIP] Connected!
[DoIP] Routing activation success
```

---

## 6. 트러블슈팅

### 문제 1: Link DOWN 상태

```c
// Ethernet 케이블이 연결되었는지 확인
// PHY 초기화 대기 시간 추가
IfxStm_waitTicks(&MODULE_STM0, 300000000); // 300ms wait
```

### 문제 2: IP 주소 충돌

```bash
# PC에서 ARP 캐시 클리어
$ arp -d 192.168.1.10

# ping 테스트
$ ping 192.168.1.10
```

### 문제 3: lwIP 메모리 부족

```c
// lwipopts.h에서 메모리 증가
#define MEM_SIZE                (128*1024)  // 64KB → 128KB
#define MEMP_NUM_PBUF           32          // 16 → 32
```

---

## 7. 다음 단계

1. ✅ Ethernet IP 설정 완료
2. ✅ lwIP TCP/IP 스택 통합
3. ✅ FreeRTOS 통합 (선택)
4. 🔜 DoIP 클라이언트 통합
5. 🔜 VMG와 통신 테스트
6. 🔜 OTA 업데이트 구현

---

## 참고자료

- **AURIX Development Studio**: https://www.infineon.com/cms/en/tools/aurix-tools/
- **TC375 User Manual**: https://www.infineon.com/dgdl/TC37x_um_v1.1.pdf
- **iLLD API Reference**: (ADS 설치 경로)/Help/iLLD_Documentation
- **lwIP Documentation**: https://www.nongnu.org/lwip/
- **FreeRTOS**: https://www.freertos.org/

