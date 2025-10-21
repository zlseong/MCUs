# TC375 ↔ Gateway 통신 프로토콜

## 개요

TC375 디바이스와 Vehicle Gateway 간의 통신 프로토콜입니다.

## 전송 계층

- **프로토콜**: TLS 1.3
- **포트**: 8765 (설정 가능)
- **인코딩**: JSON over TCP

## 메시지 포맷

### 기본 구조

```json
{
  "type": "MESSAGE_TYPE",
  "device_id": "tc375-xxx",
  "payload": {},
  "timestamp": "2025-10-21 00:00:00"
}
```

## 메시지 타입

### 1. HEARTBEAT (디바이스 → 게이트웨이)

주기적으로 전송되는 생존 신호

```json
{
  "type": "HEARTBEAT",
  "device_id": "tc375-sim-001",
  "payload": {
    "status": "alive"
  },
  "timestamp": "2025-10-21 15:30:00"
}
```

**주기**: 기본 10초

### 2. SENSOR_DATA (디바이스 → 게이트웨이)

센서 데이터 전송

```json
{
  "type": "SENSOR_DATA",
  "device_id": "tc375-sim-001",
  "payload": {
    "temperature": 25.5,
    "pressure": 101.3,
    "voltage": 12.0
  },
  "timestamp": "2025-10-21 15:30:05"
}
```

**주기**: 기본 5초

### 3. STATUS_REPORT (디바이스 → 게이트웨이)

디바이스 상태 보고

```json
{
  "type": "STATUS_REPORT",
  "device_id": "tc375-sim-001",
  "payload": {
    "uptime": 3600,
    "memory_usage": 45.2,
    "cpu_usage": 12.5,
    "firmware_version": "1.0.0"
  },
  "timestamp": "2025-10-21 15:30:00"
}
```

### 4. COMMAND_ACK (디바이스 → 게이트웨이)

명령 수신 확인

```json
{
  "type": "COMMAND_ACK",
  "device_id": "tc375-sim-001",
  "payload": {
    "command_id": "cmd-12345",
    "success": true,
    "result": "Command executed successfully"
  },
  "timestamp": "2025-10-21 15:30:10"
}
```

### 5. ERROR (디바이스 → 게이트웨이)

에러 보고

```json
{
  "type": "ERROR",
  "device_id": "tc375-sim-001",
  "payload": {
    "error": "Sensor read timeout",
    "code": 1001
  },
  "timestamp": "2025-10-21 15:30:15"
}
```

## 연결 흐름

```
1. TCP 연결 수립
   TC375 → Gateway (port 8765)

2. TLS 핸드셰이크
   - TLS 1.3
   - (선택) 클라이언트 인증서
   - (선택) PQC Hybrid

3. 등록 메시지
   TC375 → Gateway: STATUS_REPORT (초기 상태)

4. 정상 운영
   - Heartbeat: 10초마다
   - Sensor Data: 5초마다
   - Command ACK: 명령 수신 시

5. 연결 종료
   - Graceful: FIN/ACK
   - 또는 Heartbeat timeout
```

## 에러 처리

### 연결 실패
- 자동 재연결 (5초 간격)
- 최대 재시도: 무제한

### 타임아웃
- Heartbeat 미수신: 30초 → 연결 끊김으로 간주
- 명령 응답 대기: 10초

## 보안

- **암호화**: TLS 1.3 필수
- **인증**: (선택) 클라이언트 인증서
- **무결성**: TLS 레이어에서 보장

## 향후 확장

- CAN 메시지 전달
- OTA 업데이트 프로토콜
- 바이너리 프로토콜 (Protocol Buffers)
- PQC (Post-Quantum Cryptography) 완전 지원

