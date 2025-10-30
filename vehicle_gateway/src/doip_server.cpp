/**
 * DoIP Server with PQC for VMG
 * Central gateway for TC375 ECUs
 */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

extern "C" {
#include "pqc_config.h"

typedef struct PQC_Server PQC_Server;
PQC_Server* pqc_server_create(uint16_t port, const PQC_Config* config,
                              const char* cert_file,
                              const char* key_file,
                              const char* ca_file);
int pqc_server_accept(PQC_Server* server, SSL** out_ssl, int* out_fd);
void pqc_server_destroy(PQC_Server* server);
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
    SSL* ssl;
    int fd;
    uint16_t source_address;
    bool activated;
    
public:
    DoIPHandler(SSL* s, int f) 
        : ssl(s), fd(f), source_address(0), activated(false) {}
    
    ~DoIPHandler() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        if (fd >= 0) {
            close(fd);
        }
    }
    
    void handle() {
        std::cout << "[DoIP] Client connected" << std::endl;
        
        uint8_t buffer[4096];
        
        while (true) {
            int n = SSL_read(ssl, buffer, sizeof(buffer));
            if (n <= 0) {
                std::cout << "[DoIP] Connection closed" << std::endl;
                break;
            }
            
            if (n < DOIP_HEADER_SIZE) {
                std::cerr << "[DoIP] Invalid packet size" << std::endl;
                continue;
            }
            
            // Parse DoIP header
            uint8_t protocol_version = buffer[0];
            uint8_t inverse_version = buffer[1];
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
        uint8_t activation_type = payload[2];
        
        std::cout << "[DoIP] Routing activation from 0x" 
                 << std::hex << source_address << std::dec << std::endl;
        
        // Send response
        uint8_t response[256];
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
        
        SSL_write(ssl, response, pos);
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
        
        // Echo response for testing
        uint8_t response[4096];
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
        
        SSL_write(ssl, response, pos);
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
                  << " certs/mlkem768_mldsa65_server.crt "
                  << "certs/mlkem768_mldsa65_server.key "
                  << "certs/ca.crt 13400" << std::endl;
        return 1;
    }
    
    std::string cert = argv[1];
    std::string key = argv[2];
    std::string ca = argv[3];
    uint16_t port = atoi(argv[4]);
    
    // Use recommended PQC config
    const PQC_Config* config = &PQC_CONFIGS[1]; // mlkem768 + mldsa65
    
    std::cout << "========================================" << std::endl;
    std::cout << "VMG DoIP Server with PQC" << std::endl;
    std::cout << "========================================" << std::endl;
    pqc_print_config(config);
    std::cout << "Port: " << port << std::endl;
    std::cout << "========================================" << std::endl;
    
    PQC_Server* server = pqc_server_create(port, config,
                                           cert.c_str(), key.c_str(), ca.c_str());
    
    if (!server) {
        std::cerr << "Failed to create server" << std::endl;
        return 1;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::vector<std::thread> threads;
    
    std::cout << "\n[VMG] Ready to accept TC375 clients..." << std::endl;
    
    while (running) {
        SSL* ssl = nullptr;
        int fd = 0;
        
        if (pqc_server_accept(server, &ssl, &fd)) {
            threads.emplace_back([ssl, fd]() {
                DoIPHandler handler(ssl, fd);
                handler.handle();
            });
        }
    }
    
    std::cout << "\n[VMG] Shutting down..." << std::endl;
    
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    
    pqc_server_destroy(server);
    
    return 0;
}

