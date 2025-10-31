# Reconnection Strategy for In-Vehicle Network

## 문제 상황

### 질문: "부팅 시점의 싱크가 안 맞으면? 그에 따른 대비책도 구현되어 있음?"

### 현재 상태: ❌ **재연결 로직 없음!**

```c
// 현재 코드 (doip_client.c)
int doip_client_connect(DoIPClient_t* client) {
    // TCP 연결 시도
    if (doip_socket_tcp_connect(...) != 0) {
        return -1;  // ← 실패하면 그냥 종료!
    }
    return 0;
}
```

**문제점:**
- 서버가 아직 준비 안 됨 → 연결 실패 → 재시도 없음
- 네트워크 일시 장애 → 연결 끊김 → 재연결 안 함
- 서버 재시작 → 클라이언트는 모름 → 계속 대기

---

## 시나리오별 문제

### 시나리오 1: ECU가 ZG보다 먼저 부팅

```
Time  ECU                      ZG
────────────────────────────────────────
T0    [부팅 완료]              [전원 OFF]
      [DoIP Client 시작]
      [ZG 연결 시도]
      ❌ 연결 실패!
      [종료...]               
                               
T1                             [부팅 시작]
                               [DoIP Server 시작]
                               [Listen...]
                               
T2                             [대기 중...]
      ❌ ECU는 이미 종료됨!
```

**결과:** ECU와 ZG 연결 안 됨!

---

### 시나리오 2: ZG가 VMG보다 먼저 부팅

```
Time  ZG                       VMG
────────────────────────────────────────
T0    [부팅 완료]              [부팅 중...]
      [DoIP Client 시작]
      [VMG 연결 시도]
      ❌ 연결 실패!
      [종료...]               
                               
T1                             [부팅 완료]
                               [DoIP Server 시작]
                               [Listen...]
                               
T2                             [대기 중...]
      ❌ ZG는 이미 종료됨!
```

**결과:** ZG와 VMG 연결 안 됨!

---

### 시나리오 3: 네트워크 일시 장애

```
Time  ECU                      ZG
────────────────────────────────────────
T0    [연결됨]                [연결됨]
      [메시지 송수신]          [메시지 송수신]
                               
T1    [네트워크 장애 발생]
      ❌ 연결 끊김!           ❌ 연결 끊김!
                               
T2    [네트워크 복구]
      ❌ 재연결 안 함!        [대기 중...]
```

**결과:** 네트워크 복구 후에도 연결 안 됨!

---

## 해결 방안

### 1. 재연결 로직 (Reconnection Logic)

#### A. 클라이언트 측 (ZG, ECU)

