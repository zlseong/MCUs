/**
 * @file doip_server.hpp
 * @brief DoIP Server for VMG (Vehicle Gateway)
 * 
 * C++ implementation of Python DoIPServer class.
 * Supports TCP/UDP for DoIP (ISO 13400) communication.
 */

#ifndef DOIP_SERVER_HPP
#define DOIP_SERVER_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <cstdint>

namespace vmg {

/**
 * @brief DoIP Message Types
 */
enum class DoIPPayloadType : uint16_t {
    VehicleIdentificationReq = 0x0001,
    VehicleIdentificationRes = 0x0004,
    RoutingActivationReq = 0x0005,
    RoutingActivationRes = 0x0006,
    AliveCheckReq = 0x0007,
    AliveCheckRes = 0x0008,
    DiagnosticMessage = 0x8001,
    DiagnosticMessagePosAck = 0x8002,
    DiagnosticMessageNegAck = 0x8003
};

/**
 * @brief DoIP Header (8 bytes)
 */
struct DoIPHeader {
    uint8_t protocol_version;           // 0x02
    uint8_t inverse_protocol_version;   // 0xFD
    uint16_t payload_type;              // Host byte order
    uint32_t payload_length;            // Host byte order

    DoIPHeader() : protocol_version(0x02), inverse_protocol_version(0xFD),
                   payload_type(0), payload_length(0) {}
};

/**
 * @brief DoIP Message
 */
class DoIPMessage {
public:
    DoIPMessage(DoIPPayloadType type = DoIPPayloadType::VehicleIdentificationReq,
                const std::vector<uint8_t>& payload = {});
    
    // Serialize to bytes
    std::vector<uint8_t> toBytes() const;
    
    // Deserialize from bytes
    static DoIPMessage fromBytes(const std::vector<uint8_t>& data);
    static DoIPMessage fromBytes(const uint8_t* data, size_t len);
    
    // Getters
    DoIPPayloadType getPayloadType() const { return static_cast<DoIPPayloadType>(header_.payload_type); }
    const std::vector<uint8_t>& getPayload() const { return payload_data_; }
    const DoIPHeader& getHeader() const { return header_; }
    
    // Setters
    void setPayloadType(DoIPPayloadType type) { header_.payload_type = static_cast<uint16_t>(type); }
    void setPayload(const std::vector<uint8_t>& payload);

private:
    DoIPHeader header_;
    std::vector<uint8_t> payload_data_;
};

/**
 * @brief DoIP Client Session (per TCP connection)
 */
class DoIPClientSession {
public:
    DoIPClientSession(int socket, const std::string& address);
    ~DoIPClientSession();

    int getSocket() const { return socket_; }
    const std::string& getAddress() const { return address_; }
    bool isRoutingActive() const { return routing_active_; }
    void setRoutingActive(bool active) { routing_active_ = active; }
    uint16_t getSourceAddress() const { return source_address_; }
    void setSourceAddress(uint16_t addr) { source_address_ = addr; }

private:
    int socket_;
    std::string address_;
    bool routing_active_;
    uint16_t source_address_;
};

/**
 * @brief UDS Diagnostic Request Handler
 */
using UDSHandler = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

/**
 * @brief DoIP Server Configuration
 */
struct DoIPServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 13400;
    std::string vin = "WBADT43452G296403";
    uint16_t logical_address = 0x0100;
    std::vector<uint8_t> eid = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};  // MAC address
    std::vector<uint8_t> gid = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};  // Group ID
    size_t max_clients = 10;
    bool enable_tls = false;
};

/**
 * @brief DoIP Server
 * 
 * Python DoIPServer equivalent.
 * Handles both UDP (vehicle discovery) and TCP (diagnostic communication).
 */
class DoIPServer {
public:
    explicit DoIPServer(const DoIPServerConfig& config = DoIPServerConfig());
    ~DoIPServer();

    // Start/Stop server
    bool start();
    void stop();
    bool isRunning() const { return running_; }

    // Register UDS handler
    void registerUDSHandler(UDSHandler handler);
    
    // Set custom VIN/EID/GID
    void setVIN(const std::string& vin);
    void setLogicalAddress(uint16_t address);
    void setEID(const std::vector<uint8_t>& eid);
    void setGID(const std::vector<uint8_t>& gid);

    // Statistics
    size_t getActiveConnections() const;
    uint64_t getTotalMessages() const { return total_messages_; }

private:
    // Server threads
    void udpListenerThread();
    void tcpAcceptThread();
    void clientHandlerThread(std::shared_ptr<DoIPClientSession> session);

    // Message handlers
    void handleUDPMessage(const DoIPMessage& msg, const std::string& client_addr);
    void handleTCPMessage(const DoIPMessage& msg, std::shared_ptr<DoIPClientSession> session);

    // Specific message handlers
    DoIPMessage handleVehicleIdentificationReq(const DoIPMessage& msg);
    DoIPMessage handleRoutingActivationReq(const DoIPMessage& msg, std::shared_ptr<DoIPClientSession> session);
    DoIPMessage handleDiagnosticMessage(const DoIPMessage& msg, std::shared_ptr<DoIPClientSession> session);
    DoIPMessage handleAliveCheckReq(const DoIPMessage& msg);

    // Socket helpers
    int createUDPSocket();
    int createTCPSocket();
    bool sendMessage(int socket, const DoIPMessage& msg);
    DoIPMessage receiveMessage(int socket);

    // Configuration
    DoIPServerConfig config_;

    // Sockets
    int udp_socket_;
    int tcp_socket_;

    // State
    std::atomic<bool> running_;
    std::atomic<uint64_t> total_messages_;

    // Threads
    std::unique_ptr<std::thread> udp_thread_;
    std::unique_ptr<std::thread> tcp_thread_;
    std::vector<std::unique_ptr<std::thread>> client_threads_;

    // Client sessions
    std::map<int, std::shared_ptr<DoIPClientSession>> sessions_;
    mutable std::mutex sessions_mutex_;

    // UDS handler
    UDSHandler uds_handler_;
    std::mutex uds_mutex_;
};

} // namespace vmg

#endif // DOIP_SERVER_HPP

