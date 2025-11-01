# VMG Dynamic IP Management Architecture

## 문제 정의

**시나리오:**
```
차량이 도시 A → 도시 B로 이동
  ↓
기지국 변경: Tower-A → Tower-B → Tower-C
  ↓
IP 변경: 10.1.1.100 → 10.2.5.200 → 10.3.9.150
  ↓
기존 TCP/MQTT 연결 끊김
  ↓
❌ 서버는 VMG의 새 IP를 모름
❌ 진행 중이던 OTA 업데이트 중단
❌ 진단 요청 전송 불가
```

---

## 해결 방안: 3가지 전략

---

## 전략 1: MQTT Keep-Alive + Reconnection (권장 ✅)

### A. 아키텍처

```
┌──────────────────────────────────────────────────────────┐
│                     OTA Server                           │
│  - MQTT Broker (고정 IP/도메인)                           │
│  - ota-server.company.com:8883                          │
└──────────────────┬───────────────────────────────────────┘
                   │
                   │ MQTT over PQC-TLS
                   │ (Client 주도 연결)
                   │
┌──────────────────▼───────────────────────────────────────┐
│                      VMG                                 │
│  - MQTT Client (항상 재연결 시도)                          │
│  - Client ID: "vehicle-{VIN}"                           │
│  - IP: 가변 (10.1.1.100 → 10.2.5.200 → ...)             │
└──────────────────────────────────────────────────────────┘
```

### B. 핵심 원리

**1. 서버는 고정 도메인/IP 사용**
```
서버 주소: ota-server.company.com
  ↓ DNS 해석
고정 IP: 52.78.123.45 (AWS ELB/ALB 등)
```

**2. VMG는 항상 클라이언트 역할**
```
VMG의 IP가 바뀌어도 문제없음!
  ↓
VMG가 서버에 연결하는 구조이므로
서버는 VMG의 IP를 알 필요 없음
  ↓
MQTT Broker가 Client ID로 식별
```

**3. 연결 끊김 시 자동 재연결**
```
[IP 변경 감지]
  ↓
[기존 TCP 연결 끊김]
  ↓
[VMG: 새 IP로 재연결 시도]
  ↓
[MQTT Clean Session = False]
  ↓
[서버: 기존 세션 복원]
  ↓
[미전송 메시지 자동 전달]
```

---

### C. 구현: VMG MQTT Client

```cpp
// vehicle_gateway/include/mqtt_client_persistent.hpp

#ifndef MQTT_CLIENT_PERSISTENT_HPP
#define MQTT_CLIENT_PERSISTENT_HPP

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

namespace vmg {

/**
 * @brief Persistent MQTT Client with Auto-Reconnection
 * 
 * Features:
 * - Automatic reconnection on network changes
 * - Exponential backoff for retries
 * - Session persistence (Clean Session = False)
 * - QoS 1 for reliable message delivery
 */
class MQTTClientPersistent {
public:
    MQTTClientPersistent(const std::string& server_url,
                         const std::string& client_id);
    ~MQTTClientPersistent();
    
    /**
     * @brief Initialize client with credentials
     * 
     * @param cert_path Client certificate
     * @param key_path Client private key
     * @param ca_path CA certificate
     * @return true on success
     */
    bool initialize(const std::string& cert_path,
                   const std::string& key_path,
                   const std::string& ca_path);
    
    /**
     * @brief Start connection and maintain it
     */
    void start();
    
    /**
     * @brief Stop connection
     */
    void stop();
    
    /**
     * @brief Publish message
     * 
     * @param topic MQTT topic
     * @param payload Message payload
     * @param qos Quality of Service (0, 1, 2)
     * @return true if queued successfully
     */
    bool publish(const std::string& topic,
                const std::string& payload,
                int qos = 1);
    
    /**
     * @brief Subscribe to topic
     * 
     * @param topic MQTT topic
     * @param callback Message callback
     * @param qos Quality of Service
     * @return true on success
     */
    bool subscribe(const std::string& topic,
                  std::function<void(const std::string&)> callback,
                  int qos = 1);
    
    /**
     * @brief Check if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Get connection statistics
     */
    struct ConnectionStats {
        uint64_t total_connections;
        uint64_t failed_connections;
        uint64_t reconnections;
        uint64_t messages_sent;
        uint64_t messages_received;
        std::chrono::steady_clock::time_point last_connection_time;
        std::string current_local_ip;
    };
    
    ConnectionStats getStats() const;

private:
    /**
     * @brief Connection loop with auto-reconnection
     */
    void connectionLoop();
    
    /**
     * @brief Attempt to connect
     * 
     * @return true if connected
     */
    bool attemptConnection();
    
    /**
     * @brief Handle connection lost
     */
    void onConnectionLost();
    
    /**
     * @brief Detect IP change
     * 
     * @return true if IP changed
     */
    bool detectIPChange();
    
    /**
     * @brief Get current local IP
     */
    std::string getCurrentLocalIP();

private:
    std::string server_url_;
    std::string client_id_;
    
    std::string cert_path_;
    std::string key_path_;
    std::string ca_path_;
    
    std::atomic<bool> is_running_;
    std::atomic<bool> is_connected_;
    std::thread connection_thread_;
    
    void* mqtt_client_;  // Paho MQTT client
    
    // Reconnection settings
    uint32_t reconnect_delay_ms_ = 1000;      // Initial: 1 second
    uint32_t max_reconnect_delay_ms_ = 60000; // Max: 60 seconds
    uint32_t reconnect_multiplier_ = 2;       // Exponential backoff
    
    // IP tracking
    std::string last_known_ip_;
    
    // Statistics
    mutable ConnectionStats stats_;
};

} // namespace vmg

#endif // MQTT_CLIENT_PERSISTENT_HPP
```