```c
// zonal_gateway/tc375/src/doip_client_reconnect.c

#define RECONNECT_INTERVAL_MS    5000   // 5초마다 재시도
#define MAX_RECONNECT_ATTEMPTS   0      // 무한 재시도 (0 = infinite)
#define INITIAL_BACKOFF_MS       1000   // 초기 대기 시간
#define MAX_BACKOFF_MS           30000  // 최대 대기 시간 (30초)

typedef struct {
    mbedtls_doip_client client;
    
    // Reconnection state
    bool is_connected;
    uint32_t reconnect_count;
    uint32_t backoff_ms;
    uint32_t last_attempt_time;
    
    // Server info
    char server_host[64];
    uint16_t server_port;
    
    // Certificates
    char cert_file[128];
    char key_file[128];
    char ca_file[128];
    
} DoIPClientReconnect_t;

/**
 * @brief Initialize reconnecting DoIP client
 */
int doip_client_reconnect_init(
    DoIPClientReconnect_t* client,
    const char* server_host,
    uint16_t server_port,
    const char* cert_file,
    const char* key_file,
    const char* ca_file
) {
    if (!client || !server_host) return -1;
    
    memset(client, 0, sizeof(DoIPClientReconnect_t));
    
    strncpy(client->server_host, server_host, sizeof(client->server_host) - 1);
    client->server_port = server_port;
    strncpy(client->cert_file, cert_file, sizeof(client->cert_file) - 1);
    strncpy(client->key_file, key_file, sizeof(client->key_file) - 1);
    strncpy(client->ca_file, ca_file, sizeof(client->ca_file) - 1);
    
    client->backoff_ms = INITIAL_BACKOFF_MS;
    client->is_connected = false;
    
    return 0;
}

/**
 * @brief Attempt to connect (with exponential backoff)
 */
int doip_client_reconnect_connect(DoIPClientReconnect_t* client) {
    if (!client) return -1;
    
    // Already connected
    if (client->is_connected) {
        return 0;
    }
    
    // Check if enough time has passed since last attempt
    uint32_t current_time = get_system_time_ms();
    if (current_time - client->last_attempt_time < client->backoff_ms) {
        return -1;  // Too soon to retry
    }
    
    printf("[DoIP] Attempting to connect to %s:%u (attempt %u)...\n",
           client->server_host, client->server_port, client->reconnect_count + 1);
    
    client->last_attempt_time = current_time;
    
    // Try to connect
    int ret = mbedtls_doip_client_connect(
        &client->client,
        client->server_host,
        client->server_port,
        client->cert_file,
        client->key_file,
        client->ca_file
    );
    
    if (ret == 0) {
        // Connection successful!
        printf("[DoIP] Connected successfully!\n");
        client->is_connected = true;
        client->reconnect_count = 0;
        client->backoff_ms = INITIAL_BACKOFF_MS;
        return 0;
    }
    
    // Connection failed
    printf("[DoIP] Connection failed, will retry in %u ms\n", client->backoff_ms);
    
    client->reconnect_count++;
    
    // Exponential backoff (double the wait time, up to max)
    client->backoff_ms *= 2;
    if (client->backoff_ms > MAX_BACKOFF_MS) {
        client->backoff_ms = MAX_BACKOFF_MS;
    }
    
    return -1;
}

/**
 * @brief Check connection health and reconnect if needed
 */
int doip_client_reconnect_check(DoIPClientReconnect_t* client) {
    if (!client) return -1;
    
    // If not connected, try to connect
    if (!client->is_connected) {
        return doip_client_reconnect_connect(client);
    }
    
    // Check if connection is still alive (send keepalive)
    int ret = mbedtls_doip_client_send_alive_check(&client->client);
    if (ret != 0) {
        printf("[DoIP] Connection lost, will reconnect...\n");
        
        // Close old connection
        mbedtls_doip_client_close(&client->client);
        client->is_connected = false;
        client->backoff_ms = INITIAL_BACKOFF_MS;
        
        // Try immediate reconnect
        return doip_client_reconnect_connect(client);
    }
    
    return 0;
}

/**
 * @brief FreeRTOS task for DoIP client with auto-reconnect
 */
void doip_client_reconnect_task(void* params) {
    DoIPClientReconnect_t* client = (DoIPClientReconnect_t*)params;
    
    printf("[DoIP] Client task started\n");
    
    while (1) {
        // Try to connect/reconnect
        doip_client_reconnect_check(client);
        
        // If connected, process messages
        if (client->is_connected) {
            uint8_t buf[4096];
            int len = mbedtls_doip_client_recv(&client->client, buf, sizeof(buf), 100);
            
            if (len > 0) {
                // Process received message
                handle_doip_message(buf, len);
            } else if (len < 0) {
                // Connection error
                printf("[DoIP] Receive error, connection lost\n");
                mbedtls_doip_client_close(&client->client);
                client->is_connected = false;
            }
        }
        
        // Small delay
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

### 2. Exponential Backoff 전략

```
연결 시도 1: 실패 → 1초 대기
연결 시도 2: 실패 → 2초 대기
연결 시도 3: 실패 → 4초 대기
연결 시도 4: 실패 → 8초 대기
연결 시도 5: 실패 → 16초 대기
연결 시도 6: 실패 → 30초 대기 (최대)
연결 시도 7: 실패 → 30초 대기
...
연결 시도 N: 성공! → 연결 유지
```

**장점:**
- 초기에는 빠르게 재시도 (서버가 곧 준비될 수 있음)
- 시간이 지날수록 재시도 간격 증가 (네트워크 부하 감소)
- 최대 대기 시간 제한 (무한정 대기 방지)

---

### 3. 연결 상태 모니터링

#### Keepalive (Alive Check)

```c
// 주기적으로 Alive Check 전송
void doip_keepalive_task(void* params) {
    DoIPClientReconnect_t* client = (DoIPClientReconnect_t*)params;
    
    while (1) {
        if (client->is_connected) {
            // Send Alive Check Request (DoIP 0x0007)
            int ret = mbedtls_doip_client_send_alive_check(&client->client);
            
            if (ret != 0) {
                printf("[DoIP] Keepalive failed, connection lost\n");
                client->is_connected = false;
            }
        }
        
        // Keepalive every 30 seconds
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
```

---

### 4. 서버 측 대응 (VMG, ZG)

#### Accept 루프 개선

```c
// vehicle_gateway/example_vmg_doip_server_mbedtls.cpp

void doip_server_accept_loop(mbedtls_doip_server* server) {
    printf("[VMG] DoIP Server listening on port 13400\n");
    printf("[VMG] Waiting for client connections...\n");
    
    while (running) {
        // Accept with timeout (non-blocking)
        mbedtls_doip_client* client = mbedtls_doip_server_accept_timeout(
            server, 
            1000  // 1 second timeout
        );
        
        if (client) {
            printf("[VMG] Client connected from %s\n", 
                   mbedtls_doip_client_get_peer_addr(client));
            
            // Spawn thread to handle client
            std::thread client_thread(handle_client, client);
            client_thread.detach();
        }
        
        // Check for shutdown signal
        if (shutdown_requested) {
            break;
        }
    }
    
    printf("[VMG] DoIP Server stopped\n");
}
```

---

## 개선된 부팅 시퀀스

### 시나리오 1: ECU가 ZG보다 먼저 부팅 (개선)

```
Time  ECU                      ZG
────────────────────────────────────────
T0    [부팅 완료]              [전원 OFF]
      [DoIP Client 시작]
      [ZG 연결 시도]
      ❌ 연결 실패
      [1초 대기...]
                               
T1    [ZG 연결 재시도]         [부팅 시작]
      ❌ 연결 실패
      [2초 대기...]
                               
T2                             [부팅 완료]
                               [DoIP Server 시작]
                               [Listen...]
                               
T3    [ZG 연결 재시도]
      ✅ 연결 성공!
      [TLS Handshake]          [Accept]
      [DoIP Routing Activation]
      [연결 유지]              [연결 유지]
```

**결과:** ✅ 부팅 시점 차이에도 자동 연결!

---

### 시나리오 2: 네트워크 일시 장애 (개선)

```
Time  ECU                      ZG
────────────────────────────────────────
T0    [연결됨]                [연결됨]
      [메시지 송수신]          [메시지 송수신]
                               
T1    [네트워크 장애 발생]
      ❌ Keepalive 실패!      ❌ 연결 끊김!
      [재연결 시작]
      [1초 대기...]
                               
T2    [네트워크 복구]
      [ZG 연결 재시도]
      ✅ 연결 성공!
      [TLS Handshake]          [Accept]
      [DoIP Routing Activation]
      [연결 유지]              [연결 유지]
```

**결과:** ✅ 네트워크 복구 후 자동 재연결!

---

## 구현 체크리스트

### ✅ 필수 구현 사항

- [ ] **Exponential Backoff** 재연결 로직
- [ ] **무한 재시도** (차량은 계속 연결 시도해야 함)
- [ ] **Keepalive** (연결 상태 모니터링)
- [ ] **연결 끊김 감지** (Timeout, Error 처리)
- [ ] **재연결 시 DoIP Routing Activation** 재수행
- [ ] **로그 출력** (디버깅용)

### ⚠️ 주의 사항

1. **무한 루프 방지**
   - 최대 Backoff 시간 설정 (30초)
   - CPU 사용률 최소화 (vTaskDelay 사용)

2. **메모리 누수 방지**
   - 재연결 시 이전 소켓 정리
   - mbedTLS 컨텍스트 재사용 또는 정리

3. **동기화**
   - 여러 Task에서 접근 시 Mutex 사용
   - 연결 상태 플래그 atomic 처리

---

## 타임라인 비교

### ❌ 현재 (재연결 없음)

```
T0: ECU 부팅 → ZG 연결 시도 → 실패 → 종료
T1: ZG 부팅 → Listen
T2: ❌ ECU와 ZG 연결 안 됨!
```

### ✅ 개선 (재연결 있음)

```
T0: ECU 부팅 → ZG 연결 시도 → 실패 → 1초 대기
T1: ECU 재시도 → 실패 → 2초 대기
T2: ZG 부팅 → Listen
T3: ECU 재시도 → ✅ 성공!
```

---

## 코드 위치

### 구현 필요 파일

1. **`zonal_gateway/tc375/src/doip_client_reconnect.c`**
   - 재연결 로직 구현

2. **`zonal_gateway/tc375/include/doip_client_reconnect.h`**
   - 재연결 API 정의

3. **`end_node_ecu/tc375/src/doip_client_reconnect.c`**
   - ECU용 재연결 로직 (ZG와 동일)

4. **`vehicle_gateway/common/mbedtls_doip.c`**
   - Accept timeout 추가
   - Keepalive 지원

---

## 설정 예시

```c
// config.h
#define DOIP_RECONNECT_ENABLED       1
#define DOIP_RECONNECT_INTERVAL_MS   5000
#define DOIP_RECONNECT_MAX_ATTEMPTS  0      // 0 = infinite
#define DOIP_INITIAL_BACKOFF_MS      1000
#define DOIP_MAX_BACKOFF_MS          30000
#define DOIP_KEEPALIVE_INTERVAL_MS   30000
#define DOIP_KEEPALIVE_TIMEOUT_MS    5000
```

---

## 요약

### 질문: "부팅 시점의 싱크가 안 맞으면? 그에 따른 대비책도 구현되어 있음?"

### 답변: **❌ 현재 구현 안 됨!**

**필요한 개선 사항:**

1. ✅ **Exponential Backoff 재연결**
2. ✅ **무한 재시도** (차량은 계속 시도해야 함)
3. ✅ **Keepalive** (연결 상태 모니터링)
4. ✅ **연결 끊김 자동 감지 및 재연결**

**구현 우선순위: 🔴 HIGH (필수)**

차량 네트워크에서 재연결 로직이 없으면 **부팅 순서나 네트워크 장애 시 통신 불가**합니다!

