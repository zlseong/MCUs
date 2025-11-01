# VMG Dynamic IP - Quick Start Guide

## 시나리오: 차량이 이동하면서 IP가 변경되는 상황

---

## 1. 문제 상황

```
차량 A (서울)          → 차량 B (부산)
IP: 10.1.1.100         → IP: 10.5.9.200
기지국: Tower-Seoul    → 기지국: Tower-Busan
```

**기존 방식 (❌):**
- IP 변경 시 TCP 연결 끊김
- 서버는 새 IP를 모름
- OTA 업데이트 중단

**새로운 방식 (✅):**
- MQTT Clean Session = False
- 자동 재연결
- 세션 지속
- 메시지 큐잉

---

## 2. 핵심 원리

### A. 서버는 고정, VMG는 클라이언트

```
┌──────────────────────────────────────┐
│  OTA Server (고정)                    │
│  ota-server.company.com              │
│  IP: 52.78.123.45                    │
│  Port: 8883 (MQTT over TLS)          │
└──────────────┬───────────────────────┘
               │
               │ ← VMG가 항상 서버에 연결
               │    (서버는 VMG IP를 몰라도 됨!)
               │
┌──────────────▼───────────────────────┐
│  VMG (가변 IP)                        │
│  IP: 10.x.x.x (계속 변경됨)           │
│  Client ID: vehicle-VIN123456        │
└──────────────────────────────────────┘
```

### B. Clean Session = False

```cpp
// 서버가 세션을 기억함!
MQTTClient_connectOptions opts;
opts.cleansession = 0;  // ← 핵심!

/**
 * 효과:
 * 1. 연결 끊겨도 세션 유지
 * 2. QoS 1/2 메시지 큐에 보관
 * 3. 재연결 시 자동 전달
 * 4. 구독 정보도 유지
 */
```

---

## 3. 구현 방법

### Step 1: MQTT Client 생성

```cpp
#include "mqtt_client_persistent.hpp"

// Create client
vmg::MQTTClientPersistent mqtt_client(
    "ssl://ota-server.company.com:8883",
    "vehicle-KMHGH4JH1NU123456"  // VIN을 Client ID로 사용
);

// Initialize with TLS certificates
mqtt_client.initialize(
    "/etc/vmg/certs/vmg-client.crt",
    "/etc/vmg/certs/vmg-client.key",
    "/etc/vmg/certs/ca.crt"
);

// Configure reconnection
mqtt_client.setReconnectionParams(
    1000,   // Initial delay: 1 second
    60000,  // Max delay: 60 seconds
    2       // Exponential multiplier
);

// Start connection manager
mqtt_client.start();
```

---

### Step 2: Subscribe to Topics

```cpp
// Subscribe to OTA updates
mqtt_client.subscribe(
    "vehicle/KMHGH4JH1NU123456/ota/update",
    [](const std::string& topic, const std::string& payload) {
        std::cout << "OTA Update received: " << payload << std::endl;
        // Process OTA update...
    },
    1  // QoS 1
);

// Subscribe to diagnostics requests
mqtt_client.subscribe(
    "vehicle/KMHGH4JH1NU123456/diagnostics/request",
    [](const std::string& topic, const std::string& payload) {
        std::cout << "Diagnostic request: " << payload << std::endl;
        // Process diagnostic request...
    },
    1  // QoS 1
);

// Subscribe to commands
mqtt_client.subscribe(
    "vehicle/KMHGH4JH1NU123456/commands/#",  // Wildcard
    [](const std::string& topic, const std::string& payload) {
        std::cout << "Command received: " << topic << std::endl;
        // Process command...
    },
    1  // QoS 1
);
```

---

### Step 3: Publish Messages

```cpp
// Publish status (QoS 1 for reliability)
mqtt_client.publish(
    "vehicle/KMHGH4JH1NU123456/status",
    "{\"status\":\"online\",\"battery\":85}",
    1  // QoS 1 - guaranteed delivery
);

// Publish diagnostic response
mqtt_client.publish(
    "vehicle/KMHGH4JH1NU123456/diagnostics/response",
    "{\"request_id\":\"diag-12345\",\"response\":\"62F190...\"}",
    1  // QoS 1
);
```

---

### Step 4: Handle IP Changes (자동)

