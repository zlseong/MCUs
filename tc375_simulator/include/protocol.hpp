#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace tc375 {

using json = nlohmann::json;

// Message types
enum class MessageType {
    HEARTBEAT,
    STATUS_REPORT,
    COMMAND_ACK,
    SENSOR_DATA,
    ERROR
};

std::string messageTypeToString(MessageType type);
MessageType stringToMessageType(const std::string& str);

// Protocol message structure
struct ProtocolMessage {
    MessageType type;
    std::string device_id;
    json payload;
    std::string timestamp;

    // Serialization
    std::string toJSON() const;
    static ProtocolMessage fromJSON(const std::string& json_str);
};

// Message builders
ProtocolMessage createHeartbeat(const std::string& device_id);
ProtocolMessage createStatusReport(const std::string& device_id, const json& status);
ProtocolMessage createSensorData(const std::string& device_id, const json& data);
ProtocolMessage createCommandAck(const std::string& device_id, const std::string& command_id, bool success);
ProtocolMessage createError(const std::string& device_id, const std::string& error_msg);

} // namespace tc375