---

### D. 구현: 연결 로직

```cpp
// vehicle_gateway/src/mqtt_client_persistent.cpp

void MQTTClientPersistent::connectionLoop() {
    uint32_t current_delay = reconnect_delay_ms_;
    
    while (is_running_) {
        if (!is_connected_) {
            std::cout << "[MQTT] Attempting connection to " << server_url_ << std::endl;
            
            // Check if IP changed
            if (detectIPChange()) {
                std::cout << "[MQTT] Local IP changed: " 
                          << last_known_ip_ << " → " 
                          << getCurrentLocalIP() << std::endl;
                stats_.current_local_ip = getCurrentLocalIP();
            }
            
            if (attemptConnection()) {
                std::cout << "[MQTT] Connected successfully!" << std::endl;
                is_connected_ = true;
                current_delay = reconnect_delay_ms_; // Reset delay
                stats_.total_connections++;
                stats_.last_connection_time = std::chrono::steady_clock::now();
            } else {
                std::cout << "[MQTT] Connection failed. Retrying in " 
                          << current_delay << "ms" << std::endl;
                stats_.failed_connections++;
                
                // Exponential backoff
                std::this_thread::sleep_for(std::chrono::milliseconds(current_delay));
                current_delay *= reconnect_multiplier_;
                if (current_delay > max_reconnect_delay_ms_) {
                    current_delay = max_reconnect_delay_ms_;
                }
            }
        } else {
            // Connected - check for IP changes periodically
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            if (detectIPChange()) {
                std::cout << "[MQTT] IP changed while connected. Reconnecting..." << std::endl;
                onConnectionLost();
            }
        }
    }
}

bool MQTTClientPersistent::attemptConnection() {
    // Paho MQTT connection options
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    
    // Critical: Clean Session = False for session persistence
    conn_opts.cleansession = 0;  // Preserve session!
    
    // TLS configuration
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    ssl_opts.trustStore = ca_path_.c_str();
    ssl_opts.keyStore = cert_path_.c_str();
    ssl_opts.privateKey = key_path_.c_str();
    ssl_opts.enableServerCertAuth = 1;
    
    conn_opts.ssl = &ssl_opts;
    
    // Keepalive and timeouts
    conn_opts.keepAliveInterval = 30;  // 30 seconds
    conn_opts.connectTimeout = 10;      // 10 seconds
    conn_opts.automaticReconnect = 0;   // We handle reconnection manually
    
    // Attempt connection
    int rc = MQTTClient_connect(mqtt_client_, &conn_opts);
    
    return (rc == MQTTCLIENT_SUCCESS);
}

bool MQTTClientPersistent::detectIPChange() {
    std::string current_ip = getCurrentLocalIP();
    
    if (last_known_ip_.empty()) {
        last_known_ip_ = current_ip;
        return false;
    }
    
    if (current_ip != last_known_ip_) {
        last_known_ip_ = current_ip;
        return true;
    }
    
    return false;
}

std::string MQTTClientPersistent::getCurrentLocalIP() {
    // TODO: Implement using system APIs
    // Linux: getifaddrs()
    // Windows: GetAdaptersAddresses()
    
    // For demonstration:
    return "10.x.x.x";
}

void MQTTClientPersistent::onConnectionLost() {
    std::cout << "[MQTT] Connection lost. Reconnecting..." << std::endl;
    is_connected_ = false;
    stats_.reconnections++;
    
    // Disconnect cleanly
    MQTTClient_disconnect(mqtt_client_, 0);
}
```

---

### E. MQTT 설정: Clean Session = False

