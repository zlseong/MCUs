/**
 * @file remote_diagnostics_handler.hpp
 * @brief Remote Diagnostics Handler for VMG
 * 
 * Handles diagnostic requests from OTA Server via MQTT
 * and forwards them to Zonal Gateways/ECUs via DoIP.
 */

#ifndef REMOTE_DIAGNOSTICS_HANDLER_HPP
#define REMOTE_DIAGNOSTICS_HANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <cstdint>
#include <chrono>

namespace vmg {

/**
 * @brief Diagnostic request from server
 */
struct DiagnosticRequest {
    std::string request_id;
    std::string vin;
    std::string ecu_id;
    uint8_t service_id;
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;
    uint32_t timeout_ms = 5000;
    uint8_t retry_count = 0;
    uint8_t max_retries = 3;
};

/**
 * @brief Diagnostic response to server
 */
struct DiagnosticResponse {
    std::string request_id;
    std::string ecu_id;
    bool success;
    std::vector<uint8_t> response_data;
    std::string error_message;
    uint32_t duration_ms;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief ECU routing information
 */
struct ECURouting {
    std::string ecu_id;
    std::string zonal_gateway_id;
    uint16_t logical_address;
    std::string ip_address;
    uint16_t port;
};

/**
 * @brief Diagnostic request callback
 */
using DiagnosticResponseCallback = std::function<void(const DiagnosticResponse&)>;

/**
 * @brief Remote Diagnostics Handler
 * 
 * Responsibilities:
 * 1. Receive diagnostic requests from MQTT
 * 2. Route requests to appropriate Zonal Gateway/ECU
 * 3. Handle timeouts and retries
 * 4. Send responses back via MQTT
 */
class RemoteDiagnosticsHandler {
public:
    RemoteDiagnosticsHandler();
    ~RemoteDiagnosticsHandler();
    
    /**
     * @brief Initialize handler
     * 
     * @param mqtt_client MQTT client for server communication
     * @param doip_client DoIP client for ZG/ECU communication
     * @return true on success
     */
    bool initialize(void* mqtt_client, void* doip_client);
    
    /**
     * @brief Handle incoming diagnostic request from MQTT
     * 
     * @param json_payload JSON payload from MQTT message
     * @return true if request accepted
     */
    bool handleRequest(const std::string& json_payload);
    
    /**
     * @brief Handle broadcast diagnostic request
     * 
     * @param json_payload JSON payload with zone_id
     * @return true if request accepted
     */
    bool handleBroadcastRequest(const std::string& json_payload);
    
    /**
     * @brief Process pending requests (call periodically)
     */
    void processPendingRequests();
    
    /**
     * @brief Register ECU routing information
     * 
     * @param ecu_id ECU identifier
     * @param zonal_gateway_id Zonal Gateway identifier
     * @param logical_address DoIP logical address
     * @param ip_address ZG IP address
     * @param port ZG port (default 13400)
     */
    void registerECU(
        const std::string& ecu_id,
        const std::string& zonal_gateway_id,
        uint16_t logical_address,
        const std::string& ip_address,
        uint16_t port = 13400
    );
    
    /**
     * @brief Set response callback
     * 
     * @param callback Callback function for responses
     */
    void setResponseCallback(DiagnosticResponseCallback callback);
    
    /**
     * @brief Get statistics
     * 
     * @return Map of statistics
     */
    std::map<std::string, uint64_t> getStatistics() const;

private:
    /**
     * @brief Parse JSON diagnostic request
     * 
     * @param json_payload JSON string
     * @param request Output request
     * @return true on success
     */
    bool parseRequest(const std::string& json_payload, DiagnosticRequest& request);
    
    /**
     * @brief Find ECU routing information
     * 
     * @param ecu_id ECU identifier
     * @return Routing info or nullptr
     */
    const ECURouting* findECURouting(const std::string& ecu_id) const;
    
    /**
     * @brief Send diagnostic request to ECU via DoIP
     * 
     * @param request Diagnostic request
     * @return true if sent successfully
     */
    bool sendToECU(const DiagnosticRequest& request);
    
    /**
     * @brief Build DoIP diagnostic message
     * 
     * @param source_address Tester logical address (0x0E00)
     * @param target_address ECU logical address
     * @param uds_data UDS service + data
     * @return DoIP message bytes
     */
    std::vector<uint8_t> buildDoIPDiagnosticMessage(
        uint16_t source_address,
        uint16_t target_address,
        const std::vector<uint8_t>& uds_data
    );
    
    /**
     * @brief Parse DoIP diagnostic response
     * 
     * @param doip_message DoIP message bytes
     * @param uds_response Output UDS response
     * @return true on success
     */
    bool parseDoIPDiagnosticResponse(
        const std::vector<uint8_t>& doip_message,
        std::vector<uint8_t>& uds_response
    );
    
    /**
     * @brief Handle timeout for request
     * 
     * @param request_id Request identifier
     */
    void handleTimeout(const std::string& request_id);
    
    /**
     * @brief Retry failed request
     * 
     * @param request_id Request identifier
     */
    void retryRequest(const std::string& request_id);
    
    /**
     * @brief Send response to server via MQTT
     * 
     * @param response Diagnostic response
     */
    void sendResponse(const DiagnosticResponse& response);
    
    /**
     * @brief Build JSON response
     * 
     * @param response Diagnostic response
     * @return JSON string
     */
    std::string buildResponseJSON(const DiagnosticResponse& response);

private:
    void* mqtt_client_;
    void* doip_client_;
    
    // ECU routing table
    std::map<std::string, ECURouting> ecu_routing_;
    
    // Pending requests
    std::map<std::string, DiagnosticRequest> pending_requests_;
    
    // Response callback
    DiagnosticResponseCallback response_callback_;
    
    // Statistics
    uint64_t total_requests_;
    uint64_t successful_requests_;
    uint64_t failed_requests_;
    uint64_t timeout_requests_;
    uint64_t retry_requests_;
};

} // namespace vmg

#endif // REMOTE_DIAGNOSTICS_HANDLER_HPP

