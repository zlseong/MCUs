# Remote Diagnostics Usage Guide

## 원격 진단 사용 방법

---

## 1. 시스템 구성

```
[Admin PC] → [OTA Server] → [VMG] → [Zonal Gateway] → [ECU]
```

---

## 2. API 엔드포인트

### A. 단일 ECU 진단

```bash
curl -X POST http://ota-server:8443/api/v1/diagnostics/send \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "KMHGH4JH1NU123456",
    "ecu_id": "ECU_001",
    "service_id": "0x22",
    "data": "F190"
  }'
```

**응답:**
```json
{
  "status": "sent",
  "request_id": "diag-a1b2c3d4",
  "vin": "KMHGH4JH1NU123456",
  "ecu_id": "ECU_001",
  "message": "Diagnostic request sent to VMG"
}
```

---

### B. 결과 조회

```bash
curl http://ota-server:8443/api/v1/diagnostics/results/diag-a1b2c3d4
```

**응답:**
```json
{
  "request_id": "diag-a1b2c3d4",
  "status": "completed",
  "response_data": "62F190KMHGH4JH1NU123456",
  "decoded": {
    "service": "ReadDataByIdentifier",
    "did": "0xF190",
    "value": "KMHGH4JH1NU123456"
  }
}
```

---

### C. Zone 브로드캐스트

```bash
curl -X POST http://ota-server:8443/api/v1/diagnostics/broadcast \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "KMHGH4JH1NU123456",
    "zone_id": "ZG_POWERTRAIN",
    "service_id": "0x19",
    "data": "02FF"
  }'
```

**응답:**
```json
{
  "status": "sent",
  "request_id": "diag-broadcast-x9y8z7w6",
  "vin": "KMHGH4JH1NU123456",
  "zone_id": "ZG_POWERTRAIN",
  "message": "Broadcast diagnostic request sent to VMG"
}
```

---

### D. 지원 서비스 목록

```bash
curl http://ota-server:8443/api/v1/diagnostics/supported-services
```

**응답:**
```json
{
  "services": [
    {
      "sid": "0x10",
      "name": "DiagnosticSessionControl",
      "description": "Switch diagnostic session"
    },
    {
      "sid": "0x22",
      "name": "ReadDataByIdentifier",
      "description": "Read data by DID"
    }
  ],
  "common_dids": [
    {
      "did": "0xF190",
      "name": "VIN",
      "description": "Vehicle Identification Number"
    }
  ]
}
```

---

## 3. 사용 예시

### 예시 1: VIN 읽기

```bash
# Request
curl -X POST http://ota-server:8443/api/v1/diagnostics/send \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "KMHGH4JH1NU123456",
    "ecu_id": "ECU_001",
    "service_id": "0x22",
    "data": "F190"
  }'

# Response
{
  "request_id": "diag-12345",
  "status": "sent"
}

# Wait 2-5 seconds

# Get results
curl http://ota-server:8443/api/v1/diagnostics/results/diag-12345

# Response
{
  "request_id": "diag-12345",
  "status": "completed",
  "response_data": "62F190KMHGH4JH1NU123456",
  "decoded": {
    "service": "ReadDataByIdentifier",
    "did": "0xF190",
    "value": "KMHGH4JH1NU123456"
  }
}
```

---

### 예시 2: 소프트웨어 버전 읽기

```bash
curl -X POST http://ota-server:8443/api/v1/diagnostics/send \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "KMHGH4JH1NU123456",
    "ecu_id": "ECU_001",
    "service_id": "0x22",
    "data": "F195"
  }'
```

**예상 응답:**
```
62F195 01 02 03  → Version 1.2.3
```

---

### 예시 3: DTC 읽기

```bash
curl -X POST http://ota-server:8443/api/v1/diagnostics/send \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "KMHGH4JH1NU123456",
    "ecu_id": "ECU_001",
    "service_id": "0x19",
    "data": "02FF"
  }'
```

**예상 응답:**
```
590202 P0001 P0002  → 2 DTCs found
```

---

### 예시 4: ECU 리셋

```bash
curl -X POST http://ota-server:8443/api/v1/diagnostics/send \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "KMHGH4JH1NU123456",
    "ecu_id": "ECU_001",
    "service_id": "0x11",
    "data": "01"
  }'
```

**예상 응답:**
```
5101  → Hard Reset Positive Response
```

---

### 예시 5: Zone 전체 DTC 조회

```bash
curl -X POST http://ota-server:8443/api/v1/diagnostics/broadcast \
  -H "Content-Type: application/json" \
  -d '{
    "vin": "KMHGH4JH1NU123456",
    "zone_id": "ZG_POWERTRAIN",
    "service_id": "0x19",
    "data": "02FF"
  }'
```

**예상 응답 (집계):**
```json
{
  "request_id": "diag-broadcast-12345",
  "results": {
    "ECU_001": {
      "status": "success",
      "response": "590202P0001P0002"
    },
    "ECU_002": {
      "status": "success",
      "response": "590200"
    },
    "ECU_003": {
      "status": "error",
      "error": "Timeout"
    }
  }
}
```

---

## 4. UDS 서비스 상세

### 0x10: Diagnostic Session Control

**용도:** 진단 세션 전환

```bash
# Default Session
"service_id": "0x10", "data": "01"

# Programming Session
"service_id": "0x10", "data": "02"

# Extended Session
"service_id": "0x10", "data": "03"
```

---

### 0x11: ECU Reset

**용도:** ECU 재부팅

