/**
 * TC375 PQC DoIP Client Test
 */

#include "pqc_doip_client.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <vmg_host> <vmg_port> <cert> <key> <ca>" << std::endl;
        std::cerr << "Example: " << argv[0]
                  << " 192.168.1.1 13400 "
                  << "certs/mlkem768_mldsa65_client.crt "
                  << "certs/mlkem768_mldsa65_client.key "
                  << "certs/ca.crt" << std::endl;
        return 1;
    }
    
    std::string vmg_host = argv[1];
    uint16_t vmg_port = atoi(argv[2]);
    std::string cert = argv[3];
    std::string key = argv[4];
    std::string ca = argv[5];
    
    std::cout << "========================================" << std::endl;
    std::cout << "TC375 DoIP Client with PQC" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "VMG: " << vmg_host << ":" << vmg_port << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Create client with recommended PQC config
    PQC_DoIP_Client client(vmg_host, vmg_port, 0x0E80,
                          PQC_KEM::MLKEM768, PQC_SIG::MLDSA65);
    
    // Connect to VMG
    if (!client.connect(cert, key, ca)) {
        std::cerr << "Failed to connect to VMG" << std::endl;
        return 1;
    }
    
    // Send routing activation
    if (!client.send_routing_activation()) {
        std::cerr << "Failed to activate routing" << std::endl;
        return 1;
    }
    
    // Send diagnostic messages
    std::cout << "\n[TC375] Sending diagnostic messages..." << std::endl;
    
    for (int i = 0; i < 5; i++) {
        std::vector<uint8_t> data = {0x10, 0x01}; // DiagnosticSessionControl
        
        if (client.send_diagnostic_message(0x0001, data)) {
            std::cout << "[" << i + 1 << "/5] Sent diagnostic message" << std::endl;
            
            // Receive response
            auto response = client.receive_diagnostic_message();
            if (!response.empty()) {
                std::cout << "  Response (" << response.size() << " bytes): ";
                for (auto b : response) {
                    printf("%02X ", b);
                }
                std::cout << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\n[TC375] Disconnecting..." << std::endl;
    client.disconnect();
    
    return 0;
}

