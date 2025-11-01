# mbedTLS Handshake Timing in In-Vehicle Network

## 핵심 질문

**"ZG와 VMG, 그리고 ZG와 ECU의 mbedTLS 핸드쉐이크는 FreeRTOS 이후 DoIP Server/Client 시작하면서 하는거네?"**

## 답변: 부분적으로 맞습니다!

정확히는:
1. **DoIP Server는 시작 시 Listen만 함** (Handshake 안 함)
2. **DoIP Client가 연결할 때 Handshake 발생**
3. **Handshake는 연결 시점에 동적으로 발생**

---

## 1. 전체 시퀀스

### A. VMG 부팅 (Linux)

```
[VMG 부팅]
    ↓
[Linux Kernel 시작]
    ↓
[vmg_gateway 프로세스 시작]
    ↓
┌─────────────────────────────────────────────┐
│ DoIP Server 초기화                          │
│  - mbedTLS 컨텍스트 생성                    │
│  - 인증서 로드 (server_cert.pem)           │
│  - Port 13400 Listen                        │
│  - Accept 대기 ← 여기서 대기!              │
└─────────────────────────────────────────────┘
    ↓
[대기 상태 - ZG/ECU 연결 기다림]
```

**중요:** VMG는 부팅 시 **Handshake를 하지 않습니다!**  
단지 **Listen 상태**로 연결을 기다립니다.

---

### B. Zonal Gateway (ZG) 부팅 (TC375)

```
[전원 ON]
    ↓
[SSW (10ms)]
    ↓
[Bootloader (50ms)]
    ↓
[Application 시작 (100ms)]
    ↓
┌─────────────────────────────────────────────┐
│ FreeRTOS 시작 (50ms)                        │
│  - 하드웨어 초기화                          │
│  - Ethernet PHY 초기화                      │
│  - CAN 초기화                               │
│  - Task 생성                                │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ DoIP Server Task 시작                       │
│  - mbedTLS 컨텍스트 생성                    │
│  - 인증서 로드 (zg_server_cert.pem)        │
│  - Port 13400 Listen                        │
│  - Accept 대기 ← 여기서 대기!              │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ DoIP Client Task 시작                       │
│  - mbedTLS 컨텍스트 생성                    │
│  - 인증서 로드 (zg_client_cert.pem)        │
│  - VMG 연결 시도 (192.168.1.1:13400)       │
│      ↓                                      │
│  ┌─────────────────────────────────┐       │
│  │ mbedTLS Handshake 시작! ← 여기! │       │
│  │  1. TCP 연결                     │       │
│  │  2. ClientHello                  │       │
│  │  3. ServerHello                  │       │
│  │  4. Certificate Exchange         │       │
│  │  5. Key Exchange                 │       │
│  │  6. Finished                     │       │
│  └─────────────────────────────────┘       │
│      ↓                                      │
│  [TLS 연결 성공]                            │
│  [DoIP Routing Activation]                 │
└─────────────────────────────────────────────┘
    ↓
[대기 상태 - VMG/ECU 메시지 처리]
```

**중요:** ZG는 부팅 시:
- **DoIP Server는 Listen만** (Handshake 안 함)
- **DoIP Client는 VMG 연결 시도** → **이때 Handshake 발생!**

---

### C. End Node ECU 부팅 (TC375)