```cpp
// Critical Configuration

MQTTClient_connectOptions conn_opts;
conn_opts.cleansession = 0;  // ← 중요!

/**
 * Clean Session = False의 의미:
 * 
 * 1. 서버는 Client ID별로 세션 저장
 * 2. 연결이 끊겨도 세션 유지
 * 3. QoS 1/2 메시지 큐에 보관
 * 4. 재연결 시 자동으로 미전송 메시지 전달
 * 5. 구독 정보도 유지
 */
```

---

### F. 메시지 흐름 예시

```
시나리오: OTA 업데이트 중 IP 변경

[0초] VMG: 10.1.1.100으로 연결
  ↓
[5초] Server → VMG: "firmware_chunk_1"
  ↓ QoS 1, ACK 전송
[10초] Server → VMG: "firmware_chunk_2"
  ↓ QoS 1, ACK 전송
[15초] ⚠️ 기지국 변경! IP → 10.2.5.200
  ↓ TCP 연결 끊김
[16초] Server → VMG: "firmware_chunk_3" (전송 실패, 큐에 보관)
  ↓
[17초] VMG: 새 IP (10.2.5.200)로 재연결 시도
  ↓
[18초] VMG → Server: CONNECT (Clean Session = False, Client ID = "vehicle-VIN123")
  ↓
[19초] Server: 세션 복원, 큐에 있던 메시지 확인
  ↓
[20초] Server → VMG: "firmware_chunk_3" (자동 재전송!)
  ↓ QoS 1, ACK 전송
[25초] OTA 업데이트 정상 진행...
```

---

## 전략 2: Dynamic DNS + Heartbeat

### A. 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                   OTA Server                            │
│  - MQTT Broker: ota-server.com:8883                    │
│  - HTTP API: https://ota-server.com/api/v1             │
│  - VMG Registry Database                               │
│    {                                                    │
│      "VIN123": {                                        │
│        "current_ip": "10.2.5.200",                     │
│        "last_heartbeat": "2025-11-01T10:30:00Z",       │
│        "status": "online"                              │
│      }                                                  │
│    }                                                    │
└─────────────────────────────────────────────────────────┘
                    ▲
                    │ Heartbeat (매 30초)
                    │ POST /api/v1/vehicles/{vin}/heartbeat
                    │ Body: { "ip": "10.2.5.200", "status": "online" }
                    │
┌───────────────────┴─────────────────────────────────────┐
│                      VMG                                │
│  - Heartbeat Task (FreeRTOS)                           │
│  - IP Change Detector                                   │
│  - Current IP: 10.2.5.200 (가변)                        │
└─────────────────────────────────────────────────────────┘
```

### B. Heartbeat 구현

```cpp
// vehicle_gateway/src/heartbeat_task.cpp

