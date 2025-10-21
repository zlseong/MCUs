#include "device_simulator.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <random>
#include <iomanip>
#include <chrono>

namespace tc375 {

SimulatorConfig SimulatorConfig::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filepath);
    }

    json j;
    file >> j;

    SimulatorConfig config;
    config.device_id = j["device"]["id"].get<std::string>();
    config.device_type = j["device"]["type"].get<std::string>();
    config.gateway_host = j["gateway"]["host"].get<std::string>();
    config.gateway_port = j["gateway"]["port"].get<int>();
    config.use_tls = j["gateway"]["use_tls"].get<bool>();
    
    if (j.contains("gateway") && j["gateway"].contains("verify_peer")) {
        config.verify_peer = j["gateway"]["verify_peer"].get<bool>();
    } else {
        config.verify_peer = false;
    }
    
    if (j.contains("gateway") && j["gateway"].contains("ca_cert")) {
        config.ca_cert_path = j["gateway"]["ca_cert"].get<std::string>();
    }
    
    config.heartbeat_interval_sec = j.value("heartbeat_interval_sec", 10);
    config.sensor_update_interval_sec = j.value("sensor_update_interval_sec", 5);

    return config;
}

DeviceSimulator::DeviceSimulator(const SimulatorConfig& config)
    : config_(config)
    , running_(false)
    , temperature_(25.0f)
    , pressure_(101.3f)
    , voltage_(12.0f)
{
    client_ = std::make_unique<TlsClient>(config_.gateway_host, config_.gateway_port);
    client_->setVerifyPeer(config_.verify_peer);
    if (!config_.ca_cert_path.empty()) {
        client_->setCACertPath(config_.ca_cert_path);
    }
}

DeviceSimulator::~DeviceSimulator() {
    stop();
}

bool DeviceSimulator::start() {
    if (running_) {
        return true;
    }

    std::cout << "=== TC375 Device Simulator ===" << std::endl;
    std::cout << "Device ID: " << config_.device_id << std::endl;
    std::cout << "Type: " << config_.device_type << std::endl;
    std::cout << "Gateway: " << config_.gateway_host << ":" << config_.gateway_port << std::endl;
    std::cout << "===============================" << std::endl << std::endl;

    // Connect to gateway
    if (!client_->connect()) {
        std::cerr << "[Simulator] Failed to connect to gateway" << std::endl;
        return false;
    }

    running_ = true;

    // Start worker threads
    worker_thread_ = std::thread(&DeviceSimulator::workerLoop, this);
    sensor_thread_ = std::thread(&DeviceSimulator::sensorLoop, this);

    std::cout << "[Simulator] Started successfully" << std::endl;
    return true;
}

void DeviceSimulator::stop() {
    if (!running_) {
        return;
    }

    std::cout << "[Simulator] Stopping..." << std::endl;
    running_ = false;

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (sensor_thread_.joinable()) {
        sensor_thread_.join();
    }

    client_->disconnect();
    std::cout << "[Simulator] Stopped" << std::endl;
}

void DeviceSimulator::workerLoop() {
    auto last_heartbeat = std::chrono::steady_clock::now();
    auto last_sensor_data = std::chrono::steady_clock::now();

    while (running_) {
        auto now = std::chrono::steady_clock::now();

        // Send heartbeat
        auto heartbeat_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_heartbeat).count();
        if (heartbeat_elapsed >= config_.heartbeat_interval_sec) {
            sendHeartbeat();
            last_heartbeat = now;
        }

        // Send sensor data
        auto sensor_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_sensor_data).count();
        if (sensor_elapsed >= config_.sensor_update_interval_sec) {
            sendSensorData();
            last_sensor_data = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void DeviceSimulator::sensorLoop() {
    while (running_) {
        updateSensors();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void DeviceSimulator::sendHeartbeat() {
    auto msg = createHeartbeat(config_.device_id);
    std::string json_str = msg.toJSON();
    
    std::cout << "[Simulator] Sending heartbeat" << std::endl;
    client_->send(json_str + "\n");
}

void DeviceSimulator::sendSensorData() {
    json sensor_data = {
        {"temperature", temperature_},
        {"pressure", pressure_},
        {"voltage", voltage_}
    };

    auto msg = createSensorData(config_.device_id, sensor_data);
    std::string json_str = msg.toJSON();
    
    std::cout << "[Simulator] Sending sensor data: "
              << "T=" << temperature_ << "°C, "
              << "P=" << pressure_ << " kPa, "
              << "V=" << voltage_ << " V" << std::endl;
    
    client_->send(json_str + "\n");
}

void DeviceSimulator::updateSensors() {
    // Simulate sensor readings with random variations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> temp_dist(0.0f, 0.5f);
    std::normal_distribution<float> press_dist(0.0f, 0.2f);
    std::normal_distribution<float> volt_dist(0.0f, 0.1f);

    temperature_ += temp_dist(gen);
    pressure_ += press_dist(gen);
    voltage_ += volt_dist(gen);

    // Keep within realistic ranges
    temperature_ = std::max(15.0f, std::min(35.0f, temperature_));
    pressure_ = std::max(95.0f, std::min(105.0f, pressure_));
    voltage_ = std::max(11.0f, std::min(13.0f, voltage_));
}

std::string DeviceSimulator::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string DeviceSimulator::getStatusReport() const {
    std::stringstream ss;
    ss << "=== Simulator Status ===" << std::endl;
    ss << "Device: " << config_.device_id << std::endl;
    ss << "Running: " << (running_ ? "Yes" : "No") << std::endl;
    ss << "Connected: " << (client_->isConnected() ? "Yes" : "No") << std::endl;
    ss << "Temperature: " << temperature_ << " °C" << std::endl;
    ss << "Pressure: " << pressure_ << " kPa" << std::endl;
    ss << "Voltage: " << voltage_ << " V" << std::endl;
    return ss.str();
}

} // namespace tc375