```cpp
// IP 변경은 자동으로 처리됨!
// 백그라운드 스레드가 계속 감시

while (true) {
    // 1. 연결 상태 확인
    if (!mqtt_client.isConnected()) {
        std::cout << "Disconnected. Reconnecting..." << std::endl;
    }
    
    // 2. IP 변경 감지
    // (자동으로 reconnect 시도)
    
    // 3. 재연결 성공
    // (큐에 있던 메시지 자동 전송)
    
    sleep(10);
}
```

---

## 4. 실전 예제: OTA 업데이트 중 IP 변경

### 시나리오

```
[0초]  VMG: 10.1.1.100으로 연결
       Server → VMG: "firmware_chunk_1" (QoS 1)
       VMG → Server: ACK

[5초]  Server → VMG: "firmware_chunk_2" (QoS 1)
       VMG → Server: ACK

[10초] ⚠️ 차량 이동! 기지국 변경
       IP: 10.1.1.100 → 10.2.5.200
       TCP 연결 끊김!

[11초] Server → VMG: "firmware_chunk_3" (전송 실패)
       → 서버의 MQTT Broker가 메시지 큐에 보관

[12초] VMG: 새 IP (10.2.5.200)로 재연결 시도
       CONNECT packet:
         - Client ID: vehicle-VIN123456
         - Clean Session: 0 (세션 유지!)

[13초] Server: 세션 복원 확인
       → 큐에 있던 "firmware_chunk_3" 확인

[14초] Server → VMG: "firmware_chunk_3" (재전송!)
       VMG → Server: ACK

[15초] OTA 업데이트 정상 진행...
       Server → VMG: "firmware_chunk_4"
```

---

## 5. 코드 예제 (Complete)

```cpp
// vehicle_gateway/src/main_with_persistent_mqtt.cpp

#include "mqtt_client_persistent.hpp"
#include <iostream>
#include <signal.h>

static bool is_running = true;

void signal_handler(int signal) {
    std::cout << "Shutting down..." << std::endl;
    is_running = false;
}

int main() {
    signal(SIGINT, signal_handler);
    
    // 1. Create MQTT client
    std::string vin = "KMHGH4JH1NU123456";
    vmg::MQTTClientPersistent mqtt_client(
        "ssl://ota-server.company.com:8883",
        "vehicle-" + vin
    );
    
    // 2. Initialize
    if (!mqtt_client.initialize(
        "/etc/vmg/certs/vmg-client.crt",
        "/etc/vmg/certs/vmg-client.key",
        "/etc/vmg/certs/ca.crt"
    )) {
        std::cerr << "Failed to initialize MQTT client" << std::endl;
        return 1;
    }
    
    // 3. Configure reconnection
    mqtt_client.setReconnectionParams(1000, 60000, 2);
    mqtt_client.setMessageQueueLimit(100);  // Max 100 messages in queue
    
    // 4. Start connection manager
    mqtt_client.start();
    
    // 5. Subscribe to topics
    mqtt_client.subscribe(
        "vehicle/" + vin + "/ota/update",
        [](const std::string& topic, const std::string& payload) {
            std::cout << "[OTA] Received: " << payload.substr(0, 50) << "..." << std::endl;
            // Handle OTA update
        },
        1
    );
    
    mqtt_client.subscribe(
        "vehicle/" + vin + "/diagnostics/request",
        [&mqtt_client, vin](const std::string& topic, const std::string& payload) {
            std::cout << "[Diag] Request: " << payload << std::endl;
            
            // Process diagnostic request...
            std::string response = "{\"result\":\"success\"}";
            
            // Send response
            mqtt_client.publish(
                "vehicle/" + vin + "/diagnostics/response",
                response,
                1
            );
        },
        1
    );
    
    // 6. Main loop
    while (is_running) {
        // Publish heartbeat
        mqtt_client.publish(
            "vehicle/" + vin + "/heartbeat",
            "{\"status\":\"online\",\"timestamp\":\"" + getCurrentTimestamp() + "\"}",
            1
        );
        
        // Print statistics
        auto stats = mqtt_client.getStats();
        std::cout << "Stats: "
                  << "Connected=" << stats.is_connected << ", "
                  << "Reconnections=" << stats.reconnections << ", "
                  << "Sent=" << stats.messages_sent << ", "
                  << "Received=" << stats.messages_received << ", "
                  << "Queued=" << stats.messages_queued << std::endl;
        
        // Sleep
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
    
    // 7. Cleanup
    mqtt_client.stop();
    
    return 0;
}
```

