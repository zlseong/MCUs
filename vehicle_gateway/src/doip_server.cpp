/**
 * @file doip_server.cpp
 * @brief DoIP Server Implementation
 */

#include "doip_server.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace vmg {

// ============================================================================
// DoIPMessage Implementation
// ============================================================================

DoIPMessage::DoIPMessage(DoIPPayloadType type, const std::vector<uint8_t>& payload)
    : payload_data_(payload) {
    header_.payload_type = static_cast<uint16_t>(type);
    header_.payload_length = static_cast<uint32_t>(payload.size());
}

void DoIPMessage::setPayload(const std::vector<uint8_t>& payload) {
    payload_data_ = payload;
    header_.payload_length = static_cast<uint32_t>(payload.size());
}

std::vector<uint8_t> DoIPMessage::toBytes() const {
    std::vector<uint8_t> result;
    result.reserve(8 + payload_data_.size());

    // Header (8 bytes, network byte order)
    result.push_back(header_.protocol_version);
    result.push_back(header_.inverse_protocol_version);

    // Payload type (big-endian)
    uint16_t pt_be = htons(header_.payload_type);
    result.push_back((pt_be >> 8) & 0xFF);
    result.push_back(pt_be & 0xFF);

    // Payload length (big-endian)
    uint32_t len_be = htonl(header_.payload_length);
    result.push_back((len_be >> 24) & 0xFF);
    result.push_back((len_be >> 16) & 0xFF);
    result.push_back((len_be >> 8) & 0xFF);
    result.push_back(len_be & 0xFF);

    // Payload
    result.insert(result.end(), payload_data_.begin(), payload_data_.end());

    return result;
}

DoIPMessage DoIPMessage::fromBytes(const std::vector<uint8_t>& data) {
    return fromBytes(data.data(), data.size());
}

DoIPMessage DoIPMessage::fromBytes(const uint8_t* data, size_t len) {
    if (len < 8) {
        throw std::runtime_error("Invalid DoIP message: too short");
    }

    // Parse header
    uint8_t protocol_version = data[0];
    uint8_t inverse_protocol_version = data[1];

    if (protocol_version != 0x02 || inverse_protocol_version != 0xFD) {
        throw std::runtime_error("Invalid DoIP protocol version");
    }

    uint16_t payload_type = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    uint32_t payload_length = (static_cast<uint32_t>(data[4]) << 24) |
                               (static_cast<uint32_t>(data[5]) << 16) |
                               (static_cast<uint32_t>(data[6]) << 8) |
                               data[7];

    if (len < 8 + payload_length) {
        throw std::runtime_error("Invalid DoIP message: payload truncated");
    }

    // Extract payload
    std::vector<uint8_t> payload(data + 8, data + 8 + payload_length);

    DoIPMessage msg(static_cast<DoIPPayloadType>(payload_type), payload);
    return msg;
}

// ============================================================================
// DoIPClientSession Implementation
// ============================================================================

DoIPClientSession::DoIPClientSession(int socket, const std::string& address)
    : socket_(socket), address_(address), routing_active_(false), source_address_(0) {
}

DoIPClientSession::~DoIPClientSession() {
    if (socket_ >= 0) {
        close(socket_);
    }
}

// ============================================================================
// DoIPServer Implementation
// ============================================================================

DoIPServer::DoIPServer(const DoIPServerConfig& config)
    : config_(config), udp_socket_(-1), tcp_socket_(-1), running_(false), total_messages_(0) {
}

DoIPServer::~DoIPServer() {
    stop();
}

bool DoIPServer::start() {
    if (running_) {
        std::cerr << "DoIP Server already running" << std::endl;
        return false;
    }

    // Create sockets
    udp_socket_ = createUDPSocket();
    tcp_socket_ = createTCPSocket();

    if (udp_socket_ < 0 || tcp_socket_ < 0) {
        std::cerr << "Failed to create sockets" << std::endl;
        return false;
    }

    running_ = true;

    // Start threads
    udp_thread_ = std::make_unique<std::thread>(&DoIPServer::udpListenerThread, this);
    tcp_thread_ = std::make_unique<std::thread>(&DoIPServer::tcpAcceptThread, this);

    std::cout << "DoIP Server started on " << config_.host << ":" << config_.port << std::endl;
    std::cout << "  VIN: " << config_.vin << std::endl;
    std::cout << "  Logical Address: 0x" << std::hex << config_.logical_address << std::dec << std::endl;

    return true;
}

void DoIPServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Close sockets
    if (udp_socket_ >= 0) {
        close(udp_socket_);
        udp_socket_ = -1;
    }
    if (tcp_socket_ >= 0) {
        close(tcp_socket_);
        tcp_socket_ = -1;
    }

    // Join threads
    if (udp_thread_ && udp_thread_->joinable()) {
        udp_thread_->join();
    }
    if (tcp_thread_ && tcp_thread_->joinable()) {
        tcp_thread_->join();
    }

    // Wait for client threads
    for (auto& thread : client_threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    client_threads_.clear();

    // Close client sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.clear();
    }

    std::cout << "DoIP Server stopped" << std::endl;
}

void DoIPServer::registerUDSHandler(UDSHandler handler) {
    std::lock_guard<std::mutex> lock(uds_mutex_);
    uds_handler_ = handler;
}

void DoIPServer::setVIN(const std::string& vin) {
    config_.vin = vin;
}

void DoIPServer::setLogicalAddress(uint16_t address) {
    config_.logical_address = address;
}

void DoIPServer::setEID(const std::vector<uint8_t>& eid) {
    config_.eid = eid;
}

void DoIPServer::setGID(const std::vector<uint8_t>& gid) {
    config_.gid = gid;
}

size_t DoIPServer::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

// ============================================================================
// Socket Creation
// ============================================================================

int DoIPServer::createUDPSocket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create UDP socket: " << strerror(errno) << std::endl;
        return -1;
    }

    // Set socket options
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config_.port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind UDP socket: " << strerror(errno) << std::endl;
        close(sock);
        return -1;
    }

    return sock;
}

int DoIPServer::createTCPSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create TCP socket: " << strerror(errno) << std::endl;
        return -1;
    }

    // Set socket options
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config_.port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind TCP socket: " << strerror(errno) << std::endl;
        close(sock);
        return -1;
    }

    // Listen
    if (listen(sock, static_cast<int>(config_.max_clients)) < 0) {
        std::cerr << "Failed to listen on TCP socket: " << strerror(errno) << std::endl;
        close(sock);
        return -1;
    }

    return sock;
}

// ============================================================================
// Thread Functions
// ============================================================================

void DoIPServer::udpListenerThread() {
    std::vector<uint8_t> buffer(4096);

    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        ssize_t recv_len = recvfrom(udp_socket_, buffer.data(), buffer.size(), 0,
                                     (struct sockaddr*)&client_addr, &addr_len);

        if (recv_len < 0) {
            if (errno == EINTR || !running_) {
                break;
            }
            std::cerr << "UDP receive error: " << strerror(errno) << std::endl;
            continue;
        }

        try {
            DoIPMessage msg = DoIPMessage::fromBytes(buffer.data(), recv_len);
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::string client_address = std::string(client_ip) + ":" + 
                                          std::to_string(ntohs(client_addr.sin_port));

            handleUDPMessage(msg, client_address);
            total_messages_++;

            // Send response back to client
            if (msg.getPayloadType() == DoIPPayloadType::VehicleIdentificationReq) {
                DoIPMessage response = handleVehicleIdentificationReq(msg);
                std::vector<uint8_t> response_data = response.toBytes();
                
                sendto(udp_socket_, response_data.data(), response_data.size(), 0,
                       (struct sockaddr*)&client_addr, addr_len);
            }

        } catch (const std::exception& e) {
            std::cerr << "UDP message parsing error: " << e.what() << std::endl;
        }
    }
}

void DoIPServer::tcpAcceptThread() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_sock = accept(tcp_socket_, (struct sockaddr*)&client_addr, &addr_len);
        if (client_sock < 0) {
            if (errno == EINTR || !running_) {
                break;
            }
            std::cerr << "TCP accept error: " << strerror(errno) << std::endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::string client_address = std::string(client_ip) + ":" + 
                                      std::to_string(ntohs(client_addr.sin_port));

        std::cout << "TCP connection from " << client_address << std::endl;

        // Create session
        auto session = std::make_shared<DoIPClientSession>(client_sock, client_address);

        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_[client_sock] = session;
        }

        // Start client handler thread
        client_threads_.push_back(
            std::make_unique<std::thread>(&DoIPServer::clientHandlerThread, this, session)
        );
    }
}

