# DoIP/UDS Implementation for TC375

Python DoIPMessage 및 DoIPClient 클래스를 C로 포팅한 TC375 MCU용 구현입니다.

## 파일 구조

```
tc375_bootloader/
├── common/
│   ├── doip_message.h         # DoIP 메시지 프레이밍 (헤더/직렬화/파싱)
│   ├── doip_message.c
│   ├── doip_client.h          # DoIP 클라이언트 (TCP/UDP 통신)
│   ├── doip_client.c
│   ├── doip_socket_lwip.c     # 소켓 구현 (lwIP 기반, 포팅 필요)
│   ├── uds_handler.h          # UDS 진단 서비스 핸들러
│   ├── uds_handler.c
│   └── uds_platform_tc375.c   # TC375 플랫폼 종속 구현
└── example_doip_client.c      # 사용 예제
```

## 기능

### 1. DoIP 메시지 프레이밍

Python `DoIPMessage` 클래스와 동일한 기능:

```c
// 메시지 생성
uint8_t buffer[512];
size_t len = doip_build_diagnostic_message(
    0x0E00,           // source_address
    0x0100,           // target_address
    uds_data,         // UDS 요청 데이터
    uds_len,
    buffer,
    sizeof(buffer)
);

// 메시지 파싱
DoIPHeader_t header;
const uint8_t* payload;
doip_parse_message(rx_buffer, rx_len, &header, &payload);
```

### 2. DoIP 클라이언트

Python `DoIPClient` 클래스와 동일한 인터페이스:

```c
DoIPClient_t client;

// 초기화
doip_client_init(&client, "192.168.1.100", 13400, 0x0E00, 0x0100);

// 연결
doip_client_connect(&client);

// 차량 식별 (UDP broadcast)
char vin[18];
doip_client_vehicle_identification(&client, vin);

// 라우팅 활성화
doip_client_routing_activation(&client, 0x00);

// 진단 메시지 전송
uint8_t request[] = {0x10, 0x01};  // Session Control
uint8_t response[256];
size_t resp_len;
doip_client_send_diagnostic(&client, request, 2, response, sizeof(response), &resp_len);

// 연결 종료
doip_client_disconnect(&client);
```

### 3. UDS 서비스 핸들러

서버 측 UDS 요청 처리:

```c
UDSHandler_t handler;
uds_handler_init(&handler);

// UDS 요청 처리
uint8_t request[] = {0x10, 0x02};  // Programming session
uint8_t response[256];
size_t resp_len;
uds_handler_process(&handler, request, 2, response, sizeof(response), &resp_len);
```

지원되는 UDS 서비스:
- **0x10** Diagnostic Session Control
- **0x11** ECU Reset
- **0x27** Security Access
- **0x3E** Tester Present
- **0x22** Read Data By Identifier
- **0x2E** Write Data By Identifier
- **0x34** Request Download (OTA)
- **0x36** Transfer Data (OTA)
- **0x37** Request Transfer Exit (OTA)

## Python 코드와의 비교

### Python (원본)
```python
class DoIPMessage:
    def __init__(self, payload_type, payload_data=b''):
        self.protocol_version = 0x02
        self.inverse_protocol_version = 0xFD
        self.payload_type = payload_type
        self.payload_data = payload_data
        
    def to_bytes(self):
        header = struct.pack('!BBHI', ...)
        return header + self.payload_data

client = DoIPClient('192.168.1.100')
client.connect()
client.routing_activation()
response = client.send_diagnostic_message(b'\x10\x01')
```

### C (포팅)
```c
// 동일한 헤더 포맷 (8바이트, network byte order)
typedef struct {
    uint8_t  protocol_version;          // 0x02
    uint8_t  inverse_protocol_version;  // 0xFD
    uint16_t payload_type;
    uint32_t payload_length;
} DoIPHeader_t;

DoIPClient_t client;
doip_client_init(&client, "192.168.1.100", 13400, 0x0E00, 0x0100);
doip_client_connect(&client);
doip_client_routing_activation(&client, 0x00);
uint8_t req[] = {0x10, 0x01};
doip_client_send_diagnostic(&client, req, 2, resp, sizeof(resp), &resp_len);
```

## 빌드 및 통합

### 1. 부트로더에 통합

`tc375_bootloader/bootloader/stage2_main.c`에 DoIP 클라이언트 추가:

```c
#include "common/doip_client.h"
#include "common/uds_handler.h"

void bootloader_main(void) {
    // ... 기존 초기화 ...
    
    // DoIP 클라이언트 초기화
    DoIPClient_t doip_client;
    doip_client_init(&doip_client, "192.168.1.1", 13400, 0x0100, 0x0100);
    
    // 게이트웨이 연결 대기
    while (1) {
        if (doip_client_connect(&doip_client) == 0) {
            if (doip_client_routing_activation(&doip_client, 0x00) == 0) {
                break;  // 연결 성공
            }
        }
        delay_ms(1000);
    }
    
    // UDS 요청 처리 루프
    while (1) {
        // OTA 업데이트 또는 진단 처리
    }
}
```

### 2. CMakeLists.txt 수정

```cmake
# DoIP/UDS 소스 추가
set(DOIP_SOURCES
    common/doip_message.c
    common/doip_client.c
    common/doip_socket_lwip.c
    common/uds_handler.c
    common/uds_platform_tc375.c
)

add_executable(bootloader
    bootloader/stage2_main.c
    ${DOIP_SOURCES}
)

target_include_directories(bootloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    common/
)
```

### 3. 소켓 구현 포팅

`doip_socket_lwip.c`를 TC375 환경에 맞게 수정:

- **lwIP 사용 시**: lwIP socket API 사용
- **다른 TCP/IP 스택**: 해당 스택의 socket API로 교체
- **베어메탈**: 이더넷 드라이버 직접 구현

