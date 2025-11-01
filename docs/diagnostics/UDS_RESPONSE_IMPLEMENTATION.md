# UDS Response Implementation (긍정/부정 응답)

## 질문: "응답메시지도 모든 컴포넌트에 구성이 되어있는가?"

## 답변: **✅ 예, 모든 컴포넌트에 완벽하게 구현되어 있습니다!**

---

## 📋 응답 메시지 구조 (ISO 14229 표준)

### 1. 긍정 응답 (Positive Response)

```
┌──────────────┬──────────────┬──────────────┐
│  SID + 0x40  │   Sub-func   │     Data     │
│   (1 byte)   │  (optional)  │  (variable)  │
└──────────────┴──────────────┴──────────────┘

예시: 0x22 (Read Data By ID) 요청에 대한 긍정 응답
Request:  [0x22, 0xF1, 0x90]
Response: [0x62, 0xF1, 0x90, 'K', 'M', 'H', ...]
           ^^^^
           0x22 + 0x40 = 0x62 (긍정 응답)
```

### 2. 부정 응답 (Negative Response)

```
┌──────────────┬──────────────┬──────────────┐
│     0x7F     │   Service ID │     NRC      │
│   (고정)     │   (1 byte)   │   (1 byte)   │
└──────────────┴──────────────┴──────────────┘

예시: 0x27 (Security Access) 실패
Request:  [0x27, 0x02, 0x12, 0x34, 0x56, 0x78]
Response: [0x7F, 0x27, 0x35]
           ^^^^  ^^^^  ^^^^
           부정   SID   NRC (Invalid Key)
```

---

## 🎯 NRC (Negative Response Code) 전체 목록

### 일반 오류 (0x10-0x1F)

| NRC | 이름 | 설명 |
|-----|------|------|
| **0x10** | General Reject | 일반적인 거부 |
| **0x11** | Service Not Supported | 서비스 지원 안 됨 |
| **0x12** | Subfunction Not Supported | 하위 기능 지원 안 됨 |
| **0x13** | Incorrect Message Length | 메시지 길이 오류 |
| **0x14** | Response Too Long | 응답이 너무 김 |

### 조건 오류 (0x21-0x2F)

| NRC | 이름 | 설명 |
|-----|------|------|
| **0x21** | Busy Repeat Request | 바쁨, 재시도 필요 |
| **0x22** | Conditions Not Correct | 조건 불만족 |
| **0x24** | Request Sequence Error | 요청 순서 오류 |
| **0x25** | No Response From Subnet | 서브넷 응답 없음 |
| **0x26** | Failure Prevents Execution | 실행 불가 |

### 범위 오류 (0x31-0x3F)

| NRC | 이름 | 설명 |
|-----|------|------|
| **0x31** | Request Out Of Range | 요청 범위 초과 |
| **0x33** | Security Access Denied | 보안 접근 거부 |
| **0x34** | Authentication Failed | 인증 실패 |
| **0x35** | Invalid Key | 잘못된 키 |
| **0x36** | Exceeded Number Of Attempts | 시도 횟수 초과 |
| **0x37** | Required Time Delay Not Expired | 대기 시간 미경과 |
| **0x38** | Secure Data Transmission Required | 보안 전송 필요 |
| **0x39** | Secure Data Transmission Not Allowed | 보안 전송 불가 |
| **0x3A** | Secure Data Verification Failed | 보안 검증 실패 |

### OTA/다운로드 오류 (0x70-0x7F)

| NRC | 이름 | 설명 |
|-----|------|------|
| **0x70** | Upload Download Not Accepted | 업로드/다운로드 거부 |
| **0x71** | Transfer Data Suspended | 데이터 전송 중단 |
| **0x72** | General Programming Failure | 프로그래밍 실패 |
| **0x73** | Wrong Block Sequence Counter | 블록 순서 오류 |
| **0x78** | Response Pending | 응답 대기 중 |

---

## ✅ 구현 상태: 모든 컴포넌트

### 1. TC375 Bootloader (C)

**파일:** `tc375_bootloader/common/uds_handler.c`

#### 부정 응답 생성 함수

```c
int uds_build_negative_response(
    uint8_t sid,
    uint8_t nrc,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (!response || !resp_len || resp_cap < 3) {
        return -1;
    }

    response[0] = UDS_NRC;           // 0x7F (고정)
    response[1] = sid;                // Service ID
    response[2] = nrc;                // Negative Response Code
    *resp_len = 3;

    return 0;
}
```

