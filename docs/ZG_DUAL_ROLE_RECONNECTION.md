# Zonal Gateway Dual Role Reconnection Strategy

## ZG의 이중 역할

### 질문: "ZG는 mbedTLS 클라이언트기도 하고 서버이기도 하잖아 그 점에 대해서도 대비가 되어있지?"

### 답변: **부분적으로만 구현됨!**

```
Zonal Gateway (ZG)
    │
    ├─ [DoIP Client] → VMG 연결
    │   └─ ✅ 재연결 로직 구현됨 (doip_client_reconnect.c)
    │
    └─ [DoIP Server] → ECU 연결 수신
        └─ ❌ 재연결 로직 없음! (서버는 Accept만 함)
```

---

## 1. ZG의 두 가지 역할

### A. Client 역할 (VMG 방향)

```
ZG (Client)                VMG (Server)
    │                          │
    ├─ TCP Connect ──────────> │
    ├─ TLS Handshake <───────> │
    ├─ DoIP Routing Activation │
    └─ 메시지 송수신 <────────> │
```

**문제:**
- VMG가 재시작되면?
- 네트워크 일시 장애?

**해결:** ✅ `doip_client_reconnect.c` 구현됨

---

### B. Server 역할 (ECU 방향)

```
ECU (Client)               ZG (Server)
    │                          │
    │ <──────────── TCP Accept │
    │ <───────> TLS Handshake  │
    │         DoIP Routing     │
    │ <────────> 메시지 송수신 │
```

**문제:**
- ECU 연결이 끊어지면?
- ECU가 재시작되면?
- 여러 ECU 중 일부만 끊어지면?

**해결:** ❌ **구현 안 됨!**

---

## 2. 현재 구현 상태

### ✅ Client 측 (ZG → VMG)

```c
// zonal_gateway/tc375/src/zonal_gateway_main.c

void doip_client_task(void* params) {
    DoIPClientReconnect_t client;
    
    // VMG 연결 (재연결 로직 포함)
    doip_client_reconnect_init(&client, "192.168.1.1", 13400, ...);
    doip_client_reconnect_task(&client);  // ← 자동 재연결!
}
```

**상태:** ✅ 재연결 구현됨

---

### ❌ Server 측 (ZG ← ECU)

```c
// zonal_gateway/tc375/src/zonal_gateway_main.c

void doip_server_task(void* params) {
    mbedtls_doip_server server;
    
    // 초기화
    mbedtls_doip_server_init(&server);
    mbedtls_doip_server_set_cert(&server, ...);
    mbedtls_doip_server_listen(&server, 13400);
    
    // Accept 루프
    while (1) {
        mbedtls_doip_client* ecu_client = mbedtls_doip_server_accept(&server);
        
        if (ecu_client) {
            // ECU 연결됨
            handle_ecu_connection(ecu_client);  // ← 연결 끊김 처리?
        }
    }
}
```

**문제점:**
1. **ECU 연결 끊김 감지 안 함**
2. **재연결 시도 없음** (ECU가 재시도해야 함)
3. **여러 ECU 관리 로직 없음**

**상태:** ❌ 재연결 로직 없음

---

## 3. 필요한 개선 사항

### A. Server 측 연결 관리

```c
// zonal_gateway/tc375/include/doip_server_connection_manager.h

typedef struct {
    mbedtls_doip_client* client;
    char ecu_id[32];
    uint16_t logical_address;
    bool is_connected;
    uint32_t last_activity_time_ms;
    uint32_t total_messages_sent;
    uint32_t total_messages_recv;
} ECUConnection_t;

typedef struct {
    mbedtls_doip_server server;
    ECUConnection_t ecu_connections[MAX_ECU_COUNT];
    uint32_t active_ecu_count;
} DoIPServerManager_t;
```

---

### B. 연결 상태 모니터링

```c
// zonal_gateway/tc375/src/doip_server_connection_manager.c

void doip_server_monitor_connections(DoIPServerManager_t* mgr) {
    uint32_t current_time = GET_TIME_MS();
    
    for (int i = 0; i < MAX_ECU_COUNT; i++) {
        ECUConnection_t* ecu = &mgr->ecu_connections[i];
        
        if (!ecu->is_connected) {
            continue;
        }
        
        // Check timeout (no activity for 60 seconds)
        if (current_time - ecu->last_activity_time_ms > 60000) {
            printf("[ZG] ECU %s timeout, closing connection\n", ecu->ecu_id);
            
            // Close connection
            mbedtls_doip_client_close(ecu->client);
            ecu->is_connected = false;
            mgr->active_ecu_count--;
            
            // ECU will reconnect automatically (via doip_client_reconnect)
        }
    }
}
```

---

### C. 다중 ECU 처리

