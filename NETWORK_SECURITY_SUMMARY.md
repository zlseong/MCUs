# Network Security Summary

## 보안 아키텍처 개요

```
┌─────────────────────────────────────────────────────────────┐
│                     External Internet                        │
│                   (Quantum Threat 존재)                      │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           │ [PQC-TLS]
                           │ • ML-KEM-768 (Key Exchange)
                           │ • ECDSA-P256 (Signature)
                           │ • mTLS Authentication
                           │ • ~15ms handshake
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                         VMG                                  │
│                    (MacBook Air)                             │
│                                                              │
│  [Firewall/Gateway Role]                                    │
│  - External: PQC-TLS (HTTPS/MQTT)                          │
│  - Internal: Plain DoIP (No encryption)                     │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           │ [Plain DoIP]
                           │ • No TLS
                           │ • TCP Port 13400
                           │ • <1ms latency
                           │ • Physical security
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                  Zonal Gateway (ZG)                          │
│                   (TC375 MCU)                                │
│                                                              │
│  [Physically Isolated Vehicle Network]                      │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           │ [Plain DoIP]
                           │ • No TLS
                           │ • TCP Port 13400
                           │ • <1ms latency
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                   End Node ECU                               │
│                   (TC375 MCU)                                │
│                                                              │
│  [Physical Access Only]                                     │
└──────────────────────────────────────────────────────────────┘
```

## 보안 설정 상세

### 1. External Communication (PQC-TLS 사용) ✓

#### 1.1 VMG → OTA Server (HTTPS)

**파일**: `vehicle_gateway/src/https_client.cpp`

```cpp
// Line 138
const int PQC_CONFIG_ID = 1;  // ML-KEM-768 + ECDSA-P256
const PQC_Config* config = &PQC_CONFIGS[PQC_CONFIG_ID];

// OpenSSL with PQC
SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
SSL_CTX_set_groups_list(ctx, config->openssl_groups);      // mlkem768
SSL_CTX_set1_sigalgs_list(ctx, config->openssl_sigalgs);   // ecdsa_secp256r1_sha256
```

**보안 특성:**
- **Key Exchange**: ML-KEM-768 (Quantum-safe, 192-bit)
- **Signature**: ECDSA-P256 (Lightweight, 128-bit)
- **Cipher**: AES-256-GCM
- **Authentication**: mTLS (Mutual certificate verification)
- **Handshake Time**: ~14ms
- **Overhead**: ~2.3 KB

#### 1.2 VMG → MQTT Broker

**파일**: `vehicle_gateway/src/mqtt_client.cpp`

```cpp
// Line 44
const int PQC_CONFIG_ID = 1;  // ML-KEM-768 + ECDSA-P256
const PQC_Config* config = &PQC_CONFIGS[PQC_CONFIG_ID];

// MQTT over PQC-TLS
mqtt_client_create(broker_url, config, cert, key, ca);
```

**보안 특성:**
- 동일한 PQC-TLS 설정
- QoS 1/2로 메시지 신뢰성 보장
- Keep-alive를 통한 연결 모니터링

### 2. In-Vehicle Communication (Plain DoIP) ✓

#### 2.1 VMG DoIP Server

**파일**: `vehicle_gateway/example_vmg_doip_server.cpp`

```cpp
// Line 42
DoIPServerConfig config;
config.host = "0.0.0.0";
config.port = 13400;
config.enable_tls = false;  // ← Plain TCP (No TLS)
```

**구현**: `vehicle_gateway/src/doip_server.cpp`

```cpp
// Plain TCP socket (No SSL/TLS wrapper)
tcp_socket_ = socket(AF_INET, SOCK_STREAM, 0);
bind(tcp_socket_, (struct sockaddr*)&addr, sizeof(addr));
listen(tcp_socket_, LISTEN_BACKLOG);

// Direct read/write (No SSL_read/SSL_write)
recv(client_socket, buffer, size, 0);
send(client_socket, buffer, size, 0);
```

**보안 특성:**
- **암호화**: 없음 (Plain text)
- **인증**: DoIP Routing Activation (논리 주소 기반)
- **보안 모델**: Physical isolation + Access control
- **성능**: <1ms latency

