/**
 * VMG Gateway Main
 * Integrates DoIP Server, HTTPS Client, MQTT Client
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

extern "C" {
#include "pqc_config.h"
}

// Forward declarations for servers
extern int pqc_doip_server_example(uint16_t port);

std::atomic<bool> running(true);

void signal_handler(int sig) {
    std::cout << "\n[VMG] Received signal " << sig << ", shutting down..." << std::endl;
    running = false;
}

void print_banner() {
    std::cout << R"(
╔══════════════════════════════════════════════════╗
║     Vehicle Management Gateway (VMG)             ║
║     Pure PQC TLS | DoIP | HTTPS | MQTT           ║
╚══════════════════════════════════════════════════╝
)" << std::endl;
}

int main(int argc, char** argv) {
    print_banner();
    
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Check OpenSSL version
    std::cout << "[VMG] OpenSSL version: " << OpenSSL_version(OPENSSL_VERSION) << std::endl;
    
    if (OPENSSL_VERSION_NUMBER < 0x30000000L) {
        std::cerr << "[VMG] Warning: OpenSSL 3.0+ required for PQC" << std::endl;
    }
    
    // Display available PQC configurations
    std::cout << "\n[VMG] Available PQC configurations:" << std::endl;
    std::cout << "===========================================" << std::endl;
    for (size_t i = 0; i < PQC_CONFIG_COUNT; i++) {
        std::cout << "[" << i << "] "
                 << PQC_CONFIGS[i].kem_name << " + "
                 << PQC_CONFIGS[i].sig_name << std::endl;
    }
    std::cout << "===========================================" << std::endl;
    std::cout << "\nRecommended: [5] ML-KEM-768 + ML-DSA-65" << std::endl;
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "\n[VMG] Gateway initialized" << std::endl;
    std::cout << "[VMG] Services:" << std::endl;
    std::cout << "  - DoIP Server:  Port 13400 (TC375 clients)" << std::endl;
    std::cout << "  - HTTPS Client: External OTA/API" << std::endl;
    std::cout << "  - MQTT Client:  Telemetry/Commands" << std::endl;
    std::cout << "\n[VMG] Press Ctrl+C to exit" << std::endl;
    
    // For now, just keep running
    // In production, this would spawn DoIP server, HTTPS poller, MQTT client threads
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "[VMG] Cleanup complete" << std::endl;
    
    return 0;
}