```c
void doip_server_accept_loop(DoIPServerManager_t* mgr) {
    printf("[ZG] DoIP Server listening on port 13400\n");
    
    while (1) {
        // Accept with timeout (non-blocking)
        mbedtls_doip_client* ecu_client = mbedtls_doip_server_accept_timeout(
            &mgr->server, 
            1000  // 1 second timeout
        );
        
        if (ecu_client) {
            // New ECU connected
            char peer_addr[64];
            mbedtls_doip_client_get_peer_addr(ecu_client, peer_addr, sizeof(peer_addr));
            
            printf("[ZG] ECU connected from %s\n", peer_addr);
            
            // Add to connection list
            int slot = find_free_slot(mgr);
            if (slot >= 0) {
                ECUConnection_t* ecu = &mgr->ecu_connections[slot];
                ecu->client = ecu_client;
                ecu->is_connected = true;
                ecu->last_activity_time_ms = GET_TIME_MS();
                mgr->active_ecu_count++;
                
                // Spawn task to handle this ECU
                xTaskCreate(handle_ecu_task, "ECU", 4096, ecu, 5, NULL);
            } else {
                printf("[ZG] No free slot for ECU, rejecting connection\n");
                mbedtls_doip_client_close(ecu_client);
            }
        }
        
        // Monitor existing connections
        doip_server_monitor_connections(mgr);
        
        // Small delay
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

### D. ECU별 메시지 처리

```c
void handle_ecu_task(void* params) {
    ECUConnection_t* ecu = (ECUConnection_t*)params;
    
    printf("[ZG] Handling ECU %s\n", ecu->ecu_id);
    
    while (ecu->is_connected) {
        uint8_t buf[4096];
        int len = mbedtls_doip_client_recv(ecu->client, buf, sizeof(buf), 1000);
        
        if (len > 0) {
            // Message received
            ecu->last_activity_time_ms = GET_TIME_MS();
            ecu->total_messages_recv++;
            
            // Process message
            handle_ecu_message(ecu, buf, len);
            
        } else if (len < 0) {
            // Connection error
            printf("[ZG] ECU %s connection error, closing\n", ecu->ecu_id);
            mbedtls_doip_client_close(ecu->client);
            ecu->is_connected = false;
            break;
        }
        
        // Timeout (len == 0) is OK, just continue
    }
    
    printf("[ZG] ECU %s handler exited\n", ecu->ecu_id);
    
    // Task will be deleted automatically
    vTaskDelete(NULL);
}
```

---

## 4. 완전한 ZG 구조

```c
// zonal_gateway/tc375/src/zonal_gateway_main.c

int main(void) {
    // Hardware initialization
    init_hardware();
    
    // Create DoIP Server Manager
    static DoIPServerManager_t server_mgr;
    doip_server_manager_init(&server_mgr);
    
    // Create DoIP Client (to VMG) with reconnection
    static DoIPClientReconnect_t vmg_client;
    doip_client_reconnect_init(&vmg_client, "192.168.1.1", 13400, ...);
    
    // Task 1: DoIP Server (for ECUs)
    xTaskCreate(doip_server_task, "DoIP_Server", 4096, &server_mgr, 5, NULL);
    
    // Task 2: DoIP Client (to VMG) with auto-reconnect
    xTaskCreate(doip_client_reconnect_task, "DoIP_Client", 4096, &vmg_client, 5, NULL);
    
    // Task 3: Message Router (between VMG and ECUs)
    xTaskCreate(message_router_task, "Router", 4096, NULL, 5, NULL);
    
    // Task 4: Connection Monitor
    xTaskCreate(connection_monitor_task, "Monitor", 2048, &server_mgr, 3, NULL);
    
    // Start FreeRTOS scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while(1);
}
```

---

## 5. 연결 시나리오

### 시나리오 1: 정상 동작

```
Time  VMG            ZG (Client+Server)        ECU
──────────────────────────────────────────────────────
T0    [Listen]       [부팅 완료]               [부팅 중]
                     [Client: VMG 연결 시도]
                     [Server: Listen]
                     
T1    [Accept]       [Client: 연결 성공]       [부팅 완료]
      <──────────────[TLS Handshake]           [ZG 연결 시도]
      [연결됨]       [Client: 연결됨]          
                     [Server: Accept] ─────────>
                     [Server: 연결 성공]       [연결 성공]
                     
T2    [메시지 처리]  [라우팅]                  [메시지 처리]
      <──────────────[VMG ↔ ZG ↔ ECU]────────>
```

---

### 시나리오 2: VMG 재시작

```
Time  VMG            ZG (Client+Server)        ECU
──────────────────────────────────────────────────────
T0    [연결됨]       [Client: 연결됨]          [연결됨]
                     [Server: ECU 연결 유지]
                     
T1    [재시작!]      [Client: 연결 끊김 감지]  [연결 유지]
      [부팅 중...]   [Client: 재연결 시작]
                     [Server: ECU 연결 유지] ← 중요!
                     
