/**
 * DoIP Server with mbedTLS for VMG
 * TC375 communication (no PQC, just standard TLS 1.3)
 */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

extern "C" {
#include "mbedtls_doip.h"
}

// DoIP Protocol Constants (ISO 13400)
#define DOIP_PROTOCOL_VERSION 0x02
#define DOIP_HEADER_SIZE 8

// DoIP Payload Types
#define DOIP_ROUTING_ACTIVATION_REQ   0x0005
#define DOIP_ROUTING_ACTIVATION_RES   0x0006
#define DOIP_DIAGNOSTIC_MESSAGE       0x8001
#define DOIP_DIAGNOSTIC_ACK           0x8002
#define DOIP_DIAGNOSTIC_NACK          0x8003

class DoIPHandler {
private:
    mbedtls_ssl_context* ssl;
    uint16_t source_address;
    bool activated;
    
public:
    DoIPHandler(mbedtls_ssl_context* s) 
        : ssl(s), source_address(0), activated(false) {}
    
    void handle() {
        std::cout << "[DoIP] Client session started" << std::endl;
        
        unsigned char buffer[4096];
        
        while (true) {
            int n = mbedtls_ssl_read(ssl, buffer, sizeof(buffer));
            if (n <= 0) {
                if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                    std::cout << "[DoIP] Connection closed cleanly" << std::endl;
                } else {
                    std::cout << "[DoIP] Connection error: -0x" 
                             << std::hex << -n << std::dec << std::endl;
                }
                break;
            }
            
            if (n < DOIP_HEADER_SIZE) {
                std::cerr << "[DoIP] Invalid packet size" << std::endl;
                continue;
            }
            
            // Parse DoIP header
            uint8_t protocol_version = buffer[0];
            uint16_t payload_type = (buffer[2] << 8) | buffer[3];
            uint32_t payload_length = (buffer[4] << 24) | (buffer[5] << 16) |
                                     (buffer[6] << 8) | buffer[7];
            
            if (protocol_version != DOIP_PROTOCOL_VERSION) {
                std::cerr << "[DoIP] Unsupported protocol version" << std::endl;
                continue;
            }
            
            std::cout << "[DoIP] Received payload type: 0x" 
                     << std::hex << payload_type << std::dec << std::endl;
            
            switch (payload_type) {
                case DOIP_ROUTING_ACTIVATION_REQ:
                    handle_routing_activation(buffer + DOIP_HEADER_SIZE, payload_length);
                    break;
                    
                case DOIP_DIAGNOSTIC_MESSAGE:
                    handle_diagnostic_message(buffer + DOIP_HEADER_SIZE, payload_length);
                    break;
                    
                default:
                    std::cerr << "[DoIP] Unknown payload type" << std::endl;
                    break;
            }
        }
    }
    
private:
    void handle_routing_activation(const uint8_t* payload, uint32_t len) {
        if (len < 7) return;
        
        source_address = (payload[0] << 8) | payload[1];
        
        std::cout << "[DoIP] Routing activation from 0x" 
                 << std::hex << source_address << std::dec << std::endl;
        
        // Send response
        unsigned char response[256];
        int pos = 0;
        
        // DoIP header
        response[pos++] = DOIP_PROTOCOL_VERSION;
        response[pos++] = ~DOIP_PROTOCOL_VERSION;
        response[pos++] = (DOIP_ROUTING_ACTIVATION_RES >> 8) & 0xFF;
        response[pos++] = DOIP_ROUTING_ACTIVATION_RES & 0xFF;
        
        uint32_t payload_len = 9;
        response[pos++] = (payload_len >> 24) & 0xFF;
        response[pos++] = (payload_len >> 16) & 0xFF;
        response[pos++] = (payload_len >> 8) & 0xFF;
        response[pos++] = payload_len & 0xFF;
        
        // Payload
        response[pos++] = (source_address >> 8) & 0xFF;
        response[pos++] = source_address & 0xFF;
        response[pos++] = 0x00; // VMG address high
        response[pos++] = 0x01; // VMG address low
        response[pos++] = 0x10; // Success
        response[pos++] = 0x00; response[pos++] = 0x00;
        response[pos++] = 0x00; response[pos++] = 0x00;
        
        mbedtls_ssl_write(ssl, response, pos);
        activated = true;
        
        std::cout << "[DoIP] Routing activated" << std::endl;
    }
    
    void handle_diagnostic_message(const uint8_t* payload, uint32_t len) {
        if (!activated) {
            std::cerr << "[DoIP] Routing not activated" << std::endl;
            return;
        }
        
        if (len < 5) return;
        
        uint16_t target_addr = (payload[2] << 8) | payload[3];
        const uint8_t* diag_data = payload + 4;
        size_t diag_len = len - 4;
        
        std::cout << "[DoIP] Diagnostic message for 0x" 
                 << std::hex << target_addr << std::dec
                 << ", " << diag_len << " bytes" << std::endl;
        
        // Echo response
        unsigned char response[4096];
        int pos = 0;
        
        // DoIP header
        response[pos++] = DOIP_PROTOCOL_VERSION;
        response[pos++] = ~DOIP_PROTOCOL_VERSION;
        response[pos++] = (DOIP_DIAGNOSTIC_MESSAGE >> 8) & 0xFF;
        response[pos++] = DOIP_DIAGNOSTIC_MESSAGE & 0xFF;
        
        uint32_t payload_len = 4 + diag_len;
        response[pos++] = (payload_len >> 24) & 0xFF;
        response[pos++] = (payload_len >> 16) & 0xFF;
        response[pos++] = (payload_len >> 8) & 0xFF;
        response[pos++] = payload_len & 0xFF;
        
        // Payload (swap source/target)
        response[pos++] = payload[2]; response[pos++] = payload[3];
        response[pos++] = payload[0]; response[pos++] = payload[1];
        memcpy(response + pos, diag_data, diag_len);
        pos += diag_len;
        
        mbedtls_ssl_write(ssl, response, pos);
    }
};

std::atomic<bool> running(true);

void signal_handler(int sig) {
    running = false;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] 
                  << " <cert> <key> <ca> <port>" << std::endl;
        std::cerr << "Example: " << argv[0]
                  << " certs/vmg_server.crt "
                  << " certs/vmg_server.key "
                  << "certs/ca.crt 13400" << std::endl;
        return 1;
    }
    
    std::string cert = argv[1];
    std::string key = argv[2];
    std::string ca = argv[3];
    uint16_t port = atoi(argv[4]);
    
    std::cout << "========================================" << std::endl;
    std::cout << "VMG DoIP Server with mbedTLS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Protocol: TLS 1.3 (Standard, no PQC)" << std::endl;
    std::cout << "Auth: Mutual TLS" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "========================================" << std::endl;
    
    mbedtls_doip_server server;
    if (mbedtls_doip_server_init(&server, cert.c_str(), key.c_str(), 
                                 ca.c_str(), port) != 0) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::vector<std::thread> threads;
    
    std::cout << "\n[VMG] Ready to accept TC375 clients..." << std::endl;
    
    while (running) {
        mbedtls_ssl_context client_ssl;
        
        if (mbedtls_doip_server_accept(&server, &client_ssl) == 0) {
            threads.emplace_back([client_ssl]() mutable {
                DoIPHandler handler(&client_ssl);
                handler.handle();
                mbedtls_ssl_free(&client_ssl);
            });
        }
    }
    
    std::cout << "\n[VMG] Shutting down..." << std::endl;
    
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    
    mbedtls_doip_server_free(&server);
    
    return 0;
}