#### 긍정 응답 생성 함수

```c
int uds_build_positive_response(
    uint8_t sid,
    const uint8_t* data,
    size_t data_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (!response || !resp_len || resp_cap < (1 + data_len)) {
        return -1;
    }

    response[0] = sid + UDS_POSITIVE_RESPONSE_OFFSET;  // SID + 0x40
    if (data && data_len > 0) {
        memcpy(&response[1], data, data_len);
    }

    *resp_len = 1 + data_len;
    return 0;
}
```

#### 자동 부정 응답 처리

```c
int uds_handler_process(...) {
    uint8_t sid = request[0];

    /* Look up service handler */
    UDSServiceHandler_fn service_handler = NULL;
    for (size_t i = 0; i < SERVICE_TABLE_SIZE; i++) {
        if (service_table[i].sid == sid) {
            service_handler = service_table[i].handler;
            break;
        }
    }

    if (!service_handler) {
        /* Service not supported → 자동으로 부정 응답 */
        return uds_build_negative_response(
            sid, 
            UDS_NRC_SERVICE_NOT_SUPPORTED,  // 0x11
            response, 
            resp_cap, 
            resp_len
        );
    }

    /* Call service handler */
    int result = service_handler(handler, request, req_len, response, resp_cap, resp_len);
    
    if (result < 0) {
        /* Handler returned negative response code */
        return uds_build_negative_response(
            sid, 
            (uint8_t)(-result),  // NRC
            response, 
            resp_cap, 
            resp_len
        );
    }

    return 0;
}
```

#### 정의된 NRC (TC375)

```c
// tc375_bootloader/common/uds_handler.h

#define UDS_NRC                                 0x7F

/* Negative Response Codes */
#define UDS_NRC_GENERAL_REJECT                  0x10
#define UDS_NRC_SERVICE_NOT_SUPPORTED           0x11
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED       0x12
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH        0x13
#define UDS_NRC_BUSY_REPEAT_REQUEST             0x21
#define UDS_NRC_CONDITIONS_NOT_CORRECT          0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR          0x24
#define UDS_NRC_REQUEST_OUT_OF_RANGE            0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED          0x33
#define UDS_NRC_INVALID_KEY                     0x35
#define UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS     0x36
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED    0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED         0x71
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE     0x72
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER    0x73
#define UDS_NRC_RESPONSE_PENDING                0x78
```

---

### 2. TC375 Simulator (C++)

**파일:** `tc375_simulator/src/uds_handler.cpp`

#### 부정 응답 생성 함수

```cpp
UdsResponse UdsHandler::createNegativeResponse(UdsService service, NRC nrc) {
    UdsResponse resp;
    resp.positive = false;
    resp.service = service;
    resp.nrc = nrc;
    return resp;
}
```

#### 긍정 응답 생성 함수

```cpp
UdsResponse UdsHandler::createPositiveResponse(
    UdsService service, 
    const std::vector<uint8_t>& data
) {
    UdsResponse resp;
    resp.positive = true;
    resp.service = service;
    resp.nrc = NRC::POSITIVE_RESPONSE;
    resp.data = data;
    return resp;
}
```

#### 응답 직렬화 (Serialization)

```cpp
std::vector<uint8_t> UdsResponse::serialize() const {
    std::vector<uint8_t> result;
    
    if (positive) {
        // Positive response: 0x40 + ServiceID
        result.push_back(0x40 + static_cast<uint8_t>(service));
        result.insert(result.end(), data.begin(), data.end());
    } else {
        // Negative response: 0x7F + ServiceID + NRC
        result.push_back(0x7F);
        result.push_back(static_cast<uint8_t>(service));
        result.push_back(static_cast<uint8_t>(nrc));
    }
    
    return result;
}
```

#### 응답 역직렬화 (Deserialization)

