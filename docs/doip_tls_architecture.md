# DoIP + TLS  

## [DOCS]  

### ISO 13400: DoIP (Diagnostics over Internet Protocol)

```
ISO 13400-1: Requirements
ISO 13400-2: Transport protocol and network layer
ISO 13400-3: Wiring harness diagnostic interfaces
ISO 13400-4: Protocol extensions
```

###  DoIP :

```
+-------------------------------------+
|  Application Layer                  |
|  - UDS (ISO 14229)                  |
|  - Diagnostic Services              |
+-------------------------------------+
|  DoIP Layer (ISO 13400)             |
|  - Message routing                  |
|  - Source/Target addressing         |
+-------------------------------------+
|  Transport Layer                    |
|  - TCP (port 13400)                 |
+-------------------------------------+
|  Network Layer                      |
|  - IPv4 / IPv6                      |
+-------------------------------------+
|  Data Link Layer                    |
|  - Ethernet (100BASE-TX)            |
+-------------------------------------+
```

---

## [SECURITY] DoIP + TLS  

###  :

```
ISO 13400 (Original):
-   (Plain TCP)
- Port 13400

ISO 13400 + SecOC:
-   (MAC)
-  
-    !

DoIP over TLS (New Trend):
- TLS 1.2 / 1.3
- Port 13400 (StartTLS) or 13401 (TLS direct)
-   + 
```

---

## [BUILD]   : DoIP + TLS

###   :

```
+------------------------------------------------+
|  : VMG (Vehicle Management Gateway)        |
|  - TLS Client                                  |
|  - DoIP                                |
+----------------+-------------------------------+
                 | TLS 1.3 over TCP
                 | Port 13401
                 v
+------------------------------------------------+
|  TC375: Application ( SW)                  |
|  +------------------------------------------+ |
|  |  TLS Server (mbedTLS)                    | |
|  |  - X.509                           | |
|  |  - TLS 1.3 handshake                     | |
|  |  - AES-256-GCM                     | |
|  +------------+-----------------------------+ |
|               | Decrypted                     |
|  +------------v-----------------------------+ |
|  |  DoIP Layer                              | |
|  |  - Routing activation                    | |
|  |  - Diagnostic message                    | |
|  +------------+-----------------------------+ |
|               |                               |
|  +------------v-----------------------------+ |
|  |  UDS Handler (ISO 14229)                 | |
|  |  - 0x10: Diagnostic Session Control      | |
|  |  - 0x27: Security Access                 | |
|  |  - 0x31: Routine Control                 | |
|  |  - 0x34-0x37: Download (OTA)             | |
|  +------------------------------------------+ |
+------------------------------------------------+
```

---

## [PACKAGE] DoIP  

### DoIP Generic Header:

```c
typedef struct {
    uint8_t  protocol_version;  // 0x02 (ISO 13400-2:2012)
    uint8_t  inverse_version;   // 0xFD
    uint16_t payload_type;      // Message type
    uint32_t payload_length;    // Length of data
    // Followed by payload
} __attribute__((packed)) DoIPHeader;
```

### DoIP Payload Types:

```c
// Vehicle identification
#define DOIP_VIN_REQUEST            0x0001
#define DOIP_VIN_RESPONSE           0x0002

// Routing activation
#define DOIP_ROUTING_ACTIVATION_REQ 0x0005
#define DOIP_ROUTING_ACTIVATION_RES 0x0006

// Diagnostic message
#define DOIP_DIAGNOSTIC_MESSAGE     0x8001
#define DOIP_DIAGNOSTIC_ACK         0x8002
#define DOIP_DIAGNOSTIC_NACK        0x8003
```

### DoIP Diagnostic Message:

```c
typedef struct {
    DoIPHeader header;
    uint16_t source_address;    //   (: 0x0E80)
    uint16_t target_address;    // ECU  (: 0x1234)
    uint8_t  user_data[];       // UDS 
} __attribute__((packed)) DoIPDiagMessage;
```

---

## [SECURITY] TLS  DoIP 

### Option A: DoIP over TLS (Direct)

```
 -> TLS Handshake -> 
 -> [Encrypted DoIP] -> 
 <- [Encrypted DoIP] <- 
```

**:**
- TCP 13401 (TLS-wrapped DoIP)

**:**
-  
- Man-in-the-middle 
-  

**:**
-  DoIP    

### Option B: StartTLS (Upgrade)

```
 -> DoIP Hello (Plain) -> 
 <- DoIP TLS Upgrade  <- 
 -> TLS Handshake -> 
 -> [Encrypted DoIP] -> 
```

**:**
- TCP 13400 (Plain -> TLS )