---

## 6. 서버 설정 (Mosquitto 예제)

```conf
# /etc/mosquitto/mosquitto.conf

# TLS Configuration
listener 8883
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate true

# Session Persistence (중요!)
persistence true
persistence_location /var/lib/mosquitto/
autosave_interval 300

# QoS Settings
max_queued_messages 1000
max_inflight_messages 20

# Client Settings
max_connections 10000
max_keepalive 3600

# Logging
log_dest file /var/log/mosquitto/mosquitto.log
log_type all
connection_messages true
```

---

## 7. 테스트

### A. 정상 연결 테스트

```bash
# VMG 측
./vmg_gateway

# 출력:
# [MQTT] Connecting to ssl://ota-server.company.com:8883
# [MQTT] Connected successfully!
# [MQTT] Subscribed to vehicle/VIN123456/ota/update
# [MQTT] Subscribed to vehicle/VIN123456/diagnostics/request
```

---

### B. IP 변경 시뮬레이션

```bash
# VMG 측 (네트워크 인터페이스 재시작)
sudo ifconfig eth0 down
sleep 2
sudo ifconfig eth0 up

# 출력:
# [MQTT] Connection lost. Reconnecting...
# [MQTT] Local IP changed: 10.1.1.100 → 10.2.5.200
# [MQTT] Attempting connection (delay: 1000ms)
# [MQTT] Connected successfully!
# [MQTT] Processing queued messages (3 messages)
# [MQTT] Queued message sent: vehicle/VIN123456/heartbeat
```

---

### C. 서버 측 메시지 전송

```bash
# 서버 측 (mosquitto_pub)
mosquitto_pub -h ota-server.company.com -p 8883 \
  --cafile ca.crt --cert admin.crt --key admin.key \
  -i "admin-client" \
  -t "vehicle/VIN123456/diagnostics/request" \
  -m '{"service_id":"0x22","data":"F190"}' \
  -q 1

# VMG 측 출력:
# [Diag] Request: {"service_id":"0x22","data":"F190"}
# [Diag] Sending response...
```

---

## 8. 통계 확인

```cpp
auto stats = mqtt_client.getStats();

std::cout << "Connection Statistics:" << std::endl;
std::cout << "  Total connections: " << stats.total_connections << std::endl;
std::cout << "  Failed connections: " << stats.failed_connections << std::endl;
std::cout << "  Reconnections: " << stats.reconnections << std::endl;
std::cout << "  Messages sent: " << stats.messages_sent << std::endl;
std::cout << "  Messages received: " << stats.messages_received << std::endl;
std::cout << "  Messages queued: " << stats.messages_queued << std::endl;
std::cout << "  Current IP: " << stats.current_local_ip << std::endl;
std::cout << "  Connected: " << (stats.is_connected ? "Yes" : "No") << std::endl;
```

---

## 9. 문제 해결

### Q: 재연결이 안 돼요!

**A:**
```bash
# 1. 네트워크 확인
ping ota-server.company.com

# 2. 포트 확인
telnet ota-server.company.com 8883

# 3. 인증서 확인
openssl s_client -connect ota-server.company.com:8883 \
  -CAfile ca.crt -cert vmg-client.crt -key vmg-client.key
```

---

### Q: 메시지가 손실돼요!

**A:**
- QoS 1 이상 사용 확인
- Clean Session = 0 확인
- 서버의 persistence 설정 확인

---

### Q: 재연결이 너무 느려요!

**A:**
```cpp
// 재연결 파라미터 조정
mqtt_client.setReconnectionParams(
    500,    // Initial: 0.5 second (더 빠름)
    30000,  // Max: 30 seconds
    2       // Multiplier
);
```

---

## 10. 결론

✅ **VMG IP 변경 문제 해결!**

**핵심:**
1. 서버는 고정 도메인/IP
2. VMG는 클라이언트 (서버가 VMG IP 몰라도 됨)
3. MQTT Clean Session = False (세션 지속)
4. QoS 1 (메시지 보장)
5. 자동 재연결 (Exponential backoff)
6. 메시지 큐잉 (오프라인 버퍼링)

**결과:**
- 차량이 전국 어디를 이동해도 연결 유지
- IP 변경 시 자동 재연결
- OTA 업데이트 중단 없음
- 메시지 손실 없음

**Production Ready!** 🚗✅