```bash
# Hard Reset
"service_id": "0x11", "data": "01"

# Soft Reset
"service_id": "0x11", "data": "03"
```

---

### 0x22: Read Data By Identifier

**용도:** DID로 데이터 읽기

```bash
# VIN (0xF190)
"service_id": "0x22", "data": "F190"

# Serial Number (0xF18C)
"service_id": "0x22", "data": "F18C"

# Software Version (0xF195)
"service_id": "0x22", "data": "F195"
```

---

### 0x19: Read DTC Information

**용도:** DTC 읽기

```bash
# Report Number of DTC
"service_id": "0x19", "data": "01FF"

# Report DTC by Status Mask
"service_id": "0x19", "data": "02FF"
```

---

### 0x27: Security Access

**용도:** 보안 레벨 상승

```bash
# Request Seed (Level 1)
"service_id": "0x27", "data": "01"

# Send Key (Level 1)
"service_id": "0x27", "data": "02XXXXXXXX"
```

---

### 0x2E: Write Data By Identifier

**용도:** DID로 데이터 쓰기

```bash
# Write Configuration
"service_id": "0x2E", "data": "F199010203"
```

---

### 0x31: Routine Control

**용도:** 루틴 실행

```bash
# Start Routine (0xFF00)
"service_id": "0x31", "data": "01FF00"

# Stop Routine
"service_id": "0x31", "data": "02FF00"

# Request Results
"service_id": "0x31", "data": "03FF00"
```

---

### 0x3E: Tester Present

**용도:** 세션 유지

```bash
# Keep session alive
"service_id": "0x3E", "data": "00"
```

---

## 5. 에러 처리

### Timeout

```json
{
  "request_id": "diag-12345",
  "status": "timeout",
  "error": "ECU did not respond within 5 seconds"
}
```

---

### Negative Response

```json
{
  "request_id": "diag-12345",
  "status": "negative_response",
  "nrc": "0x33",
  "nrc_description": "Security Access Denied"
}
```

**주요 NRC:**
- `0x11`: Service Not Supported
- `0x12`: Sub-Function Not Supported
- `0x13`: Incorrect Message Length
- `0x22`: Conditions Not Correct
- `0x33`: Security Access Denied
- `0x31`: Request Out Of Range

---

### Network Error

```json
{
  "request_id": "diag-12345",
  "status": "network_error",
  "error": "VMG connection lost"
}
```

---

## 6. Python 예제

```python
import requests
import time

# OTA Server URL
BASE_URL = "http://ota-server:8443/api/v1"

def read_vin(vin, ecu_id):
    """Read VIN from ECU"""
    
    # Send request
    response = requests.post(
        f"{BASE_URL}/diagnostics/send",
        json={
            "vin": vin,
            "ecu_id": ecu_id,
            "service_id": "0x22",
            "data": "F190"
        }
    )
    
    if response.status_code != 200:
        print(f"Error: {response.text}")
        return None
    
    request_id = response.json()["request_id"]
    print(f"Request sent: {request_id}")
    
    # Wait for response
    time.sleep(2)
    
    # Get results
    response = requests.get(f"{BASE_URL}/diagnostics/results/{request_id}")
    
    if response.status_code != 200:
        print(f"Error: {response.text}")
        return None
    
    result = response.json()
    
    if result["status"] == "completed":
        print(f"VIN: {result['decoded']['value']}")
        return result['decoded']['value']
    else:
        print(f"Status: {result['status']}")
        return None

# Usage
if __name__ == "__main__":
    vin = read_vin("KMHGH4JH1NU123456", "ECU_001")
```

---

## 7. 보안 고려사항

### 1. 인증
- 모든 요청은 PQC-TLS로 암호화
- 서버 인증서 검증 필수

### 2. 권한 관리
- Level 1: 읽기 전용 (0x22, 0x19)
- Level 2: 쓰기 가능 (0x2E)
- Level 3: 제어 가능 (0x31, 0x11)

### 3. 감사 로그
- 모든 진단 요청/응답 기록
- 타임스탬프, 사용자, ECU, 서비스 ID 저장

---

## 8. 성능 고려사항

### 타임아웃
- 단일 요청: 5초
- 브로드캐스트: 10초
- 재시도: 최대 3회

### 동시 요청
- 최대 동시 요청: 16개
- Queue 크기: 64개

### 네트워크 지연
- Server → VMG: ~100ms
- VMG → ZG: ~50ms
- ZG → ECU: ~50ms
- 총 RTT: ~400ms

---

## 9. 문제 해결

### Q: 응답이 없어요
**A:** 
1. ECU가 연결되어 있는지 확인
2. Zonal Gateway가 실행 중인지 확인
3. VMG가 MQTT에 연결되어 있는지 확인

### Q: Negative Response가 와요
**A:**
1. 서비스 ID가 올바른지 확인
2. 보안 레벨이 충분한지 확인 (0x27 먼저 실행)
3. 진단 세션이 올바른지 확인 (0x10 먼저 실행)

### Q: Timeout이 자주 발생해요
**A:**
1. 네트워크 상태 확인
2. ECU 부하 확인
3. Timeout 값 증가 고려

---

## 10. 다음 단계

1. ✅ Server API 구현 완료
2. ✅ VMG Handler 구현 완료
3. ✅ ZG Router 구현 완료
4. ✅ ECU Handler 이미 구현됨
5. ⏳ MQTT 통합 (TODO)
6. ⏳ 데이터베이스 저장 (TODO)
7. ⏳ Web UI 개발 (TODO)

---

**원격 진단 시스템 구현 완료!** ✅