**:**
-  DoIP  
-  TLS

**:**
-  Hello 
-  

---

## [CODE] : TC375 DoIP + TLS Server

### 1. TLS Server 

```c
// doip_tls_server.h
#ifndef DOIP_TLS_SERVER_H
#define DOIP_TLS_SERVER_H

#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#define DOIP_TLS_PORT       13401
#define DOIP_PLAIN_PORT     13400

class DoIPTLSServer {
public:
    DoIPTLSServer();
    ~DoIPTLSServer();
    
    // TLS 
    bool initTLS(const char* cert_path, const char* key_path);
    
    //  
    bool start(uint16_t port);
    
    //   
    bool acceptClient();
    
    // DoIP   (TLS decrypt)
    int receiveDoIPMessage(uint8_t* buffer, size_t max_len);
    
    // DoIP   (TLS encrypt)
    int sendDoIPMessage(const uint8_t* data, size_t len);
    
private:
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_net_context listen_fd;
    mbedtls_net_context client_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
};

#endif
```

### 2. DoIP  

```c
// doip_handler.cpp

#include "doip_handler.h"
#include <cstring>

// DoIP Generic Header 
bool DoIPHandler::parseHeader(const uint8_t* data, DoIPHeader* header) {
    if (data[0] != 0x02 || data[1] != 0xFD) {
        return false;  // Invalid protocol version
    }
    
    header->protocol_version = data[0];
    header->inverse_version = data[1];
    header->payload_type = (data[2] << 8) | data[3];
    header->payload_length = (data[4] << 24) | (data[5] << 16) | 
                            (data[6] << 8) | data[7];
    
    return true;
}

// Routing Activation 
void DoIPHandler::handleRoutingActivation(const uint8_t* payload) {
    uint16_t source_addr = (payload[0] << 8) | payload[1];
    uint8_t activation_type = payload[2];
    
    std::cout << "[DoIP] Routing activation from 0x" 
              << std::hex << source_addr << std::endl;
    
    // Activation response 
    uint8_t response[13];
    response[0] = 0x02;  // Protocol version
    response[1] = 0xFD;  // Inverse
    response[2] = 0x00;  // Payload type (MSB)
    response[3] = 0x06;  // ROUTING_ACTIVATION_RES
    response[4] = 0x00;  // Length (MSB)
    response[5] = 0x00;
    response[6] = 0x00;
    response[7] = 0x05;  // 5 bytes payload
    
    // Payload
    response[8] = (source_addr >> 8) & 0xFF;  // Source
    response[9] = source_addr & 0xFF;
    response[10] = 0x12;  // Our address (MSB)
    response[11] = 0x34;  // Our address (LSB)
    response[12] = 0x10;  // Success
    
    sendResponse(response, sizeof(response));
}

// Diagnostic Message 
void DoIPHandler::handleDiagnosticMessage(const uint8_t* payload, 
                                         size_t payload_len) {
    uint16_t source_addr = (payload[0] << 8) | payload[1];
    uint16_t target_addr = (payload[2] << 8) | payload[3];
    
    // UDS  
    const uint8_t* uds_data = payload + 4;
    size_t uds_len = payload_len - 4;
    
    std::cout << "[DoIP] Diagnostic message:" << std::endl;
    std::cout << "  Source: 0x" << std::hex << source_addr << std::endl;
    std::cout << "  Target: 0x" << std::hex << target_addr << std::endl;
    std::cout << "  UDS SID: 0x" << std::hex << (int)uds_data[0] << std::endl;
    
    // UDS Handler 
    uint8_t uds_response[4096];
    size_t response_len = uds_handler_.process(uds_data, uds_len, 
                                               uds_response, sizeof(uds_response));
    
    // DoIP Diagnostic Message wrapping
    sendDiagnosticResponse(target_addr, source_addr, 
                          uds_response, response_len);
}
```

### 3. : TLS + DoIP

```c
// main.cpp - TC375 Application

#include "doip_tls_server.h"
#include "doip_handler.h"

int main() {
    // 1. TLS Server 
    DoIPTLSServer tls_server;
    if (!tls_server.initTLS("/certs/server.crt", "/certs/server.key")) {
        std::cerr << "TLS init failed" << std::endl;
        return -1;
    }
    
    // 2. DoIP Handler 
    DoIPHandler doip_handler;
    
    // 3.  
    if (!tls_server.start(DOIP_TLS_PORT)) {
        std::cerr << "Server start failed" << std::endl;
        return -1;
    }
    
    std::cout << "[DoIP] Server listening on port " << DOIP_TLS_PORT << std::endl;
    
    // 4.  
    while (true) {
        //   
        if (tls_server.acceptClient()) {
            std::cout << "[DoIP] Client connected" << std::endl;
            
            // DoIP   (TLS  decrypt)
            uint8_t buffer[8192];
            while (true) {
                int len = tls_server.receiveDoIPMessage(buffer, sizeof(buffer));
                if (len <= 0) break;
                
                // DoIP 
                doip_handler.processMessage(buffer, len);
                
                // Response doip_handler tls_server  
            }
            
            std::cout << "[DoIP] Client disconnected" << std::endl;
        }
    }
    
    return 0;
}
```

