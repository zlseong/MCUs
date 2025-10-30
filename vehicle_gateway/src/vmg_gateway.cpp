/**
 * VMG Gateway Main
 * Integrates DoIP Server, HTTPS Client, MQTT Client
 * 
 * PQC is ONLY used for VMG <-> External Server communication
 * VMG <-> ZG <-> ECU uses plain DoIP (no PQC overhead)
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

extern "C" {
#include "pqc_config.h"
}

// ============================================================================
// PQC Configuration for External Server Communication
// Change this number to test different PQC parameters (0-5)
// ============================================================================
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  1   // Default: ML-KEM-768 + ECDSA-P256

// Available configurations:
// [0] ML-KEM-512  + ECDSA-P256  (fastest, 128-bit)
// [1] ML-KEM-768  + ECDSA-P256  (recommended, 192-bit) <- DEFAULT
// [2] ML-KEM-1024 + ECDSA-P256  (highest security, 256-bit)
// [3] ML-KEM-512  + ML-DSA-44   (pure PQC, 128-bit)
// [4] ML-KEM-768  + ML-DSA-65   (pure PQC, 192-bit)
// [5] ML-KEM-1024 + ML-DSA-87   (pure PQC, 256-bit)

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
║     DoIP | HTTPS | MQTT                          ║
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
    
    // Get PQC configuration for external server
    const PQC_Config* pqc_cfg = &PQC_CONFIGS[PQC_CONFIG_ID_FOR_EXTERNAL_SERVER];
    
    std::cout << "\n[VMG] Network Architecture:" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << "  External Server <--PQC-TLS--> VMG" << std::endl;
    std::cout << "       VMG <--Plain DoIP--> Zonal Gateway" << std::endl;
    std::cout << "           Zonal Gateway <--Plain DoIP--> ECU" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    std::cout << "\n[VMG] PQC Configuration (External Server only):" << std::endl;
    std::cout << "  KEM:       " << pqc_cfg->kem_name << std::endl;
    std::cout << "  Signature: " << pqc_cfg->sig_name << std::endl;
    std::cout << "  Config ID: " << PQC_CONFIG_ID_FOR_EXTERNAL_SERVER << std::endl;
    std::cout << "\n  To change: Edit PQC_CONFIG_ID_FOR_EXTERNAL_SERVER in vmg_gateway.cpp" << std::endl;
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "\n[VMG] Gateway initialized" << std::endl;
    std::cout << "[VMG] Services:" << std::endl;
    std::cout << "  - DoIP Server:  Port 13400 (ZG/ECU clients, NO PQC)" << std::endl;
    std::cout << "  - HTTPS Client: External OTA/API (WITH PQC)" << std::endl;
    std::cout << "  - MQTT Client:  Telemetry/Commands (WITH PQC)" << std::endl;
    std::cout << "\n[VMG] Press Ctrl+C to exit" << std::endl;
    
    // For now, just keep running
    // In production, this would spawn DoIP server, HTTPS poller, MQTT client threads
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "[VMG] Cleanup complete" << std::endl;
    
    return 0;
}

