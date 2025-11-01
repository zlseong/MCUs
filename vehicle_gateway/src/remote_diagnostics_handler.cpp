/**
 * @file remote_diagnostics_handler.cpp
 * @brief Remote Diagnostics Handler Implementation
 */

#include "remote_diagnostics_handler.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

// JSON parsing (simple implementation, use nlohmann/json in production)
#include <regex>

namespace vmg {

RemoteDiagnosticsHandler::RemoteDiagnosticsHandler()
    : mqtt_client_(nullptr)
    , doip_client_(nullptr)
    , total_requests_(0)
    , successful_requests_(0)
    , failed_requests_(0)
    , timeout_requests_(0)
    , retry_requests_(0)
{
}

RemoteDiagnosticsHandler::~RemoteDiagnosticsHandler() {
}

bool RemoteDiagnosticsHandler::initialize(void* mqtt_client, void* doip_client) {
    mqtt_client_ = mqtt_client;
    doip_client_ = doip_client;
    
    std::cout << "[RemoteDiag] Handler initialized" << std::endl;
    return true;
}

bool RemoteDiagnosticsHandler::handleRequest(const std::string& json_payload) {
    DiagnosticRequest request;
    
    if (!parseRequest(json_payload, request)) {
        std::cerr << "[RemoteDiag] Failed to parse request" << std::endl;
        return false;
    }
    
    std::cout << "[RemoteDiag] Received request: " << request.request_id << std::endl;
    std::cout << "  ECU: " << request.ecu_id << std::endl;
    std::cout << "  Service: 0x" << std::hex << static_cast<int>(request.service_id) << std::dec << std::endl;
    
    total_requests_++;
    
    // Store as pending
    request.timestamp = std::chrono::steady_clock::now();
    pending_requests_[request.request_id] = request;
    
    // Send to ECU
    if (!sendToECU(request)) {
        std::cerr << "[RemoteDiag] Failed to send to ECU" << std::endl;
        
        // Send error response
        DiagnosticResponse response;
        response.request_id = request.request_id;
        response.ecu_id = request.ecu_id;
        response.success = false;
        response.error_message = "Failed to route to ECU";
        response.timestamp = std::chrono::steady_clock::now();
        
        sendResponse(response);
        
        pending_requests_.erase(request.request_id);
        failed_requests_++;
        return false;
    }
    
    return true;
}

bool RemoteDiagnosticsHandler::handleBroadcastRequest(const std::string& json_payload) {
    // TODO: Implement broadcast to all ECUs in a zone
    std::cout << "[RemoteDiag] Broadcast request received" << std::endl;
    
    // Parse zone_id from JSON
    // For each ECU in zone:
    //   - Create individual request
    //   - Send to ECU
    //   - Collect responses
    // Aggregate and send back
    
    return true;
}

void RemoteDiagnosticsHandler::processPendingRequests() {
    auto now = std::chrono::steady_clock::now();
    
    std::vector<std::string> to_remove;
    
    for (auto& [request_id, request] : pending_requests_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - request.timestamp
        ).count();
        
        if (elapsed > request.timeout_ms) {
            std::cout << "[RemoteDiag] Request " << request_id << " timed out" << std::endl;
            
            if (request.retry_count < request.max_retries) {
                // Retry
                std::cout << "[RemoteDiag] Retrying request (attempt " 
                          << static_cast<int>(request.retry_count + 1) << ")" << std::endl;
                
                request.retry_count++;
                request.timestamp = now;
                retry_requests_++;
                
                sendToECU(request);
            } else {
                // Max retries exceeded
                std::cout << "[RemoteDiag] Max retries exceeded for " << request_id << std::endl;
                
                DiagnosticResponse response;
                response.request_id = request_id;
                response.ecu_id = request.ecu_id;
                response.success = false;
                response.error_message = "Timeout: ECU did not respond";
                response.duration_ms = elapsed;
                response.timestamp = now;
                
                sendResponse(response);
                
                to_remove.push_back(request_id);
                timeout_requests_++;
            }
        }
    }
    
    // Remove completed/failed requests
    for (const auto& request_id : to_remove) {
        pending_requests_.erase(request_id);
    }
}

void RemoteDiagnosticsHandler::registerECU(
    const std::string& ecu_id,
    const std::string& zonal_gateway_id,
    uint16_t logical_address,
    const std::string& ip_address,
    uint16_t port
) {
    ECURouting routing;
    routing.ecu_id = ecu_id;
    routing.zonal_gateway_id = zonal_gateway_id;
    routing.logical_address = logical_address;
    routing.ip_address = ip_address;
    routing.port = port;
    
    ecu_routing_[ecu_id] = routing;
    
    std::cout << "[RemoteDiag] Registered ECU: " << ecu_id 
              << " @ " << zonal_gateway_id 
              << " (0x" << std::hex << logical_address << std::dec << ")" << std::endl;
}

void RemoteDiagnosticsHandler::setResponseCallback(DiagnosticResponseCallback callback) {
    response_callback_ = callback;
}

std::map<std::string, uint64_t> RemoteDiagnosticsHandler::getStatistics() const {
    return {
        {"total_requests", total_requests_},
        {"successful_requests", successful_requests_},
        {"failed_requests", failed_requests_},
        {"timeout_requests", timeout_requests_},
        {"retry_requests", retry_requests_},
        {"pending_requests", pending_requests_.size()}
    };
}

// ============================================================================
// Private Methods
// ============================================================================

