/**
 * @file example_vmg_doip_server_mbedtls.cpp
 * @brief VMG DoIP Server with mbedTLS
 * 
 * Uses mbedTLS (Standard TLS 1.3) for in-vehicle communication
 * NO PQC - optimized for TC375 MCU clients
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

extern "C" {
#include "mbedtls_doip.h"
}

std::atomic<bool> running(true);

void signal_handler(int sig) {
    std::cout << "\n[VMG] Received signal " << sig << ", shutting down..." << std::endl;
    running = false;
}

void print_banner() {
    std::cout << R"(
╔══════════════════════════════════════════════════╗
║     Vehicle Management Gateway (VMG)             ║
║     DoIP Server with mbedTLS                     ║
╚══════════════════════════════════════════════════╝
)" << std::endl;
}

int main(int argc, char** argv) {
    print_banner();
    
    // Default paths
    const char* cert_file = "certs/vmg_server.crt";
    const char* key_file = "certs/vmg_server.key";
    const char* ca_file = "certs/ca.crt";
    uint16_t port = 13400;
    
    // Parse command line arguments
    if (argc >= 4) {
        cert_file = argv[1];
        key_file = argv[2];
        ca_file = argv[3];
    }
    if (argc >= 5) {
        port = (uint16_t)atoi(argv[4]);
    }
    
    std::cout << "[VMG] Configuration:" << std::endl;
    std::cout << "  Certificate: " << cert_file << std::endl;
    std::cout << "  Private Key: " << key_file << std::endl;
    std::cout << "  CA Cert:     " << ca_file << std::endl;
    std::cout << "  Port:        " << port << std::endl;
    std::cout << "  TLS:         mbedTLS (Standard TLS 1.3)" << std::endl;
    std::cout << std::endl;
    
    // Initialize mbedTLS DoIP server
    mbedtls_doip_server server;
    if (mbedtls_doip_server_init(&server, cert_file, key_file, ca_file, port) != 0) {
        std::cerr << "[ERROR] Failed to initialize mbedTLS DoIP server" << std::endl;
        return 1;
    }
    
    std::cout << "[VMG] DoIP Server started on port " << port << std::endl;
    std::cout << "[VMG] Waiting for TC375 clients..." << std::endl;
    std::cout << std::endl;
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Accept loop
    while (running) {
        std::cout << "[VMG] Waiting for client connection..." << std::endl;
        
        mbedtls_ssl_context client_ssl;
        mbedtls_ssl_init(&client_ssl);
        
        int ret = mbedtls_doip_server_accept(&server, &client_ssl);
        if (ret != 0) {
            if (running) {
                std::cerr << "[ERROR] Accept failed: -0x" << std::hex << -ret << std::dec << std::endl;
            }
            mbedtls_ssl_free(&client_ssl);
            continue;
        }
        
        std::cout << "[VMG] Client connected with TLS" << std::endl;
        std::cout << "[VMG] Cipher suite: " << mbedtls_ssl_get_ciphersuite(&client_ssl) << std::endl;
        std::cout << "[VMG] Protocol version: " << mbedtls_ssl_get_version(&client_ssl) << std::endl;
        
        // Handle client in new thread
        std::thread([](mbedtls_ssl_context* ssl) {
            unsigned char buffer[4096];
            
            while (true) {
                int n = mbedtls_ssl_read(ssl, buffer, sizeof(buffer));
                
                if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                    std::cout << "[VMG] Client closed connection" << std::endl;
                    break;
                }
                
                if (n < 0) {
                    std::cerr << "[VMG] Read error: -0x" << std::hex << -n << std::dec << std::endl;
                    break;
                }
                
                if (n == 0) {
                    std::cout << "[VMG] Connection closed" << std::endl;
                    break;
                }
                
                std::cout << "[VMG] Received " << n << " bytes (DoIP message)" << std::endl;
                
                // Echo back (for testing)
                mbedtls_ssl_write(ssl, buffer, n);
            }
            
            mbedtls_ssl_close_notify(ssl);
            mbedtls_ssl_free(ssl);
            delete ssl;
            
        }, &client_ssl).detach();
    }
    
    std::cout << "[VMG] Shutting down..." << std::endl;
    mbedtls_doip_server_free(&server);
    std::cout << "[VMG] Server stopped" << std::endl;
    
    return 0;
}

