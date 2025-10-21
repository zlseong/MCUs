#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace tc375 {

using json = nlohmann::json;

// ============================================================================
// Permanent Configuration (Flash - Immutable)
// ============================================================================

struct PermanentConfig {
    // ECU Identity
    char     ecu_serial[32];      // "TC375-001-2025-10-21"
    uint8_t  mac_address[6];      // Hardware MAC
    char     hardware_version[16];// "TC375TP-v2.0"
    uint32_t manufacture_date;    // Unix timestamp
    
    // Vehicle Identity  
    char     vin[17];             // Vehicle Identification Number
    char     vehicle_model[32];   // "Genesis G80"
    uint16_t vehicle_year;        // 2025
    uint8_t  vehicle_type;        // 0=Sedan, 1=SUV, etc.
    
    // Calibration Data
    float    adc_calibration[8];
    float    temperature_offset;
    float    voltage_calibration;
    
    uint8_t  reserved[128];
    uint32_t crc32;
} __attribute__((packed));

// ============================================================================
// Device Configuration (EEPROM - Rewritable)
// ============================================================================

struct DeviceConfig {
    // Network Settings
    uint32_t ip_address;          // 192.168.1.100
    uint32_t subnet_mask;         // 255.255.255.0
    uint32_t gateway_ip;          // Gateway IP
    uint16_t gateway_port;        // 8765
    char     gateway_host[64];    // "gateway.example.com"
    
    // Protocol Settings
    uint8_t  tls_enabled;         // 0=No, 1=Yes
    uint8_t  tls_verify_peer;     // 0=No, 1=Yes
    uint16_t heartbeat_interval;  // seconds
    uint16_t sensor_interval;     // seconds
    
    // Feature Flags
    uint8_t  ota_enabled;
    uint8_t  diagnostics_enabled;
    uint8_t  log_level;           // 0-3
    uint8_t  can_bitrate;         // 0=500K, 1=1M
    
    // CAN Configuration
    uint32_t can_id_base;
    uint8_t  can_mode;            // 0=Normal, 1=Listen-only
    
    uint8_t  reserved[64];
    uint32_t crc32;
} __attribute__((packed));

// ============================================================================
// Device Information Manager
// ============================================================================

class DeviceInfo {
public:
    DeviceInfo();
    ~DeviceInfo() = default;

    // Load from storage (Flash/EEPROM simulation)
    bool loadPermanentConfig(const std::string& filepath = "config/permanent.json");
    bool loadDeviceConfig(const std::string& filepath = "config/device.json");
    bool saveDeviceConfig(const std::string& filepath = "config/device.json");

    // Permanent Info (Flash - Read Only)
    std::string getEcuSerial() const;
    std::string getMacAddress() const;
    std::string getHardwareVersion() const;
    std::string getVIN() const;
    std::string getVehicleModel() const;
    uint16_t getVehicleYear() const;
    
    // Device Config (EEPROM - Writable)
    std::string getIPAddress() const;
    uint16_t getGatewayPort() const;
    std::string getGatewayHost() const;
    bool isTLSEnabled() const;
    uint16_t getHeartbeatInterval() const;
    
    // Update config (UDS WriteDataByID)
    bool updateIPAddress(const std::string& ip);
    bool updateGatewayPort(uint16_t port);
    bool updateFeatureFlag(const std::string& flag, bool enabled);

    // JSON Serialization
    json getPermanentInfo() const;      // Flash 데이터만
    json getDeviceConfig() const;       // EEPROM 데이터만
    json getFullInfo() const;           // 전체 정보
    json getRegistrationMessage() const; // 등록용 (1회)
    json getStatusMessage() const;       // 상태 보고용 (주기)

private:
    PermanentConfig perm_config_;
    DeviceConfig dev_config_;
    
    uint32_t calculateCRC(const void* data, size_t size) const;
    bool verifyCRC(const void* data, size_t size, uint32_t expected_crc) const;
};

} // namespace tc375

