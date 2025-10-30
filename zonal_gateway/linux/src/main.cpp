/**
 * @file main.cpp
 * @brief Zonal Gateway Linux Main Application
 */

#include "zonal_gateway_linux.hpp"
#include <iostream>
#include <signal.h>

static vmg::ZonalGatewayLinux* g_zg = nullptr;

void signal_handler(int sig) {
    if (g_zg) {
        std::cout << "\n[MAIN] Received signal " << sig << ", shutting down..." << std::endl;
        g_zg->stop();
    }
    exit(0);
}

int main(int argc, char** argv) {
    /* Parse arguments */
    uint8_t zone_id = 1;
    std::string vmg_ip = "192.168.1.1";
    uint16_t vmg_port = 13400;
    
    if (argc >= 2) zone_id = std::stoi(argv[1]);
    if (argc >= 3) vmg_ip = argv[2];
    if (argc >= 4) vmg_port = std::stoi(argv[3]);
    
    std::cout << "╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Zonal Gateway (Linux x86)             ║" << std::endl;
    std::cout << "║  Zone ID: " << static_cast<int>(zone_id) << "                             ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    /* Create Zonal Gateway */
    vmg::ZonalGatewayLinux zg(zone_id, vmg_ip, vmg_port);
    g_zg = &zg;
    
    /* Register signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Start */
    if (!zg.start()) {
        std::cerr << "[MAIN] Failed to start Zonal Gateway" << std::endl;
        return -1;
    }
    
    std::cout << "[MAIN] Zonal Gateway running..." << std::endl;
    std::cout << "[MAIN] Press Ctrl+C to stop" << std::endl;
    
    /* Run */
    zg.run();
    
    /* Stop */
    zg.stop();
    
    return 0;
}