```
[전원 ON]
    ↓
[SSW (10ms)]
    ↓
[Bootloader (50ms)]
    ↓
[Application 시작 (100ms)]
    ↓
┌─────────────────────────────────────────────┐
│ FreeRTOS 시작 (50ms)                        │
│  - 하드웨어 초기화                          │
│  - CAN 초기화                               │
│  - 센서 초기화                              │
│  - Task 생성                                │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ DoIP Client Task 시작                       │
│  - mbedTLS 컨텍스트 생성                    │
│  - 인증서 로드 (ecu_client_cert.pem)       │
│  - ZG 연결 시도 (192.168.2.1:13400)        │
│      ↓                                      │
│  ┌─────────────────────────────────┐       │
│  │ mbedTLS Handshake 시작! ← 여기! │       │
│  │  1. TCP 연결                     │       │
│  │  2. ClientHello                  │       │
│  │  3. ServerHello                  │       │
│  │  4. Certificate Exchange         │       │
│  │  5. Key Exchange                 │       │
│  │  6. Finished                     │       │
│  └─────────────────────────────────┘       │
│      ↓                                      │
│  [TLS 연결 성공]                            │
│  [DoIP Routing Activation]                 │
└─────────────────────────────────────────────┘
    ↓
[대기 상태 - ZG 메시지 처리]
```

**중요:** ECU는 부팅 시:
- **DoIP Client만 있음** (Server 없음)
- **ZG 연결 시도** → **이때 Handshake 발생!**

---

## 2. Handshake 타이밍 상세

### 시나리오 1: ZG → VMG 연결

```
Time  VMG (Server)              ZG (Client)
─────────────────────────────────────────────────
T0    [부팅 완료]               [부팅 중...]
      [DoIP Server Listen]      
      [대기 중...]              
                                
T1                              [부팅 완료]
                                [DoIP Client 시작]
                                
T2                              [TCP Connect 시도]
      [Accept 성공]             ────────────────>
                                
T3    [TLS Handshake 시작]
      <────────────────────────────────────────>
      ClientHello               ────────────────>
      <────────────────────     ServerHello
      Certificate               ────────────────>
      <────────────────────     Certificate
      CertificateVerify         ────────────────>
      <────────────────────     Finished
      Finished                  ────────────────>
      [TLS 연결 성공]           [TLS 연결 성공]
                                
T4                              [DoIP Routing Activation]
      [Routing Activation OK]   ────────────────>
      <────────────────────     [Response]
                                
T5    [연결 유지]               [연결 유지]
      [메시지 대기]             [메시지 대기]
```

**Handshake 소요 시간:** ~100-200ms

---

### 시나리오 2: ECU → ZG 연결

```
Time  ZG (Server)                ECU (Client)
─────────────────────────────────────────────────
T0    [부팅 완료]               [부팅 중...]
      [DoIP Server Listen]      
      [대기 중...]              
                                
T1                              [부팅 완료]
                                [DoIP Client 시작]
                                
T2                              [TCP Connect 시도]
      [Accept 성공]             ────────────────>
                                
T3    [TLS Handshake 시작]
      <────────────────────────────────────────>
      ClientHello               ────────────────>
      <────────────────────     ServerHello
      Certificate               ────────────────>
      <────────────────────     Certificate
      CertificateVerify         ────────────────>
      <────────────────────     Finished
      Finished                  ────────────────>
      [TLS 연결 성공]           [TLS 연결 성공]
                                
T4                              [DoIP Routing Activation]
      [Routing Activation OK]   ────────────────>
      <────────────────────     [Response]
                                
T5    [연결 유지]               [연결 유지]
      [메시지 대기]             [메시지 대기]
```

**Handshake 소요 시간:** ~100-200ms

---

## 3. 코드 레벨 상세

### A. VMG DoIP Server (mbedTLS)

```cpp
// vehicle_gateway/example_vmg_doip_server_mbedtls.cpp

int main() {
    // 1. mbedTLS 초기화
    mbedtls_doip_server server;
    mbedtls_doip_server_init(&server);
    
    // 2. 인증서 로드
    mbedtls_doip_server_set_cert(&server, 
        "certs/vmg_server_cert.pem",
        "certs/vmg_server_key.pem",
        "certs/ca_cert.pem");
    
    // 3. Listen 시작
    mbedtls_doip_server_listen(&server, 13400);
    
    printf("[VMG] DoIP Server listening on port 13400\n");
    printf("[VMG] Waiting for ZG/ECU connections...\n");
    
    // 4. Accept 루프 (여기서 대기!)
    while (running) {
        // Client 연결 대기
        mbedtls_doip_client* client = mbedtls_doip_server_accept(&server);
        
        if (client) {
            // ← 여기서 Handshake 발생!
            printf("[VMG] Client connected, TLS handshake successful\n");
            
            // 새 스레드에서 처리
            std::thread client_thread(handle_client, client);
            client_thread.detach();
        }
    }
    
    return 0;
}
```

