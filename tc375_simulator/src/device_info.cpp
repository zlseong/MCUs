#include "device_info.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <iostream>

namespace tc375 {

DeviceInfo::DeviceInfo() {
    // Initialize to default values
    memset(&perm_config_, 0, sizeof(perm_config_));
    memset(&dev_config_, 0, sizeof(dev_config_));
}

bool DeviceInfo::loadPermanentConfig(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[DeviceInfo] Permanent config not found, using defaults" << std::endl;
        
        // Default values (would be in Flash on real TC375)
        strncpy(perm_config_.ecu_serial, "TC375-SIM-001", sizeof(perm_config_.ecu_serial));
        perm_config_.mac_address[0] = 0x02;
        perm_config_.mac_address[1] = 0x00;
        perm_config_.mac_address[2] = 0x00;
        perm_config_.mac_address[3] = 0xAA;
        perm_config_.mac_address[4] = 0xBB;
        perm_config_.mac_address[5] = 0xCC;
        strncpy(perm_config_.hardware_version, "TC375TP-v2.0", sizeof(perm_config_.hardware_version));
        strncpy(perm_config_.vin, "KMHGH4JH1NU123456", sizeof(perm_config_.vin));
        strncpy(perm_config_.vehicle_model, "Genesis G80", sizeof(perm_config_.vehicle_model));
        perm_config_.vehicle_year = 2025;
        
        return true;
    }

    json j;
    file >> j;

    // Parse JSON
    std::string serial = j["ecu_serial"].get<std::string>();
    strncpy(perm_config_.ecu_serial, serial.c_str(), sizeof(perm_config_.ecu_serial));
    
    std::string mac = j["mac_address"].get<std::string>();
    sscanf(mac.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
           &perm_config_.mac_address[0], &perm_config_.mac_address[1],
           &perm_config_.mac_address[2], &perm_config_.mac_address[3],
           &perm_config_.mac_address[4], &perm_config_.mac_address[5]);
    
    std::string vin = j["vin"].get<std::string>();
    strncpy(perm_config_.vin, vin.c_str(), sizeof(perm_config_.vin));
    
    std::cout << "[DeviceInfo] Permanent config loaded" << std::endl;
    return true;
}

bool DeviceInfo::loadDeviceConfig(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        // Default values
        dev_config_.gateway_port = 8765;
        strncpy(dev_config_.gateway_host, "localhost", sizeof(dev_config_.gateway_host));
        dev_config_.tls_enabled = 1;
        dev_config_.ota_enabled = 1;
        dev_config_.heartbeat_interval = 10;
        dev_config_.sensor_interval = 5;
        return true;
    }

    json j;
    file >> j;

    // Parse network settings
    std::string ip = j.value("ip_address", "192.168.1.100");
    unsigned int a, b, c, d;
    sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d);
    dev_config_.ip_address = (a << 24) | (b << 16) | (c << 8) | d;
    
    dev_config_.gateway_port = j.value("gateway_port", 8765);
    
    std::string host = j.value("gateway_host", "localhost");
    strncpy(dev_config_.gateway_host, host.c_str(), sizeof(dev_config_.gateway_host));
    
    dev_config_.tls_enabled = j.value("tls_enabled", true) ? 1 : 0;
    dev_config_.ota_enabled = j.value("ota_enabled", true) ? 1 : 0;
    dev_config_.heartbeat_interval = j.value("heartbeat_interval", 10);
    dev_config_.sensor_interval = j.value("sensor_interval", 5);
    
    std::cout << "[DeviceInfo] Device config loaded" << std::endl;
    return true;
}

bool DeviceInfo::saveDeviceConfig(const std::string& filepath) {
    json j = getDeviceConfig();
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    file << j.dump(2);
    std::cout << "[DeviceInfo] Device config saved" << std::endl;
    return true;
}

std::string DeviceInfo::getEcuSerial() const {
    return std::string(perm_config_.ecu_serial);
}

std::string DeviceInfo::getMacAddress() const {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             perm_config_.mac_address[0],
             perm_config_.mac_address[1],
             perm_config_.mac_address[2],
             perm_config_.mac_address[3],
             perm_config_.mac_address[4],
             perm_config_.mac_address[5]);
    return std::string(buf);
}

std::string DeviceInfo::getHardwareVersion() const {
    return std::string(perm_config_.hardware_version);
}

std::string DeviceInfo::getVIN() const {
    return std::string(perm_config_.vin);
}

std::string DeviceInfo::getVehicleModel() const {
    return std::string(perm_config_.vehicle_model);
}

uint16_t DeviceInfo::getVehicleYear() const {
    return perm_config_.vehicle_year;
}

