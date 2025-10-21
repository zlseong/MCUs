#include "device_simulator.hpp"
#include <iostream>
#include <csignal>
#include <string>

using namespace tc375;

// Global simulator instance for signal handling
DeviceSimulator* g_simulator = nullptr;

void signalHandler(int signal) {
    std::cout << "\n[Main] Received signal " << signal << ", shutting down..." << std::endl;
    if (g_simulator) {
        g_simulator->stop();
    }
    exit(0);
}

void printUsage() {
    std::cout << "\n=== TC375 Device Simulator ===" << std::endl;
    std::cout << "Usage: ./tc375_simulator [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  -c, --config <file>   Configuration file (default: tc375_simulator/config/device.json)" << std::endl;
    std::cout << "  -h, --help            Show this help message" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== TC375 Device Simulator v1.0 ===" << std::endl;
    std::cout << "Simulating TC375 Lite Kit device" << std::endl;
    std::cout << "Connecting to Vehicle Gateway via TLS" << std::endl;
    std::cout << std::endl;

    // Parse command line arguments
    std::string config_file = "tc375_simulator/config/device.json";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: -c/--config requires a file path" << std::endl;
                return 1;
            }
        }
    }

    try {
        // Load configuration
        std::cout << "[Main] Loading configuration from: " << config_file << std::endl;
        auto config = SimulatorConfig::loadFromFile(config_file);
        
        // Create simulator
        std::cout << "[Main] Creating simulator instance..." << std::endl;
        DeviceSimulator simulator(config);
        g_simulator = &simulator;
        
        // Set up signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Start simulator
        std::cout << "[Main] Starting simulator..." << std::endl;
        if (!simulator.start()) {
            std::cerr << "[Main] Failed to start simulator" << std::endl;
            return 1;
        }
        
        // Main loop
        std::cout << "\n" << simulator.getStatusReport() << std::endl;
        std::cout << "\nPress Ctrl+C to stop the simulator..." << std::endl << std::endl;
        
        while (simulator.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Clean shutdown
        std::cout << "[Main] Stopping simulator..." << std::endl;
        simulator.stop();
        
        std::cout << "[Main] Simulator shutdown complete" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