bool RemoteDiagnosticsHandler::parseRequest(
    const std::string& json_payload,
    DiagnosticRequest& request
) {
    // Simple JSON parsing (use proper library in production)
    // Expected format:
    // {
    //   "request_id": "diag-12345",
    //   "ecu_id": "ECU_001",
    //   "service_id": "0x22",
    //   "data": "F190"
    // }
    
    try {
        // Extract request_id
        std::regex request_id_regex(R"("request_id"\s*:\s*"([^"]+)")");
        std::smatch match;
        if (std::regex_search(json_payload, match, request_id_regex)) {
            request.request_id = match[1];
        }
        
        // Extract ecu_id
        std::regex ecu_id_regex(R"("ecu_id"\s*:\s*"([^"]+)")");
        if (std::regex_search(json_payload, match, ecu_id_regex)) {
            request.ecu_id = match[1];
        }
        
        // Extract service_id
        std::regex service_id_regex(R"("service_id"\s*:\s*"(0x[0-9A-Fa-f]+)")");
        if (std::regex_search(json_payload, match, service_id_regex)) {
            std::string service_str = match[1];
            request.service_id = std::stoi(service_str, nullptr, 16);
        }
        
        // Extract data (hex string)
        std::regex data_regex(R"("data"\s*:\s*"([0-9A-Fa-f]*)")");
        if (std::regex_search(json_payload, match, data_regex)) {
            std::string data_str = match[1];
            
            // Convert hex string to bytes
            for (size_t i = 0; i < data_str.length(); i += 2) {
                std::string byte_str = data_str.substr(i, 2);
                uint8_t byte = std::stoi(byte_str, nullptr, 16);
                request.data.push_back(byte);
            }
        }
        
        return !request.request_id.empty() && !request.ecu_id.empty();
        
    } catch (const std::exception& e) {
        std::cerr << "[RemoteDiag] Parse error: " << e.what() << std::endl;
        return false;
    }
}

const ECURouting* RemoteDiagnosticsHandler::findECURouting(const std::string& ecu_id) const {
    auto it = ecu_routing_.find(ecu_id);
    if (it != ecu_routing_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool RemoteDiagnosticsHandler::sendToECU(const DiagnosticRequest& request) {
    // Find ECU routing
    const ECURouting* routing = findECURouting(request.ecu_id);
    if (!routing) {
        std::cerr << "[RemoteDiag] ECU routing not found: " << request.ecu_id << std::endl;
        return false;
    }
    
    // Build UDS message
    std::vector<uint8_t> uds_data;
    uds_data.push_back(request.service_id);
    uds_data.insert(uds_data.end(), request.data.begin(), request.data.end());
    
    // Build DoIP message
    std::vector<uint8_t> doip_message = buildDoIPDiagnosticMessage(
        0x0E00,  // Tester address
        routing->logical_address,
        uds_data
    );
    
    std::cout << "[RemoteDiag] Sending to " << routing->zonal_gateway_id 
              << " (" << routing->ip_address << ":" << routing->port << ")" << std::endl;
    
    // TODO: Send via DoIP client
    // doip_client_send(doip_client_, routing->ip_address, routing->port, doip_message);
    
    return true;
}

std::vector<uint8_t> RemoteDiagnosticsHandler::buildDoIPDiagnosticMessage(
    uint16_t source_address,
    uint16_t target_address,
    const std::vector<uint8_t>& uds_data
) {
    std::vector<uint8_t> message;
    
    // DoIP Header
    message.push_back(0x02);  // Protocol version
    message.push_back(0xFD);  // Inverse protocol version
    
    // Payload type: Diagnostic Message (0x8001)
    message.push_back(0x80);
    message.push_back(0x01);
    
    // Payload length
    uint32_t payload_len = 4 + uds_data.size();
    message.push_back((payload_len >> 24) & 0xFF);
    message.push_back((payload_len >> 16) & 0xFF);
    message.push_back((payload_len >> 8) & 0xFF);
    message.push_back(payload_len & 0xFF);
    
    // Source address
    message.push_back((source_address >> 8) & 0xFF);
    message.push_back(source_address & 0xFF);
    
    // Target address
    message.push_back((target_address >> 8) & 0xFF);
    message.push_back(target_address & 0xFF);
    
    // UDS data
    message.insert(message.end(), uds_data.begin(), uds_data.end());
    
    return message;
}

bool RemoteDiagnosticsHandler::parseDoIPDiagnosticResponse(
    const std::vector<uint8_t>& doip_message,
    std::vector<uint8_t>& uds_response
) {
    if (doip_message.size() < 12) {
        return false;
    }
    
    // Skip DoIP header (8 bytes) and addresses (4 bytes)
    uds_response.assign(doip_message.begin() + 12, doip_message.end());
    
    return true;
}

void RemoteDiagnosticsHandler::sendResponse(const DiagnosticResponse& response) {
    std::string json = buildResponseJSON(response);
    
    std::cout << "[RemoteDiag] Sending response for " << response.request_id << std::endl;
    
    // TODO: Send via MQTT
    // mqtt_publish(mqtt_client_, topic, json);
    
    // Call callback
    if (response_callback_) {
        response_callback_(response);
    }
    
    if (response.success) {
        successful_requests_++;
    } else {
        failed_requests_++;
    }
}

std::string RemoteDiagnosticsHandler::buildResponseJSON(const DiagnosticResponse& response) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"request_id\": \"" << response.request_id << "\",\n";
    json << "  \"ecu_id\": \"" << response.ecu_id << "\",\n";
    json << "  \"success\": " << (response.success ? "true" : "false") << ",\n";
    
    if (response.success) {
        json << "  \"response_data\": \"";
        for (uint8_t byte : response.response_data) {
            json << std::hex << std::setw(2) << std::setfill('0') 
                 << static_cast<int>(byte);
        }
        json << std::dec << "\",\n";
    } else {
        json << "  \"error\": \"" << response.error_message << "\",\n";
    }
    
    json << "  \"duration_ms\": " << response.duration_ms << "\n";
    json << "}";
    
    return json.str();
}

} // namespace vmg

