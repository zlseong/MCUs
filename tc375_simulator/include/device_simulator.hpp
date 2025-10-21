#pragma once

#include "tls_client.hpp"
#include "protocol.hpp"
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

namespace tc375 {

struct SimulatorConfig {
    std::string device_id;
    std::string device_type;
    std::string gateway_host;
    int gateway_port;
    bool use_tls;
    bool verify_peer;
    std::string ca_cert_path;
    int heartbeat_interval_sec;
    int sensor_update_interval_sec;

    static SimulatorConfig loadFromFile(const std::string& filepath);
};

class DeviceSimulator {
public:
    explicit DeviceSimulator(const SimulatorConfig& config);
    ~DeviceSimulator();

    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running_; }

    // Status
    std::string getStatusReport() const;

private:
    SimulatorConfig config_;
    std::unique_ptr<TlsClient> client_;
    
    std::atomic<bool> running_;
    std::thread worker_thread_;
    std::thread sensor_thread_;

    // Simulated sensor data
    float temperature_;
    float pressure_;
    float voltage_;

    void workerLoop();
    void sensorLoop();
    
    void sendHeartbeat();
    void sendSensorData();
    void updateSensors();
    
    std::string getCurrentTimestamp() const;
};

} // namespace tc375