void DoIPServer::clientHandlerThread(std::shared_ptr<DoIPClientSession> session) {
    std::vector<uint8_t> buffer(8192);

    while (running_) {
        // Receive header first (8 bytes)
        ssize_t recv_len = recv(session->getSocket(), buffer.data(), 8, MSG_WAITALL);
        
        if (recv_len <= 0) {
            break;  // Connection closed or error
        }

        if (recv_len < 8) {
            std::cerr << "Received incomplete header from " << session->getAddress() << std::endl;
            break;
        }

        // Parse header to get payload length
        uint32_t payload_length = (static_cast<uint32_t>(buffer[4]) << 24) |
                                   (static_cast<uint32_t>(buffer[5]) << 16) |
                                   (static_cast<uint32_t>(buffer[6]) << 8) |
                                   buffer[7];

        if (payload_length > buffer.size() - 8) {
            buffer.resize(8 + payload_length);
        }

        // Receive payload
        if (payload_length > 0) {
            recv_len = recv(session->getSocket(), buffer.data() + 8, payload_length, MSG_WAITALL);
            if (recv_len < static_cast<ssize_t>(payload_length)) {
                std::cerr << "Received incomplete payload from " << session->getAddress() << std::endl;
                break;
            }
        }

        try {
            DoIPMessage msg = DoIPMessage::fromBytes(buffer.data(), 8 + payload_length);
            handleTCPMessage(msg, session);
            total_messages_++;

        } catch (const std::exception& e) {
            std::cerr << "TCP message parsing error: " << e.what() << std::endl;
        }
    }

    std::cout << "Client disconnected: " << session->getAddress() << std::endl;

    // Remove session
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session->getSocket());
    }
}

// ============================================================================
// Message Handlers
// ============================================================================

void DoIPServer::handleUDPMessage(const DoIPMessage& msg, const std::string& client_addr) {
    std::cout << "UDP message from " << client_addr << ": type=0x" 
              << std::hex << static_cast<int>(msg.getPayloadType()) << std::dec << std::endl;
}

void DoIPServer::handleTCPMessage(const DoIPMessage& msg, std::shared_ptr<DoIPClientSession> session) {
    DoIPMessage response;

    switch (msg.getPayloadType()) {
        case DoIPPayloadType::RoutingActivationReq:
            response = handleRoutingActivationReq(msg, session);
            sendMessage(session->getSocket(), response);
            break;

        case DoIPPayloadType::DiagnosticMessage:
            response = handleDiagnosticMessage(msg, session);
            sendMessage(session->getSocket(), response);
            break;

        case DoIPPayloadType::AliveCheckReq:
            response = handleAliveCheckReq(msg);
            sendMessage(session->getSocket(), response);
            break;

        default:
            std::cerr << "Unsupported payload type: 0x" << std::hex 
                      << static_cast<int>(msg.getPayloadType()) << std::dec << std::endl;
            break;
    }
}

DoIPMessage DoIPServer::handleVehicleIdentificationReq(const DoIPMessage& msg) {
    (void)msg;  // Unused

    std::vector<uint8_t> payload;
    
    // VIN (17 bytes)
    payload.insert(payload.end(), config_.vin.begin(), config_.vin.end());
    payload.resize(17, ' ');  // Pad to 17 bytes

    // Logical address (2 bytes, big-endian)
    payload.push_back((config_.logical_address >> 8) & 0xFF);
    payload.push_back(config_.logical_address & 0xFF);

    // EID (6 bytes)
    payload.insert(payload.end(), config_.eid.begin(), config_.eid.end());

    // GID (6 bytes)
    payload.insert(payload.end(), config_.gid.begin(), config_.gid.end());

    // Further action required (1 byte)
    payload.push_back(0x00);

    // VIN/GID sync status (optional, 1 byte)
    payload.push_back(0x00);

    return DoIPMessage(DoIPPayloadType::VehicleIdentificationRes, payload);
}