#### 2.2 Zonal Gateway

**파일**: `zonal_gateway/tc375/src/zonal_gateway.c`

```c
// Server role (Zone 내 ECU들에게)
int doip_server_tcp_socket;  // Plain TCP
int doip_server_udp_socket;  // Plain UDP

// Client role (VMG에게)
DoIPClient_t vmg_client;     // Plain TCP to VMG
```

**보안 특성:**
- Plain TCP/UDP sockets
- No TLS overhead
- DoIP protocol-level authentication

#### 2.3 End Node ECU

**파일**: `end_node_ecu/tc375/src/ecu_node.c`

```c
// DoIP Client to Zonal Gateway
DoIPClient_t doip_client;  // Plain TCP
```

**보안 특성:**
- Plain TCP connection
- UDS Security Access (0x27) for sensitive operations

## 보안 계층 분석

### External Network Security (인터넷 노출)

| Layer | Protocol | Security | Purpose |
|-------|----------|----------|---------|
| Application | HTTPS, MQTT | PQC-TLS | Data encryption |
| Presentation | TLS 1.3 | ML-KEM + ECDSA | Key exchange + Auth |
| Session | mTLS | X.509 Certs | Mutual authentication |
| Transport | TCP | Port 443/8883 | Reliable delivery |

**Threat Model:**
- **위협**: Harvest now, decrypt later (양자 컴퓨터)
- **대응**: Post-Quantum Cryptography (ML-KEM)
- **보호**: 차량 텔레메트리, OTA 펌웨어

### In-Vehicle Network Security (물리적 격리)

| Layer | Protocol | Security | Purpose |
|-------|----------|----------|---------|
| Application | DoIP, UDS | Routing Activation | Access control |
| Session | None | Physical isolation | - |
| Transport | TCP/UDP | Port 13400 | Reliable/Fast |
| Network | IPv4 | 192.168.1.0/24 | Local subnet |
| Physical | Ethernet | Shielded cable | EMI protection |

**Threat Model:**
- **위협**: Physical access required (차량 내부 침입)
- **대응**: Physical security + DoIP authentication
- **보호**: 진단 포트는 차량 내부에 위치

## 보안 대책 상세

### External Communication 보안

#### 1. Post-Quantum Cryptography
```cpp
// ML-KEM-768: Quantum-safe key exchange
// ECDSA-P256: Lightweight digital signature
```

**위협 시나리오:**
- 공격자가 TLS 트래픽을 저장
- 미래의 양자 컴퓨터로 복호화 시도
- **대응**: ML-KEM으로 키 교환 → 양자 컴퓨터로도 복호화 불가

#### 2. Mutual TLS (mTLS)
```cpp
// VMG와 Server 모두 인증서 검증
SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
SSL_CTX_load_verify_locations(ctx, ca_file, NULL);
```

**위협 시나리오:**
- Man-in-the-Middle 공격
- **대응**: 양방향 인증서 검증

#### 3. Certificate Pinning
```cpp
// VMG는 특정 CA만 신뢰
SSL_CTX_load_verify_locations(ctx, "certs/ca.crt", NULL);
```

### In-Vehicle Communication 보안

#### 1. Physical Isolation
```
[Internet] → [VMG Firewall] → [Isolated Vehicle Network]
                  ↑
                  └─ 유일한 게이트웨이
```

**보안 특성:**
- VMG가 유일한 외부 연결점
- 내부 네트워크는 인터넷과 물리적 분리
- 공격자는 차량 내부 접근 필요

#### 2. DoIP Routing Activation
```c
// 클라이언트는 먼저 routing activation 필요
uint16_t source_address = 0x0E00;  // Tester
uint16_t target_address = 0x0100;  // ECU

doip_send_routing_activation(client, source_address);
// Response: 0x10 (Success) or 0x00-0xFF (Error codes)
```

**보안 특성:**
- 논리 주소 기반 접근 제어
- 알려지지 않은 클라이언트는 거부

#### 3. UDS Security Access (0x27)
```c
// 민감한 서비스는 seed/key 인증 필요
uds_request_seed(session);           // 0x27 01
// ECU returns random seed
uds_send_key(session, computed_key);  // 0x27 02
// ECU verifies key
```

