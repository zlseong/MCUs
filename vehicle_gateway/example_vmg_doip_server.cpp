/**
 * @file example_vmg_doip_server.cpp
 * @brief Example VMG DoIP Server
 * 
 * Demonstrates how to use the DoIP server for VMG (Vehicle Gateway).
 * This example mirrors the Python DoIPServer usage.
 */

#include "include/doip_server.hpp"
#include "include/uds_service_handler.hpp"
#include <iostream>
#include <signal.h>
#include <unistd.h>

using namespace vmg;

// Global server instance for signal handler
DoIPServer* g_server = nullptr;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
    }
}

int main() {
    std::cout << "=== VMG DoIP Server Example ===" << std::endl;
    std::cout << std::endl;

    // Configure server
    // NOTE: This example uses Plain DoIP (no TLS)
    // For mbedTLS version, use: example_vmg_doip_server_mbedtls
    DoIPServerConfig config;
    config.host = "0.0.0.0";
    config.port = 13400;
    config.vin = "WBADT43452G296403";
    config.logical_address = 0x0100;
    config.eid = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};  // MAC address
    config.gid = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};  // Group ID
    config.max_clients = 10;
    config.enable_tls = false;  // Plain DoIP (legacy)

    // Create server
    DoIPServer server(config);
    g_server = &server;

    // Create UDS handler
    UDSServiceHandler uds_handler;
    uds_handler.setVIN(config.vin);
    uds_handler.setECUSerialNumber("VMG_ECU_001");
    uds_handler.setSoftwareVersion("v1.2.3");
    uds_handler.setHardwareVersion("HW_REV_B");

    // Register custom DID handlers (optional)
    uds_handler.registerDIDReadHandler(0xF1A0, [](uint16_t did) -> std::vector<uint8_t> {
        std::string custom_data = "Custom Data";
        return std::vector<uint8_t>(custom_data.begin(), custom_data.end());
    });

    // Register UDS handler with DoIP server
    server.registerUDSHandler([&uds_handler](const std::vector<uint8_t>& request) {
        return uds_handler.processRequest(request);
    });

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Start server
    std::cout << "Starting DoIP server..." << std::endl;
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "Server is running. Press Ctrl+C to stop." << std::endl;
    std::cout << std::endl;
    std::cout << "Test with DoIP client:" << std::endl;
    std::cout << "  1. Vehicle identification (UDP broadcast to port 13400)" << std::endl;
    std::cout << "  2. TCP connect to " << config.host << ":" << config.port << std::endl;
    std::cout << "  3. Send routing activation request" << std::endl;
    std::cout << "  4. Send diagnostic messages (UDS)" << std::endl;
    std::cout << std::endl;

    // Print statistics periodically
    while (server.isRunning()) {
        sleep(10);
        
        size_t active_conns = server.getActiveConnections();
        uint64_t total_msgs = server.getTotalMessages();
        
        std::cout << "[Stats] Active connections: " << active_conns 
                  << ", Total messages: " << total_msgs << std::endl;
    }

    std::cout << "Server stopped." << std::endl;
    return 0;
}