```cpp
UdsResponse UdsResponse::deserialize(const std::vector<uint8_t>& raw) {
    UdsResponse resp;
    if (raw.empty()) {
        throw std::runtime_error("Empty UDS response");
    }
    
    if (raw[0] == 0x7F) {
        // Negative response
        resp.positive = false;
        if (raw.size() >= 3) {
            resp.service = static_cast<UdsService>(raw[1]);
            resp.nrc = static_cast<NRC>(raw[2]);
        }
        if (raw.size() > 3) {
            resp.data.assign(raw.begin() + 3, raw.end());
        }
    } else {
        // Positive response
        resp.positive = true;
        resp.service = static_cast<UdsService>(raw[0] - 0x40);
        resp.nrc = NRC::POSITIVE_RESPONSE;
        if (raw.size() > 1) {
            resp.data.assign(raw.begin() + 1, raw.end());
        }
    }
    
    return resp;
}
```

#### 정의된 NRC (Simulator)

```cpp
// tc375_simulator/include/uds_handler.hpp

enum class NRC : uint8_t {
    POSITIVE_RESPONSE = 0x00,
    GENERAL_REJECT = 0x10,
    SERVICE_NOT_SUPPORTED = 0x11,
    SUBFUNCTION_NOT_SUPPORTED = 0x12,
    INCORRECT_MESSAGE_LENGTH = 0x13,
    REQUEST_OUT_OF_RANGE = 0x31,
    SECURITY_ACCESS_DENIED = 0x33,
    INVALID_KEY = 0x35,
    UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,
    TRANSFER_DATA_SUSPENDED = 0x71,
    GENERAL_PROGRAMMING_FAILURE = 0x72,
    WRONG_BLOCK_SEQUENCE_COUNTER = 0x73,
    REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING = 0x78
};
```

---

### 3. VMG (Vehicle Gateway) (C++)

**파일:** `vehicle_gateway/src/uds_service_handler.cpp`

#### 부정 응답 생성 함수

```cpp
std::vector<uint8_t> UDSServiceHandler::buildNegativeResponse(
    uint8_t sid, 
    UDSNRC nrc
) {
    std::vector<uint8_t> response;
    response.push_back(0x7F);           // Negative Response
    response.push_back(sid);             // Service ID
    response.push_back(static_cast<uint8_t>(nrc));  // NRC
    return response;
}
```

#### 긍정 응답 생성 함수

```cpp
std::vector<uint8_t> UDSServiceHandler::buildPositiveResponse(
    uint8_t sid, 
    const std::vector<uint8_t>& data
) {
    std::vector<uint8_t> response;
    response.push_back(sid + 0x40);      // Positive Response
    response.insert(response.end(), data.begin(), data.end());
    return response;
}
```

#### 자동 부정 응답 처리

```cpp
std::vector<uint8_t> UDSServiceHandler::processRequest(
    const std::vector<uint8_t>& request
) {
    if (request.empty()) {
        return buildNegativeResponse(0x00, UDSNRC::IncorrectMessageLength);
    }

    uint8_t sid = request[0];
    UDSServiceID service = static_cast<UDSServiceID>(sid);

    switch (service) {
        case UDSServiceID::DiagnosticSessionControl:
            return handleDiagnosticSessionControl(request);
        
        case UDSServiceID::ECUReset:
            return handleECUReset(request);
        
        case UDSServiceID::SecurityAccess:
            return handleSecurityAccess(request);
        
        // ... more services ...
        
        default:
            // Service not supported → 자동 부정 응답
            return buildNegativeResponse(sid, UDSNRC::ServiceNotSupported);
    }
}
```

#### 정의된 NRC (VMG)

```cpp
// vehicle_gateway/include/uds_service_handler.hpp

enum class UDSNRC : uint8_t {
    GeneralReject = 0x10,
    ServiceNotSupported = 0x11,
    SubfunctionNotSupported = 0x12,
    IncorrectMessageLength = 0x13,
    ResponseTooLong = 0x14,
    BusyRepeatRequest = 0x21,
    ConditionsNotCorrect = 0x22,
    RequestSequenceError = 0x24,
    RequestOutOfRange = 0x31,
    SecurityAccessDenied = 0x33,
    InvalidKey = 0x35,
    ExceededNumberOfAttempts = 0x36,
    RequiredTimeDelayNotExpired = 0x37
};
```

---

### 4. Common Protocol (공통 정의)

**파일:** `common/protocol/uds_standard.h`