---

## [CONFIG] VMG (Gateway)  

```cpp
// vmg_doip_client.cpp

#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>

class VMGDoIPClient {
public:
    // TC375 
    bool connect(const char* tc375_ip, uint16_t port = 13401) {
        // TLS 
        mbedtls_net_connect(&server_fd, tc375_ip, 
                           std::to_string(port).c_str(), 
                           MBEDTLS_NET_PROTO_TCP);
        
        // TLS Handshake
        mbedtls_ssl_handshake(&ssl);
        
        // Routing activation
        sendRoutingActivation();
        
        return true;
    }
    
    // UDS   (DoIP + TLS)
    std::vector<uint8_t> sendUDS(const std::vector<uint8_t>& uds_request) {
        // DoIP Diagnostic Message 
        std::vector<uint8_t> doip_msg;
        
        // DoIP Header
        doip_msg.push_back(0x02);  // Protocol version
        doip_msg.push_back(0xFD);  // Inverse
        doip_msg.push_back(0x80);  // Payload type (MSB)
        doip_msg.push_back(0x01);  // DIAGNOSTIC_MESSAGE
        
        uint32_t payload_len = 4 + uds_request.size();
        doip_msg.push_back((payload_len >> 24) & 0xFF);
        doip_msg.push_back((payload_len >> 16) & 0xFF);
        doip_msg.push_back((payload_len >> 8) & 0xFF);
        doip_msg.push_back(payload_len & 0xFF);
        
        // Source/Target address
        doip_msg.push_back(0x0E);  // Source (Tester)
        doip_msg.push_back(0x80);
        doip_msg.push_back(0x12);  // Target (TC375)
        doip_msg.push_back(0x34);
        
        // UDS data
        doip_msg.insert(doip_msg.end(), uds_request.begin(), uds_request.end());
        
        // TLS  
        mbedtls_ssl_write(&ssl, doip_msg.data(), doip_msg.size());
        
        // Response  (TLS  decrypt)
        uint8_t response[4096];
        int len = mbedtls_ssl_read(&ssl, response, sizeof(response));
        
        // DoIP header  UDS response 
        return extractUDSResponse(response, len);
    }
};
```

---

## [TABLE]   

### OTA  :

```
VMG -> TC375: TLS Handshake
VMG -> TC375: [Encrypted] DoIP Routing Activation
VMG <- TC375: [Encrypted] DoIP Routing Activation Response

VMG -> TC375: [Encrypted] DoIP Diagnostic Message
              +- UDS 0x10 0x03 (Extended Session)
VMG <- TC375: [Encrypted] DoIP Diagnostic Response
              +- UDS 0x50 0x03 (Positive Response)

VMG -> TC375: [Encrypted] DoIP Diagnostic Message
              +- UDS 0x27 0x01 (Security Access - Seed)
VMG <- TC375: [Encrypted] DoIP Diagnostic Response
              +- UDS 0x67 0x01 [seed data]

VMG -> TC375: [Encrypted] DoIP Diagnostic Message
              +- UDS 0x27 0x02 (Security Access - Key)
VMG <- TC375: [Encrypted] DoIP Diagnostic Response
              +- UDS 0x67 0x02 (Unlocked)

VMG -> TC375: [Encrypted] DoIP Diagnostic Message
              +- UDS 0x34 (Request Download)
VMG <- TC375: [Encrypted] DoIP Diagnostic Response
              +- UDS 0x74 (Ready)

... (Data transfer) ...
```

---

## [SECURITY]  

```
Level 1: Plain DoIP ()
-  
-  
- MITM  

Level 2: DoIP + SecOC (ISO 21434)
-   (MAC)
-  
-   

Level 3: DoIP over TLS ()
-   (AES-256-GCM)
-  (X.509 )
- MITM 
- Forward Secrecy (TLS 1.3)
```

---

## [TARGET]   

1. [OK]    
2. [PENDING] DoIP + TLS Server 
3. [PENDING] UDS Handler 
4. [PENDING] VMG Client 
5. [PENDING] OTA Manager 

**DoIP + TLS  ?** [START]