**중요:** `mbedtls_doip_server_accept()` 내부에서 **자동으로 Handshake 수행**

---

### B. ZG DoIP Client (mbedTLS)

```c
// zonal_gateway/tc375/src/doip_client_mbedtls.c

void doip_client_task(void* params) {
    // 1. mbedTLS 초기화
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert, clicert;
    mbedtls_pk_context pkey;
    
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    
    // 2. 인증서 로드
    mbedtls_x509_crt_parse_file(&clicert, "zg_client_cert.pem");
    mbedtls_pk_parse_keyfile(&pkey, "zg_client_key.pem", NULL);
    mbedtls_x509_crt_parse_file(&cacert, "ca_cert.pem");
    
    // 3. TLS 설정
    mbedtls_ssl_config_defaults(&conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey);
    
    mbedtls_ssl_setup(&ssl, &conf);
    
    // 4. VMG 연결 시도
    printf("[ZG] Connecting to VMG (192.168.1.1:13400)...\n");
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in vmg_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(13400),
        .sin_addr.s_addr = inet_addr("192.168.1.1")
    };
    
    // TCP 연결
    if (connect(sockfd, (struct sockaddr*)&vmg_addr, sizeof(vmg_addr)) < 0) {
        printf("[ZG] TCP connection failed\n");
        return;
    }
    
    printf("[ZG] TCP connected, starting TLS handshake...\n");
    
    // 5. TLS Handshake (여기서 발생!)
    mbedtls_ssl_set_bio(&ssl, &sockfd, mbedtls_net_send, mbedtls_net_recv, NULL);
    
    int ret = mbedtls_ssl_handshake(&ssl);  // ← Handshake!
    
    if (ret != 0) {
        printf("[ZG] TLS handshake failed: -0x%x\n", -ret);
        return;
    }
    
    printf("[ZG] TLS handshake successful!\n");
    printf("[ZG] Cipher suite: %s\n", mbedtls_ssl_get_ciphersuite(&ssl));
    
    // 6. DoIP Routing Activation
    send_routing_activation(&ssl);
    
    // 7. 메시지 루프
    while (1) {
        // VMG로부터 메시지 수신 대기
        uint8_t buf[4096];
        int len = mbedtls_ssl_read(&ssl, buf, sizeof(buf));
        
        if (len > 0) {
            handle_doip_message(buf, len);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**중요:** `mbedtls_ssl_handshake()` 호출 시 **Handshake 수행**

---

### C. ECU DoIP Client (mbedTLS)

```c
// end_node_ecu/tc375/src/ecu_node.c

