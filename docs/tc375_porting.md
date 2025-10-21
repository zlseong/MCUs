# TC375 Lite Kit 포팅 가이드

## Phase 2: 실제 TC375 하드웨어 포팅

이 문서는 Mac 시뮬레이터에서 실제 TC375 Lite Kit으로 포팅하는 가이드입니다.

## 사전 준비

### 하드웨어
- TC375 Lite Kit V2
- USB 케이블 (디버깅/플래싱용)
- (선택) DAP (Debug Access Port) 디버거

### 소프트웨어
- **Windows PC** (권장) 또는 Linux
- AURIX Development Studio (ADS)
- TriCore GCC 툴체인
- iLLD (Infineon Low Level Drivers)

## 개발 환경 설정

### 1. ADS 설치
```
1. Infineon 웹사이트에서 ADS 다운로드
2. 설치 및 라이선스 활성화
3. TC375 타겟 추가
```

### 2. 프로젝트 생성
```
File → New → AURIX Project
- Target: TC375TP
- Template: iLLD Base Project
```

## 코드 포팅

### 네트워크 스택

**Mac 시뮬레이터:**
```cpp
#include <openssl/ssl.h>  // POSIX SSL
```

**TC375:**
```cpp
// Option 1: LwIP + mbedTLS
#include "lwip/tcp.h"
#include "mbedtls/ssl.h"

// Option 2: 상용 TCP/IP 스택
```

### 스레딩

**Mac 시뮬레이터:**
```cpp
#include <thread>
std::thread worker_thread_;
```

**TC375:**
```cpp
// Option 1: FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
xTaskCreate(...);

// Option 2: AUTOSAR OS
```

### 파일 시스템

**Mac 시뮬레이터:**
```cpp
std::ifstream file("config.json");
```

**TC375:**
```cpp
// 컴파일 타임 설정 or
// Flash filesystem (SPIFFS, LittleFS)
```

## 통신 스택 구현

### 1. Ethernet 초기화 (TC375)

```c
// iLLD Ethernet 초기화
void init_ethernet(void) {
    // ETH 핀 설정
    IfxEth_Eth_setPinDriver(...);
    
    // PHY 초기화
    IfxEth_Eth_initPhy(...);
    
    // LwIP 초기화
    lwip_init();
}
```

### 2. TLS 클라이언트 (mbedTLS)

```c
mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;

// 초기화
mbedtls_ssl_init(&ssl);
mbedtls_ssl_config_init(&conf);

// TLS 1.3 설정
mbedtls_ssl_conf_min_version(&conf, 
    MBEDTLS_SSL_MAJOR_VERSION_3, 
    MBEDTLS_SSL_MINOR_VERSION_4);
```

### 3. 센서 읽기

```c
// VADC (Analog-Digital Converter)
float read_temperature(void) {
    IfxVadc_Adc_Channel channel;
    // VADC 초기화 및 읽기
    return result * SCALE_FACTOR;
}
```

## 메모리 관리

### Flash 메모리
```
TC375: 6 MB Flash
- 부트로더: 256 KB
- 애플리케이션: 5 MB
- 설정: 512 KB
- OTA 백업: 나머지
```

### RAM
```
TC375: 4.25 MB RAM
- 힙: 1 MB
- 스택: 128 KB
- LwIP: 500 KB
- mbedTLS: 200 KB
- 애플리케이션: 나머지
```

## 빌드 설정

### CMake → TriCore GCC

**시뮬레이터:**
```cmake
set(CMAKE_CXX_COMPILER /usr/bin/c++)
```

**TC375:**
```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER tricore-gcc)
set(CMAKE_CXX_COMPILER tricore-g++)
set(CMAKE_C_FLAGS "-mtc37x -fno-common")
```

## 디버깅

### UART 로그
```c
// UART0를 디버그 출력으로
void debug_printf(const char* fmt, ...) {
    char buffer[256];
    // UART로 전송
    IfxAsclin_Asc_write(&uart, buffer, len);
}
```

### LED 상태
```c
// LED로 상태 표시
#define LED_HEARTBEAT   P13_0
#define LED_CONNECTED   P13_1
#define LED_ERROR       P13_2
```

## 테스트 계획

### Phase 2-1: 기본 통신
1. Ethernet 링크 확인
2. TCP 연결 수립
3. 데이터 송수신

### Phase 2-2: TLS
1. mbedTLS 통합
2. TLS 핸드셰이크
3. 암호화 통신

### Phase 2-3: 프로토콜
1. JSON 파싱 (cJSON)
2. 프로토콜 메시지
3. 통합 테스트

### Phase 2-4: 안정화
1. 재연결 로직
2. 에러 처리
3. 장시간 테스트

## 성능 최적화

### 1. 메모리
- JSON 대신 바이너리 프로토콜 고려
- 정적 버퍼 사용
- 메모리 풀 활용

### 2. CPU
- 센서 읽기 최적화
- DMA 활용
- 인터럽트 기반 처리

### 3. 네트워크
- Nagle 알고리즘 비활성화
- 버퍼 크기 튜닝
- Keep-alive 설정

## 참고 자료

- Infineon TC375 User Manual
- iLLD Documentation
- LwIP Documentation
- mbedTLS Porting Guide
- FreeRTOS TC375 Port

## 도움이 필요한 경우

GitHub Issues: https://github.com/zlseong/MCUs/issues