std::string DeviceInfo::getIPAddress() const {
    uint32_t ip = dev_config_.ip_address;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF,
             ip & 0xFF);
    return std::string(buf);
}

uint16_t DeviceInfo::getGatewayPort() const {
    return dev_config_.gateway_port;
}

std::string DeviceInfo::getGatewayHost() const {
    return std::string(dev_config_.gateway_host);
}

bool DeviceInfo::isTLSEnabled() const {
    return dev_config_.tls_enabled != 0;
}

uint16_t DeviceInfo::getHeartbeatInterval() const {
    return dev_config_.heartbeat_interval;
}

bool DeviceInfo::updateIPAddress(const std::string& ip) {
    unsigned int a, b, c, d;
    if (sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return false;
    }
    
    dev_config_.ip_address = (a << 24) | (b << 16) | (c << 8) | d;
    dev_config_.crc32 = calculateCRC(&dev_config_, sizeof(dev_config_) - 4);
    
    std::cout << "[DeviceInfo] IP updated to: " << ip << std::endl;
    return true;
}

bool DeviceInfo::updateGatewayPort(uint16_t port) {
    dev_config_.gateway_port = port;
    dev_config_.crc32 = calculateCRC(&dev_config_, sizeof(dev_config_) - 4);
    
    std::cout << "[DeviceInfo] Gateway port updated to: " << port << std::endl;
    return true;
}

bool DeviceInfo::updateFeatureFlag(const std::string& flag, bool enabled) {
    if (flag == "ota") {
        dev_config_.ota_enabled = enabled ? 1 : 0;
    } else if (flag == "diagnostics") {
        dev_config_.diagnostics_enabled = enabled ? 1 : 0;
    } else if (flag == "tls") {
        dev_config_.tls_enabled = enabled ? 1 : 0;
    } else {
        return false;
    }
    
    dev_config_.crc32 = calculateCRC(&dev_config_, sizeof(dev_config_) - 4);
    std::cout << "[DeviceInfo] Feature '" << flag << "' set to: " 
              << (enabled ? "enabled" : "disabled") << std::endl;
    return true;
}

json DeviceInfo::getPermanentInfo() const {
    return {
        {"ecu_serial", getEcuSerial()},
        {"mac_address", getMacAddress()},
        {"hardware_version", getHardwareVersion()},
        {"vin", getVIN()},
        {"vehicle_model", getVehicleModel()},
        {"vehicle_year", getVehicleYear()}
    };
}

json DeviceInfo::getDeviceConfig() const {
    return {
        {"ip_address", getIPAddress()},
        {"gateway_host", getGatewayHost()},
        {"gateway_port", getGatewayPort()},
        {"tls_enabled", isTLSEnabled()},
        {"ota_enabled", dev_config_.ota_enabled != 0},
        {"heartbeat_interval", getHeartbeatInterval()},
        {"sensor_interval", dev_config_.sensor_interval}
    };
}

json DeviceInfo::getFullInfo() const {
    return {
        {"permanent", getPermanentInfo()},
        {"config", getDeviceConfig()}
    };
}

json DeviceInfo::getRegistrationMessage() const {
    // 최초 등록 시 전송 (모든 정보 포함)
    return {
        {"type", "DEVICE_REGISTRATION"},
        {"device", {
            // Permanent (Flash)
            {"ecu_serial", getEcuSerial()},
            {"mac_address", getMacAddress()},
            {"hardware_version", getHardwareVersion()},
            {"vin", getVIN()},
            {"vehicle_model", getVehicleModel()},
            {"vehicle_year", getVehicleYear()},
            
            // Config (EEPROM)
            {"ip_address", getIPAddress()},
            {"gateway_port", getGatewayPort()},
            {"tls_enabled", isTLSEnabled()},
            {"ota_enabled", dev_config_.ota_enabled != 0}
        }},
        {"timestamp", getCurrentTimestamp()}
    };
}

json DeviceInfo::getStatusMessage() const {
    // 주기적 상태 보고 (식별자 + 동적 데이터만)
    return {
        {"type", "STATUS_REPORT"},
        {"device_id", getEcuSerial()},  // 식별자만
        {"payload", {
            // 동적 데이터는 Device Simulator에서 추가
            {"connected", true}
        }},
        {"timestamp", getCurrentTimestamp()}
    };
}

uint32_t DeviceInfo::calculateCRC(const void* data, size_t size) const {
    // Simplified CRC32
    // In production: use hardware CRC or optimized algorithm
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

bool DeviceInfo::verifyCRC(const void* data, size_t size, uint32_t expected_crc) const {
    uint32_t calc = calculateCRC(data, size);
    return calc == expected_crc;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace tc375