void doip_client_task(void* params) {
    // 1. mbedTLS 초기화
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert, clicert;
    mbedtls_pk_context pkey;
    
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    
    // 2. 인증서 로드
    mbedtls_x509_crt_parse_file(&clicert, "ecu_client_cert.pem");
    mbedtls_pk_parse_keyfile(&pkey, "ecu_client_key.pem", NULL);
    mbedtls_x509_crt_parse_file(&cacert, "ca_cert.pem");
    
    // 3. TLS 설정
    mbedtls_ssl_config_defaults(&conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey);
    
    mbedtls_ssl_setup(&ssl, &conf);
    
    // 4. ZG 연결 시도
    printf("[ECU] Connecting to ZG (192.168.2.1:13400)...\n");
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in zg_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(13400),
        .sin_addr.s_addr = inet_addr("192.168.2.1")
    };
    
    // TCP 연결
    if (connect(sockfd, (struct sockaddr*)&zg_addr, sizeof(zg_addr)) < 0) {
        printf("[ECU] TCP connection failed\n");
        return;
    }
    
    printf("[ECU] TCP connected, starting TLS handshake...\n");
    
    // 5. TLS Handshake (여기서 발생!)
    mbedtls_ssl_set_bio(&ssl, &sockfd, mbedtls_net_send, mbedtls_net_recv, NULL);
    
    int ret = mbedtls_ssl_handshake(&ssl);  // ← Handshake!
    
    if (ret != 0) {
        printf("[ECU] TLS handshake failed: -0x%x\n", -ret);
        return;
    }
    
    printf("[ECU] TLS handshake successful!\n");
    printf("[ECU] Cipher suite: %s\n", mbedtls_ssl_get_ciphersuite(&ssl));
    
    // 6. DoIP Routing Activation
    send_routing_activation(&ssl);
    
    // 7. 메시지 루프
    while (1) {
        // ZG로부터 메시지 수신 대기
        uint8_t buf[4096];
        int len = mbedtls_ssl_read(&ssl, buf, sizeof(buf));
        
        if (len > 0) {
            handle_doip_message(buf, len);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**중요:** `mbedtls_ssl_handshake()` 호출 시 **Handshake 수행**

---

## 4. 전체 타임라인

```
Time   VMG                    ZG                     ECU
────────────────────────────────────────────────────────────
T0     [부팅 완료]            [전원 OFF]             [전원 OFF]
       [DoIP Server Listen]
       [대기...]

T1                            [전원 ON]              [전원 OFF]
                              [SSW → Bootloader]
                              [App 시작]

T2                            [FreeRTOS 시작]        [전원 OFF]
                              [DoIP Server Listen]
                              [DoIP Client 시작]

T3                            [VMG 연결 시도]        [전원 OFF]
       [Accept]               ──────────────────>
       <──────────────────────────────────────>
       [TLS Handshake]        [TLS Handshake]
       [연결 성공]            [연결 성공]

T4                                                   [전원 ON]
                                                     [SSW → Bootloader]
                                                     [App 시작]

T5                                                   [FreeRTOS 시작]
                                                     [DoIP Client 시작]

T6                            [Accept]               [ZG 연결 시도]
                              <──────────────────────────────
                              <──────────────────────────────>
                              [TLS Handshake]        [TLS Handshake]
                              [연결 성공]            [연결 성공]

T7     [대기]                [대기]                 [대기]
       [메시지 처리]          [메시지 라우팅]        [메시지 처리]
```

---

## 5. 요약

### 질문: "ZG와 VMG, 그리고 ZG와 ECU의 mbedTLS 핸드쉐이크는 FreeRTOS 이후 DoIP Server/Client 시작하면서 하는거네?"

### 정확한 답변:

**부분적으로 맞습니다!**

1. **DoIP Server 시작 시:**
   - mbedTLS 초기화 ✅
   - 인증서 로드 ✅
   - Listen 시작 ✅
   - **Handshake는 안 함** ❌ (대기만)

2. **DoIP Client 시작 시:**
   - mbedTLS 초기화 ✅
   - 인증서 로드 ✅
   - 서버 연결 시도 ✅
   - **Handshake 발생!** ✅ ← **여기서 발생!**

3. **Handshake 타이밍:**
   - **Server**: 부팅 시 Listen만, Client 연결 시 Handshake
   - **Client**: 부팅 시 연결 시도, 이때 Handshake 발생

### 정확한 표현:

**"ZG와 ECU는 FreeRTOS 시작 후 DoIP Client가 서버에 연결할 때 mbedTLS Handshake가 발생합니다."**

- ZG → VMG: ZG의 DoIP Client가 VMG 연결 시
- ECU → ZG: ECU의 DoIP Client가 ZG 연결 시

**Handshake는 "연결 시점"에 동적으로 발생합니다!** ✅

