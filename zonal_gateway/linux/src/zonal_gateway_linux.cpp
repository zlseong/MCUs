/**
 * @file zonal_gateway_linux.cpp
 * @brief Zonal Gateway Linux Implementation
 */

#include "zonal_gateway_linux.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

namespace vmg {

ZonalGatewayLinux::ZonalGatewayLinux(uint8_t zone_id, const std::string& vmg_ip, uint16_t vmg_port)
    : zone_id_(zone_id),
      vmg_ip_(vmg_ip),
      vmg_port_(vmg_port),
      state_(ZGState::INIT),
      running_(false),
      vmg_connected_(false),
      doip_server_tcp_socket_(-1),
      doip_server_udp_socket_(-1),
      json_server_socket_(-1),
      vmg_client_socket_(-1)
{
    std::ostringstream oss;
    oss << "ZG-" << std::setfill('0') << std::setw(3) << static_cast<int>(zone_id);
    zg_id_ = oss.str();
    
    logical_address_ = 0x0200 + zone_id; // 0x0201, 0x0202...
    
    zone_vci_.zone_id = zone_id;
}

ZonalGatewayLinux::~ZonalGatewayLinux() {
    stop();
}

bool ZonalGatewayLinux::start() {
    if (running_) return true;
    
    std::cout << "[ZG] Starting Zonal Gateway: " << zg_id_ << std::endl;
    
    /* Create server sockets */
    if (!createServerSockets()) {
        std::cerr << "[ZG] Failed to create server sockets" << std::endl;
        return false;
    }
    
    running_ = true;
    state_ = ZGState::READY;
    
    /* Start threads */
    server_thread_ = std::make_unique<std::thread>(&ZonalGatewayLinux::serverThreadFunc, this);
    discovery_thread_ = std::make_unique<std::thread>(&ZonalGatewayLinux::discoveryThreadFunc, this);
    client_thread_ = std::make_unique<std::thread>(&ZonalGatewayLinux::clientThreadFunc, this);
    
    std::cout << "[ZG] Zonal Gateway started" << std::endl;
    return true;
}

void ZonalGatewayLinux::stop() {
    if (!running_) return;
    
    std::cout << "[ZG] Stopping Zonal Gateway: " << zg_id_ << std::endl;
    
    running_ = false;
    
    /* Join threads */
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    if (discovery_thread_ && discovery_thread_->joinable()) {
        discovery_thread_->join();
    }
    if (client_thread_ && client_thread_->joinable()) {
        client_thread_->join();
    }
    
    closeServerSockets();
    
    state_ = ZGState::INIT;
    std::cout << "[ZG] Zonal Gateway stopped" << std::endl;
}

void ZonalGatewayLinux::run() {
    /* Main loop - just keep threads running */
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool ZonalGatewayLinux::createServerSockets() {
    /* TCP Socket for DoIP */
    doip_server_tcp_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (doip_server_tcp_socket_ < 0) {
        std::cerr << "[ZG] Failed to create TCP socket" << std::endl;
        return false;
    }
    
    int opt = 1;
    setsockopt(doip_server_tcp_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ZG_DOIP_SERVER_PORT);
    
    if (bind(doip_server_tcp_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[ZG] Failed to bind TCP socket to port " << ZG_DOIP_SERVER_PORT << std::endl;
        close(doip_server_tcp_socket_);
        return false;
    }
    
    if (listen(doip_server_tcp_socket_, 5) < 0) {
        std::cerr << "[ZG] Failed to listen on TCP socket" << std::endl;
        close(doip_server_tcp_socket_);
        return false;
    }
    
    /* UDP Socket for Vehicle Discovery */
    doip_server_udp_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (doip_server_udp_socket_ < 0) {
        std::cerr << "[ZG] Failed to create UDP socket" << std::endl;
        close(doip_server_tcp_socket_);
        return false;
    }
    
    setsockopt(doip_server_udp_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(ZG_DOIP_SERVER_PORT);
    
    if (bind(doip_server_udp_socket_, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        std::cerr << "[ZG] Failed to bind UDP socket to port " << ZG_DOIP_SERVER_PORT << std::endl;
        close(doip_server_tcp_socket_);
        close(doip_server_udp_socket_);
        return false;
    }
    
    std::cout << "[ZG] Server sockets created successfully" << std::endl;
    std::cout << "[ZG] DoIP Server: 0.0.0.0:" << ZG_DOIP_SERVER_PORT << " (TCP/UDP)" << std::endl;
    
    return true;
}

void ZonalGatewayLinux::closeServerSockets() {
    if (doip_server_tcp_socket_ >= 0) {
        close(doip_server_tcp_socket_);
        doip_server_tcp_socket_ = -1;
    }
    if (doip_server_udp_socket_ >= 0) {
        close(doip_server_udp_socket_);
        doip_server_udp_socket_ = -1;
    }
    if (json_server_socket_ >= 0) {
        close(json_server_socket_);
        json_server_socket_ = -1;
    }
}

void ZonalGatewayLinux::serverThreadFunc() {
    std::cout << "[ZG] Server thread started" << std::endl;
    
    while (running_) {
        /* Handle ECU connections */
        handleECUConnections();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "[ZG] Server thread stopped" << std::endl;
}

void ZonalGatewayLinux::clientThreadFunc() {
    std::cout << "[ZG] Client thread started" << std::endl;
    
    /* Connect to VMG */
    if (connectToVMG()) {
        std::cout << "[ZG] Connected to VMG: " << vmg_ip_ << ":" << vmg_port_ << std::endl;
        
        /* Send heartbeat periodically */
        while (running_) {
            if (vmg_connected_) {
                sendHeartbeatToVMG();
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    std::cout << "[ZG] Client thread stopped" << std::endl;
}

void ZonalGatewayLinux::discoveryThreadFunc() {
    std::cout << "[ZG] Discovery thread started" << std::endl;
    
    while (running_) {
        handleVehicleDiscovery();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    std::cout << "[ZG] Discovery thread stopped" << std::endl;
}

void ZonalGatewayLinux::handleECUConnections() {
    /* Accept connections from ECUs */
    /* TODO: Implement with select/poll for non-blocking */
}

void ZonalGatewayLinux::handleVehicleDiscovery() {
    /* Handle UDP vehicle discovery requests */
    /* TODO: Implement UDP receive and response */
}

bool ZonalGatewayLinux::connectToVMG() {
    std::cout << "[ZG] Connecting to VMG: " << vmg_ip_ << ":" << vmg_port_ << std::endl;
    
    vmg_client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (vmg_client_socket_ < 0) {
        std::cerr << "[ZG] Failed to create VMG client socket" << std::endl;
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(vmg_port_);
    
    if (inet_pton(AF_INET, vmg_ip_.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "[ZG] Invalid VMG IP address" << std::endl;
        close(vmg_client_socket_);
        return false;
    }
    
    if (connect(vmg_client_socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[ZG] Failed to connect to VMG" << std::endl;
        close(vmg_client_socket_);
        return false;
    }
    
    vmg_connected_ = true;
    return true;
}

bool ZonalGatewayLinux::sendHeartbeatToVMG() {
    if (!vmg_connected_) return false;
    
    /* Send Tester Present (0x3E 0x00) via DoIP */
    std::vector<uint8_t> uds_data = {0x3E, 0x00};
    /* TODO: Wrap in DoIP diagnostic message and send */
    
    return true;
}

bool ZonalGatewayLinux::sendZoneVCIToVMG() {
    if (!vmg_connected_) return false;
    
    std::lock_guard<std::mutex> lock(zone_vci_mutex_);
    
    /* Build Zone VCI JSON or binary message */
    /* TODO: Implement VCI serialization and send */
    
    return true;
}

bool ZonalGatewayLinux::updateECUInfo(const std::string& ecu_id, const ZoneECUInfo& info) {
    std::lock_guard<std::mutex> lock(zone_vci_mutex_);
    
    /* Find or add ECU */
    for (auto& ecu : zone_vci_.ecus) {
        if (ecu.ecu_id == ecu_id) {
            ecu = info;
            return true;
        }
    }
    
    /* Add new ECU */
    if (zone_vci_.ecus.size() < ZG_MAX_ECUS) {
        zone_vci_.ecus.push_back(info);
        return true;
    }
    
    return false;
}

bool ZonalGatewayLinux::checkOTAReadiness(const std::string& campaign_id) {
    std::lock_guard<std::mutex> lock(zone_vci_mutex_);
    
    if (zone_vci_.average_battery_level < 50) return false;
    if (zone_vci_.available_storage_mb < 100) return false;
    
    /* Check all ECUs online */
    for (const auto& ecu : zone_vci_.ecus) {
        if (!ecu.is_online) return false;
    }
    
    return true;
}

bool ZonalGatewayLinux::reportOTAProgress(uint8_t progress_percentage) {
    if (!vmg_connected_) return false;
    
    std::cout << "[ZG] OTA Progress: " << static_cast<int>(progress_percentage) << "%" << std::endl;
    /* TODO: Send to VMG */
    
    return true;
}

void ZonalGatewayLinux::printZoneVCI() const {
    std::lock_guard<std::mutex> lock(zone_vci_mutex_);
    
    std::cout << "\n┌─────────────────────────────────────────┐" << std::endl;
    std::cout << "│ Zone " << static_cast<int>(zone_vci_.zone_id) << " VCI Summary" << std::string(25, ' ') << "│" << std::endl;
    std::cout << "├─────────────────────────────────────────┤" << std::endl;
    std::cout << "│ ECU Count: " << zone_vci_.ecus.size() << std::string(29, ' ') << "│" << std::endl;
    std::cout << "├─────────────────────────────────────────┤" << std::endl;
    
    for (size_t i = 0; i < zone_vci_.ecus.size(); i++) {
        const auto& ecu = zone_vci_.ecus[i];
        std::cout << "│ ECU #" << (i + 1) << ": " << ecu.ecu_id << std::endl;
        std::cout << "│   Address: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << ecu.logical_address << std::dec << std::endl;
        std::cout << "│   FW Ver:  " << ecu.firmware_version << std::endl;
        std::cout << "│   OTA:     " << (ecu.ota_capable ? "YES" : "NO") << std::endl;
        std::cout << "│" << std::endl;
    }
    
    std::cout << "└─────────────────────────────────────────┘" << std::endl;
}

std::string ZonalGatewayLinux::getZoneName() const {
    return "Zone_" + std::to_string(zone_id_);
}

} // namespace vmg