T2    [부팅 완료]    [Client: 재연결 시도]     [연결 유지]
      [Listen]       
                     
T3    [Accept]       [Client: 재연결 성공!]    [연결 유지]
      <──────────────[TLS Handshake]
      [연결 복구]    [Client: 연결됨]
                     [Server: ECU 연결 유지]
```

**결과:** ✅ VMG 재시작해도 ECU 연결은 유지됨!

---

### 시나리오 3: ECU 재시작

```
Time  VMG            ZG (Client+Server)        ECU
──────────────────────────────────────────────────────
T0    [연결됨]       [Client: VMG 연결 유지]   [연결됨]
                     [Server: ECU 연결됨]
                     
T1    [연결 유지]    [Client: VMG 연결 유지]   [재시작!]
                     [Server: ECU 연결 끊김]   [부팅 중...]
                     [Server: Timeout 감지]
                     [Server: 연결 정리]
                     
T2    [연결 유지]    [Client: VMG 연결 유지]   [부팅 완료]
                     [Server: Listen]          [ZG 연결 시도]
                                               [재연결 로직 동작]
                     
T3    [연결 유지]    [Server: Accept]          [재연결 성공!]
                     [Server: 새 연결 수립] <──[TLS Handshake]
                     [Client: VMG 연결 유지]   [연결됨]
```

**결과:** ✅ ECU 재시작해도 자동 재연결!

---

### 시나리오 4: ZG 재시작 (최악의 경우)

```
Time  VMG            ZG (Client+Server)        ECU
──────────────────────────────────────────────────────
T0    [연결됨]       [Client: 연결됨]          [연결됨]
                     [Server: 연결됨]
                     
T1    [연결 끊김]    [재시작!]                 [연결 끊김]
      [Listen]       [부팅 중...]              [재연결 시작]
                     
T2    [Listen]       [부팅 완료]               [재연결 시도]
                     [Client: VMG 연결 시도]   [재연결 시도]
                     [Server: Listen]
                     
T3    [Accept]       [Client: VMG 연결 성공]   [재연결 시도]
      <──────────────[TLS Handshake]
      [연결 복구]    [Client: 연결됨]
                     [Server: Accept] ─────────>
                     [Server: ECU 연결 성공]   [재연결 성공!]
                     
T4    [연결됨]       [모두 복구!]              [연결됨]
```

**결과:** ✅ ZG 재시작해도 양방향 모두 자동 복구!

---

## 6. 구현 체크리스트

### ✅ Client 측 (ZG → VMG)

- [x] 재연결 로직 (`doip_client_reconnect.c`)
- [x] Exponential backoff
- [x] Keepalive
- [x] 무한 재시도

### ❌ Server 측 (ZG ← ECU) - 필요!

- [ ] 다중 ECU 연결 관리
- [ ] 연결 상태 모니터링
- [ ] Timeout 감지 및 정리
- [ ] 연결 끊김 로그
- [ ] 통계 정보 (연결 수, 메시지 수)

---

## 7. 필요한 새 파일

### A. Server Connection Manager

```
zonal_gateway/tc375/include/doip_server_connection_manager.h
zonal_gateway/tc375/src/doip_server_connection_manager.c
```

**기능:**
- 여러 ECU 연결 추적
- 연결 상태 모니터링
- Timeout 감지
- 통계 수집

---

### B. Message Router

```
zonal_gateway/tc375/include/message_router.h
zonal_gateway/tc375/src/message_router.c
```

**기능:**
- VMG ↔ ECU 메시지 라우팅
- 목적지 ECU 찾기
- 브로드캐스트 지원
- 메시지 큐 관리

---

## 8. 요약

### 질문: "ZG는 mbedTLS 클라이언트기도 하고 서버이기도 하잖아 그 점에 대해서도 대비가 되어있지?"

### 답변: **부분적으로만!**

| 역할 | 방향 | 재연결 로직 | 상태 |
|------|------|------------|------|
| **Client** | ZG → VMG | ✅ 구현됨 | `doip_client_reconnect.c` |
| **Server** | ZG ← ECU | ❌ 없음 | **구현 필요!** |

### 필요한 개선:

1. **Server 측 연결 관리**
   - 다중 ECU 추적
   - 연결 상태 모니터링
   - Timeout 감지

2. **ECU 재연결 처리**
   - ECU가 재시도 (Client 측 재연결)
   - ZG는 Accept만 (Server는 재연결 안 함)
   - 연결 끊김 감지 및 정리

3. **메시지 라우팅**
   - VMG ↔ ZG ↔ ECU
   - 목적지 ECU 찾기
   - 브로드캐스트

**우선순위: 🔴 HIGH**

Server 측 연결 관리가 없으면 ECU 재시작 시 문제 발생!