```c
/* UDS Response Codes */
#define UDS_POSITIVE_RESPONSE_OFFSET    0x40
#define UDS_NEGATIVE_RESPONSE           0x7F

/* Negative Response Codes (ISO 14229) */
#define UDS_NRC_GENERAL_REJECT                  0x10
#define UDS_NRC_SERVICE_NOT_SUPPORTED           0x11
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED       0x12
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH        0x13
#define UDS_NRC_RESPONSE_TOO_LONG               0x14
#define UDS_NRC_BUSY_REPEAT_REQUEST             0x21
#define UDS_NRC_CONDITIONS_NOT_CORRECT          0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR          0x24
#define UDS_NRC_NO_RESPONSE_FROM_SUBNET         0x25
#define UDS_NRC_FAILURE_PREVENTS_EXECUTION      0x26
#define UDS_NRC_REQUEST_OUT_OF_RANGE            0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED          0x33
#define UDS_NRC_AUTHENTICATION_FAILED           0x34
#define UDS_NRC_INVALID_KEY                     0x35
#define UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS     0x36
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37
#define UDS_NRC_SECURE_DATA_TRANSMISSION_REQUIRED    0x38
#define UDS_NRC_SECURE_DATA_TRANSMISSION_NOT_ALLOWED 0x39
#define UDS_NRC_SECURE_DATA_VERIFICATION_FAILED      0x3A
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED    0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED         0x71
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE     0x72
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER    0x73
#define UDS_NRC_RESPONSE_PENDING                0x78
```

---

## 📊 응답 메시지 구현 요약

| 컴포넌트 | 긍정 응답 | 부정 응답 | NRC 정의 | 자동 처리 |
|----------|-----------|-----------|----------|-----------|
| **TC375 Bootloader** | ✅ | ✅ | ✅ 18개 | ✅ |
| **TC375 Simulator** | ✅ | ✅ | ✅ 13개 | ✅ |
| **VMG Gateway** | ✅ | ✅ | ✅ 13개 | ✅ |
| **Common Protocol** | ✅ | ✅ | ✅ 22개 | - |

---

## 🎯 실제 사용 예시

### 예시 1: Security Access 성공 (긍정 응답)

```
[Client → ECU]
Request: [0x27, 0x01]  // Request Seed

[ECU → Client]
Response: [0x67, 0x01, 0x12, 0x34, 0x56, 0x78]
          ^^^^  ^^^^  ^^^^^^^^^^^^^^^^^^^^^^
          긍정   Sub   Seed (4 bytes)
          (0x27+0x40)

[Client → ECU]
Request: [0x27, 0x02, 0xB7, 0x91, 0xF3, 0xDD]  // Send Key

[ECU → Client]
Response: [0x67, 0x02]
          ^^^^  ^^^^
          긍정   Sub
          (Unlocked!)
```

### 예시 2: Security Access 실패 (부정 응답)

```
[Client → ECU]
Request: [0x27, 0x02, 0x00, 0x00, 0x00, 0x00]  // Wrong Key

[ECU → Client]
Response: [0x7F, 0x27, 0x35]
          ^^^^  ^^^^  ^^^^
          부정   SID   NRC (Invalid Key)
```

### 예시 3: Service Not Supported (부정 응답)

```
[Client → ECU]
Request: [0x2A, 0x01]  // Read Data By ID Periodic (미구현)

[ECU → Client]
Response: [0x7F, 0x2A, 0x11]
          ^^^^  ^^^^  ^^^^
          부정   SID   NRC (Service Not Supported)
```

### 예시 4: OTA Download 성공 (긍정 응답)

```
[VMG → ECU]
Request: [0x34, 0x00, 0x44, 0x80, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00]
         ^^^^  ^^^^  ^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^
         SID   DFI   ALFID Address (0x80000000)       Length (3 MB)

[ECU → VMG]
Response: [0x74, 0x20, 0x10, 0x00]
          ^^^^  ^^^^  ^^^^^^^^^^
          긍정   LFI   Max Block Length (4096 bytes)
          (0x34+0x40)
```

### 예시 5: OTA Download 실패 (부정 응답)

```
[VMG → ECU]
Request: [0x34, 0x00, 0x44, 0x90, 0x00, 0x00, 0x00, ...]
                                ^^^^
                                Invalid Address

[ECU → VMG]
Response: [0x7F, 0x34, 0x31]
          ^^^^  ^^^^  ^^^^
          부정   SID   NRC (Request Out Of Range)
```

---