**보안 특성:**
- Challenge-response 인증
- ECU별 고유 알고리즘
- 틀리면 일정 시간 잠금

#### 4. Diagnostic Session Control
```c
// 민감한 작업은 extended session 필요
uds_diagnostic_session_control(0x03);  // Extended diagnostic
// 일정 시간 후 자동으로 default session으로 복귀
```

## 성능 vs 보안 트레이드오프

### External Communication (PQC-TLS)

**성능 오버헤드:**
- Handshake: ~14ms (ML-KEM-768 + ECDSA-P256)
- Data transfer: ~100 Mbps (암호화 오버헤드 <1%)
- Memory: ~20 KB per session

**보안 이득:**
- Quantum-safe key exchange
- Harvest now, decrypt later 방어
- 30년+ 보안 수명

**결론**: 오버헤드 감수 가능 (OTA는 백그라운드 작업)

### In-Vehicle Communication (Plain DoIP)

**성능 이득:**
- Latency: <1ms
- No handshake delay
- Minimal memory footprint

**보안 수준:**
- Physical isolation
- DoIP/UDS protocol security
- 충분한 보안 (물리적 접근 필요)

**결론**: TLS 불필요 (성능 > 추가 암호화)

## 위협 모델 비교

### External Network Threats

| Threat | Likelihood | Impact | Mitigation |
|--------|-----------|--------|------------|
| Eavesdropping | High | High | PQC-TLS encryption |
| MITM | Medium | High | mTLS + Certificate pinning |
| Quantum attack | Low (now) | Critical | ML-KEM (quantum-safe) |
| DDoS | Medium | Medium | Rate limiting, firewall |

### In-Vehicle Network Threats

| Threat | Likelihood | Impact | Mitigation |
|--------|-----------|--------|------------|
| Physical access | Low | High | Physical security, locked doors |
| OBD-II attack | Low | Medium | DoIP routing activation |
| Unauthorized ECU | Very Low | Medium | Logical address whitelist |
| ECU spoofing | Very Low | High | UDS security access (0x27) |

## 규정 준수

### ISO 21434 (Cybersecurity)
- ✅ Threat analysis and risk assessment (TARA)
- ✅ Secure communication (PQC-TLS for external)
- ✅ Access control (DoIP routing activation)
- ✅ Cryptographic support (NIST-approved PQC)

### UNECE WP.29 R155
- ✅ Vehicle cyber security management system
- ✅ Secure software update (OTA with PQC-TLS)
- ✅ Data protection (encryption for external comms)
- ✅ Vehicle communication security

## 구성 파일 위치

### PQC-TLS 설정
- `vehicle_gateway/src/vmg_gateway.cpp` Line 22
- `vehicle_gateway/src/https_client.cpp` Line 138
- `vehicle_gateway/src/mqtt_client.cpp` Line 44
- `common/protocol/pqc_params.h` - Parameter definitions

### Plain DoIP 설정
- `vehicle_gateway/example_vmg_doip_server.cpp` Line 42
- `vehicle_gateway/src/doip_server.cpp` - Implementation
- `zonal_gateway/tc375/src/zonal_gateway.c` - ZG implementation
- `end_node_ecu/tc375/src/ecu_node.c` - ECU implementation

## 요약

### ✓ PQC-TLS 사용 (External)
- VMG ↔ OTA Server (HTTPS)
- VMG ↔ MQTT Broker
- **이유**: 인터넷 노출, 양자 위협 존재

### ✗ TLS 미사용 (In-Vehicle)
- VMG ↔ Zonal Gateway (DoIP)
- Zonal Gateway ↔ ECU (DoIP)
- **이유**: 물리적 격리, 성능 요구사항

### 보안 전략
1. **Defense in Depth**: 계층별 보안 대책
2. **Risk-based**: 위협 수준에 따른 보안 수준
3. **Performance-aware**: 성능 요구사항 고려
4. **Future-proof**: 양자 내성 암호화

---

**핵심 원칙**: 적절한 곳에 적절한 보안
- External: 최고 보안 (PQC-TLS)
- Internal: 적정 보안 (Physical + DoIP)