```c
// lwIP 헤더 포함
#include "lwip/sockets.h"
#include "lwip/inet.h"

// 또는 다른 스택
#include "your_tcpip_stack.h"
```

### 4. 플랫폼 함수 구현

`uds_platform_tc375.c`의 플랫폼 종속 함수 구현:

```c
void uds_platform_ecu_reset(uint8_t reset_type) {
    // TC375 리셋 구현
    IfxScuWdt_performReset();
}

uint32_t uds_platform_get_tick_ms(void) {
    // STM (System Timer) 사용
    return IfxStm_get(&MODULE_STM0) / (IfxStm_getFrequency(&MODULE_STM0) / 1000);
}

uint32_t uds_platform_generate_seed(void) {
    // HSM 하드웨어 RNG 사용
    return hsm_get_random();
}

int uds_platform_write_firmware(uint32_t address, const uint8_t* data, size_t len) {
    // PFLASH 프로그래밍
    return flash_program(address, data, len);
}
```

## 예제 실행

```bash
# 예제 빌드 (PC에서 테스트용)
cd tc375_bootloader
gcc -o example_client \
    example_doip_client.c \
    common/doip_message.c \
    common/doip_client.c \
    common/doip_socket_lwip.c \
    -I. -Wall

# 실행 (VMG 게이트웨이가 192.168.1.100에서 실행 중이어야 함)
./example_client
```

출력 예시:
```
=== DoIP Client Example ===

1. Performing vehicle identification...
   Vehicle VIN: WBADT43452G296403

2. Connecting to DoIP server...
   Connected successfully

3. Activating routing...
   Routing activated successfully

4. Performing UDS diagnostics...

   [UDS] Session Control (0x10 01)...
   Response (2 bytes): 50 01
   SUCCESS: Session control successful

   [UDS] Read Data By Identifier - VIN (0x22 F190)...
   Response (20 bytes): 62 F1 90 57 42 41 44 54 34 33 34 35 32 47 32 39 36 34 30 33
   VIN: WBADT43452G296403

   [UDS] Read DTC Information (0x19 02 FF)...
   Response (3 bytes): 59 02 00
   DTC read successful

5. Disconnecting...
   Disconnected

=== Example Complete ===
```

## OTA 업데이트 시퀀스

Python 예제의 OTA 로직을 C로 구현:

```c
// 1. Programming Session 진입
uint8_t req_session[] = {0x10, 0x02};
doip_client_send_diagnostic(&client, req_session, 2, resp, sizeof(resp), &resp_len);

// 2. Security Access
uint8_t req_seed[] = {0x27, 0x01};
doip_client_send_diagnostic(&client, req_seed, 2, resp, sizeof(resp), &resp_len);
// ... 키 계산 ...
uint8_t req_key[] = {0x27, 0x02, key[0], key[1], key[2], key[3]};
doip_client_send_diagnostic(&client, req_key, 6, resp, sizeof(resp), &resp_len);

// 3. Request Download
uint8_t req_download[] = {0x34, 0x00, 0x44, 0x82, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00};
doip_client_send_diagnostic(&client, req_download, 10, resp, sizeof(resp), &resp_len);

// 4. Transfer Data (반복)
for (uint8_t seq = 1; firmware_remaining > 0; seq++) {
    uint8_t req_transfer[1024];
    req_transfer[0] = 0x36;
    req_transfer[1] = seq;
    memcpy(&req_transfer[2], firmware_data, block_size);
    doip_client_send_diagnostic(&client, req_transfer, 2 + block_size, resp, sizeof(resp), &resp_len);
}

// 5. Request Transfer Exit
uint8_t req_exit[] = {0x37};
doip_client_send_diagnostic(&client, req_exit, 1, resp, sizeof(resp), &resp_len);

// 6. ECU Reset
uint8_t req_reset[] = {0x11, 0x01};
doip_client_send_diagnostic(&client, req_reset, 2, resp, sizeof(resp), &resp_len);
```

## 메모리 사용량

예상 메모리 사용량 (최적화 전):

- **코드**: ~15KB
- **데이터**: ~12KB (버퍼 포함)
  - DoIP 클라이언트: 8KB (TX/RX 버퍼)
  - UDS 핸들러: 4KB (응답 버퍼)

최적화 팁:
- 버퍼 크기 조정 (`DOIP_MAX_RESPONSE_SIZE`)
- 불필요한 UDS 서비스 제거
- 정적 할당 대신 스택 사용

## 보안 고려사항

1. **Security Access**: `uds_platform_calculate_key()`에 암호학적으로 안전한 알고리즘 사용
2. **Seed 생성**: HSM 하드웨어 RNG 사용
3. **Flash 보호**: 쓰기 전 주소 범위 검증
4. **TLS**: DoIP over TLS 지원 (mbedTLS 통합 필요)

## 다음 단계

1. ✅ DoIP 메시지 프레이밍 구현
2. ✅ DoIP 클라이언트 구현
3. ✅ UDS 핸들러 구현
4. ⏳ 소켓 레이어 TC375 포팅
5. ⏳ Flash 드라이버 통합
6. ⏳ 실제 하드웨어 테스트
7. ⏳ TLS 1.3 + PQC 통합

## 참고 문서

- `docs/doip_tls_architecture.md` - DoIP 통신 아키텍처
- `docs/ISO_13400_specification.md` - DoIP 표준
- `vehicle_gateway/src/doip_server_mbedtls.cpp` - 게이트웨이 DoIP 서버 구현
- `tc375_simulator/src/doip_client_mbedtls.cpp` - 시뮬레이터 구현

## 라이선스

MIT License - 원본 Python 코드와 동일