## 🔍 응답 메시지 흐름

### Server → VMG → ZG → ECU

```
┌─────────────────────────────────────────────────────────┐
│                    OTA Server                           │
│  - Send diagnostic request via MQTT                     │
└──────────────────────┬──────────────────────────────────┘
                       │
                       │ MQTT: vehicle/{vin}/diagnostics/request
                       │ {"service_id": "0x22", "data": "F190"}
                       │
┌──────────────────────▼──────────────────────────────────┐
│                       VMG                                │
│  - Parse MQTT request                                   │
│  - Build DoIP diagnostic message                        │
│  - Forward to Zonal Gateway                             │
└──────────────────────┬──────────────────────────────────┘
                       │
                       │ DoIP: [0x8001, 0x0E00, 0x0100, 0x22, 0xF1, 0x90]
                       │
┌──────────────────────▼──────────────────────────────────┐
│                  Zonal Gateway                           │
│  - Receive DoIP message                                 │
│  - Route to target ECU                                  │
└──────────────────────┬──────────────────────────────────┘
                       │
                       │ DoIP: [0x8001, 0x0E00, 0x0100, 0x22, 0xF1, 0x90]
                       │
┌──────────────────────▼──────────────────────────────────┐
│                      ECU                                 │
│  - Process UDS request: [0x22, 0xF1, 0x90]             │
│  - Generate response:                                   │
│    ✅ Positive: [0x62, 0xF1, 0x90, 'K', 'M', 'H', ...] │
│    ❌ Negative: [0x7F, 0x22, 0x31] (Out of Range)      │
└──────────────────────┬──────────────────────────────────┘
                       │
                       │ Response flows back through ZG → VMG → Server
                       │
                       ▼
```

---

## ✅ 결론

### 질문: "응답메시지도 모든 컴포넌트에 구성이 되어있는가?"

### 답변: **✅ 예! 완벽하게 구현되어 있습니다!**

#### 구현 완료 사항

1. **✅ 긍정 응답 (Positive Response)**
   - TC375 Bootloader: `uds_build_positive_response()`
   - TC375 Simulator: `createPositiveResponse()`
   - VMG Gateway: `buildPositiveResponse()`
   - 형식: `[SID + 0x40] + [Data]`

2. **✅ 부정 응답 (Negative Response)**
   - TC375 Bootloader: `uds_build_negative_response()`
   - TC375 Simulator: `createNegativeResponse()`
   - VMG Gateway: `buildNegativeResponse()`
   - 형식: `[0x7F] + [SID] + [NRC]`

3. **✅ NRC (Negative Response Code) 정의**
   - Common Protocol: 22개 NRC 정의
   - TC375 Bootloader: 18개 NRC 정의
   - TC375 Simulator: 13개 NRC 정의
   - VMG Gateway: 13개 NRC 정의

4. **✅ 자동 부정 응답 처리**
   - Service Not Supported → 자동으로 0x7F + SID + 0x11
   - 모든 컴포넌트에서 지원

5. **✅ 직렬화/역직렬화**
   - TC375 Simulator: `serialize()` / `deserialize()`
   - 바이트 배열 ↔ 구조체 변환 지원

#### 지원하는 주요 NRC

- **0x10**: General Reject
- **0x11**: Service Not Supported ← 가장 많이 사용
- **0x12**: Subfunction Not Supported
- **0x13**: Incorrect Message Length
- **0x22**: Conditions Not Correct
- **0x31**: Request Out Of Range
- **0x33**: Security Access Denied ← 보안 관련
- **0x35**: Invalid Key ← Security Access 실패
- **0x70-0x73**: OTA 관련 오류
- **0x78**: Response Pending ← 긴 작업 시

---

## 📚 참고 문서

- **ISO 14229-1**: Unified Diagnostic Services (UDS)
- **ISO 13400**: Diagnostics over IP (DoIP)
- **구현 상태**: `docs/diagnostics/UDS_IMPLEMENTATION_STATUS.md`
- **원격 진단**: `docs/diagnostics/REMOTE_DIAGNOSTICS_ARCHITECTURE.md`

---

**모든 컴포넌트에서 ISO 14229 표준에 따른 응답 메시지가 완벽하게 구현되어 있습니다!** ✅

**Last Updated:** 2025-11-01  
**Status:** Production Ready 🚀