DoIPMessage DoIPServer::handleRoutingActivationReq(const DoIPMessage& msg, 
                                                     std::shared_ptr<DoIPClientSession> session) {
    const auto& payload = msg.getPayload();
    
    if (payload.size() < 7) {
        std::cerr << "Invalid routing activation request" << std::endl;
        return DoIPMessage(DoIPPayloadType::RoutingActivationRes, {});
    }

    // Parse source address
    uint16_t source_address = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
    uint8_t activation_type = payload[2];

    std::cout << "Routing activation request: source=0x" << std::hex << source_address 
              << ", type=0x" << static_cast<int>(activation_type) << std::dec << std::endl;

    // Update session
    session->setSourceAddress(source_address);
    session->setRoutingActive(true);

    // Build response
    std::vector<uint8_t> response_payload;
    
    // Tester address (echo)
    response_payload.push_back((source_address >> 8) & 0xFF);
    response_payload.push_back(source_address & 0xFF);

    // Entity address
    response_payload.push_back((config_.logical_address >> 8) & 0xFF);
    response_payload.push_back(config_.logical_address & 0xFF);

    // Response code (0x10 = success)
    response_payload.push_back(0x10);

    // Reserved (4 bytes)
    response_payload.insert(response_payload.end(), 4, 0x00);

    return DoIPMessage(DoIPPayloadType::RoutingActivationRes, response_payload);
}

DoIPMessage DoIPServer::handleDiagnosticMessage(const DoIPMessage& msg,
                                                 std::shared_ptr<DoIPClientSession> session) {
    const auto& payload = msg.getPayload();
    
    if (payload.size() < 4) {
        std::cerr << "Invalid diagnostic message" << std::endl;
        return DoIPMessage(DoIPPayloadType::DiagnosticMessageNegAck, {});
    }

    // Parse addresses
    uint16_t source_address = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
    uint16_t target_address = (static_cast<uint16_t>(payload[2]) << 8) | payload[3];

    // Extract UDS data
    std::vector<uint8_t> uds_request(payload.begin() + 4, payload.end());

    std::cout << "Diagnostic message: SA=0x" << std::hex << source_address 
              << ", TA=0x" << target_address << std::dec 
              << ", UDS=[";
    for (size_t i = 0; i < std::min(uds_request.size(), size_t(8)); i++) {
        std::cout << std::hex << static_cast<int>(uds_request[i]) << " ";
    }
    std::cout << "]" << std::dec << std::endl;

    // Send ACK
    std::vector<uint8_t> ack_payload;
    ack_payload.push_back((source_address >> 8) & 0xFF);
    ack_payload.push_back(source_address & 0xFF);
    ack_payload.push_back((target_address >> 8) & 0xFF);
    ack_payload.push_back(target_address & 0xFF);
    ack_payload.push_back(0x00);  // ACK code
    DoIPMessage ack(DoIPPayloadType::DiagnosticMessagePosAck, ack_payload);
    sendMessage(session->getSocket(), ack);

    // Process UDS request
    std::vector<uint8_t> uds_response;
    {
        std::lock_guard<std::mutex> lock(uds_mutex_);
        if (uds_handler_) {
            uds_response = uds_handler_(uds_request);
        } else {
            // Default: echo request with positive response offset
            if (!uds_request.empty()) {
                uds_response.push_back(uds_request[0] + 0x40);
                uds_response.insert(uds_response.end(), uds_request.begin() + 1, uds_request.end());
            }
        }
    }

    // Build diagnostic response
    std::vector<uint8_t> response_payload;
    response_payload.push_back((target_address >> 8) & 0xFF);
    response_payload.push_back(target_address & 0xFF);
    response_payload.push_back((source_address >> 8) & 0xFF);
    response_payload.push_back(source_address & 0xFF);
    response_payload.insert(response_payload.end(), uds_response.begin(), uds_response.end());

    return DoIPMessage(DoIPPayloadType::DiagnosticMessage, response_payload);
}

DoIPMessage DoIPServer::handleAliveCheckReq(const DoIPMessage& msg) {
    const auto& payload = msg.getPayload();
    
    if (payload.size() < 2) {
        return DoIPMessage(DoIPPayloadType::AliveCheckRes, {});
    }

    // Echo source address
    return DoIPMessage(DoIPPayloadType::AliveCheckRes, payload);
}

bool DoIPServer::sendMessage(int socket, const DoIPMessage& msg) {
    std::vector<uint8_t> data = msg.toBytes();
    ssize_t sent = send(socket, data.data(), data.size(), 0);
    return sent == static_cast<ssize_t>(data.size());
}

} // namespace vmg