void HeartbeatTask::run() {
    while (is_running_) {
        // Get current IP
        std::string current_ip = NetworkUtils::getLocalIP();
        
        // Build heartbeat payload
        json payload = {
            {"vin", vehicle_vin_},
            {"ip", current_ip},
            {"status", "online"},
            {"timestamp", getCurrentTimestamp()},
            {"location", getGPSLocation()},  // Optional
            {"battery", getBatteryLevel()}   // Optional
        };
        
        // Send to server
        bool success = httpClient_.post(
            "https://ota-server.com/api/v1/vehicles/" + vehicle_vin_ + "/heartbeat",
            payload.dump()
        );
        
        if (success) {
            std::cout << "[Heartbeat] Sent: IP=" << current_ip << std::endl;
        } else {
            std::cerr << "[Heartbeat] Failed" << std::endl;
        }
        
        // Sleep for 30 seconds
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}
```

---

## 전략 3: Hybrid (MQTT + HTTP Fallback)

### A. 아키텍처

```
┌──────────────────────────────────────────────────────────┐
│                     OTA Server                           │
│  - Primary: MQTT (실시간 통신)                             │
│  - Fallback: HTTPS REST API (배치 처리)                   │
└──────────────────────────────────────────────────────────┘
                    ▲
                    │
        ┌───────────┴───────────┐
        │                       │
   MQTT (실시간)           HTTPS (폴링)
   Port 8883            Port 443
        │                       │
        │                       │
┌───────┴───────────────────────▼───────────────────────────┐
│                      VMG                                  │
│  - MQTT Client (primary)                                 │
│  - HTTPS Polling (fallback, 매 60초)                      │
│  - Strategy:                                             │
│    1. MQTT 연결 시도                                       │
│    2. 실패 시 HTTPS 폴링으로 전환                           │
│    3. MQTT 재연결 백그라운드에서 계속 시도                   │
└───────────────────────────────────────────────────────────┘
```

### B. 구현

```cpp
// vehicle_gateway/src/communication_manager.cpp

class CommunicationManager {
public:
    enum class Mode {
        MQTT_PRIMARY,
        HTTPS_FALLBACK
    };
    
    void run() {
        while (is_running_) {
            if (current_mode_ == Mode::MQTT_PRIMARY) {
                if (!mqtt_client_->isConnected()) {
                    std::cout << "[CommMgr] MQTT disconnected. Switching to HTTPS fallback" << std::endl;
                    current_mode_ = Mode::HTTPS_FALLBACK;
                }
            } else {
                // Try to restore MQTT connection
                if (mqtt_client_->attemptConnection()) {
                    std::cout << "[CommMgr] MQTT reconnected. Switching back to MQTT" << std::endl;
                    current_mode_ = Mode::MQTT_PRIMARY;
                } else {
                    // Use HTTPS polling
                    pollServerViaHTTPS();
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
private:
    void pollServerViaHTTPS() {
        // Check for pending messages
        auto response = https_client_->get(
            "https://ota-server.com/api/v1/vehicles/" + vin_ + "/pending-messages"
        );
        
        if (response.status == 200) {
            // Process messages
            processMessages(response.body);
        }
    }
};
```

---

## 권장 구성 (Production)

### 최종 아키텍처

```
┌──────────────────────────────────────────────────────────┐
│                Cloud Infrastructure                      │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  AWS/Azure Load Balancer (고정 IP)                  │ │
│  │  ota-server.company.com                            │ │
│  │  52.78.123.45                                       │ │
│  └───────────────┬────────────────────────────────────┘ │
│                  │                                       │
│  ┌───────────────▼────────────────────────────────────┐ │
│  │  MQTT Broker Cluster (HA)                          │ │
│  │  - EMQ X / Mosquitto Cluster                       │ │
│  │  - Clean Session = False                           │ │
│  │  - QoS 1 for reliability                           │ │
│  │  - Session persistence in Redis                    │ │
│  └────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
                   ▲
                   │ MQTT over PQC-TLS
                   │ Client ID: vehicle-{VIN}
                   │ Keepalive: 30s
                   │
┌──────────────────┴───────────────────────────────────────┐
│                Vehicle (VMG)                             │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  MQTT Client with Auto-Reconnection                │ │
│  │  - Exponential backoff: 1s → 2s → 4s → ... → 60s  │ │
│  │  - IP change detection                             │ │
│  │  - Message queue (local storage)                   │ │
│  │  - Current IP: 가변 (10.x.x.x)                      │ │
│  └────────────────────────────────────────────────────┘ │
│                                                          │
│  ┌────────────────────────────────────────────────────┐ │
│  │  Fallback: HTTPS Polling (매 60초)                  │ │
│  │  - MQTT 장시간 실패 시 활성화                         │ │
│  └────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

---

## 핵심 설정 요약

### 1. MQTT 설정
```cpp
// Clean Session = False (세션 지속)
conn_opts.cleansession = 0;

// QoS 1 (최소 1회 전달 보장)
MQTTClient_publish(client, topic, payload, qos=1);

// Keepalive (30초마다 PING)
conn_opts.keepAliveInterval = 30;
```

### 2. 재연결 설정
```cpp
// Exponential backoff
initial_delay = 1s
max_delay = 60s
multiplier = 2

// 무한 재시도
while (true) {
    if (!connected) {
        attemptConnection();
        sleep(current_delay);
        current_delay *= multiplier;
    }
}
```

### 3. 서버 설정
```
고정 도메인: ota-server.company.com
고정 IP: 52.78.123.45 (Load Balancer)
MQTT Port: 8883 (TLS)
HTTPS Port: 443
```

---

## 테스트 시나리오

```bash
# 1. 정상 연결 테스트
mosquitto_pub -h ota-server.com -p 8883 \
  --cafile ca.crt --cert vmg.crt --key vmg.key \
  -i "vehicle-VIN123" -t "vehicle/VIN123/status" \
  -m '{"status":"online"}' -q 1

# 2. IP 변경 시뮬레이션
# (VMG에서 네트워크 인터페이스 재시작)
sudo ifconfig eth0 down && sudo ifconfig eth0 up

# 3. 재연결 확인
# (로그에서 "Reconnected successfully" 확인)
```

---

## 결론

**권장 방식: 전략 1 (MQTT Keep-Alive + Reconnection)**

✅ **장점:**
- VMG IP 변경에 무관
- 자동 재연결
- 세션 지속성 (Clean Session = False)
- QoS 1로 메시지 손실 방지
- 서버는 고정 도메인/IP 사용

✅ **구현 용이:**
- MQTT 표준 기능 활용
- 추가 인프라 불필요
- 테스트 용이

✅ **Production Ready:**
- 대부분의 IoT 플랫폼이 사용하는 방식
- 검증된 안정성

**핵심:** 서버가 고정, VMG가 클라이언트 → VMG의 IP 변경은 문제없음!

