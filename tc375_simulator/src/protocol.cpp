#include "protocol.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace tc375 {

std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::HEARTBEAT: return "HEARTBEAT";
        case MessageType::STATUS_REPORT: return "STATUS_REPORT";
        case MessageType::COMMAND_ACK: return "COMMAND_ACK";
        case MessageType::SENSOR_DATA: return "SENSOR_DATA";
        case MessageType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

MessageType stringToMessageType(const std::string& str) {
    if (str == "HEARTBEAT") return MessageType::HEARTBEAT;
    if (str == "STATUS_REPORT") return MessageType::STATUS_REPORT;
    if (str == "COMMAND_ACK") return MessageType::COMMAND_ACK;
    if (str == "SENSOR_DATA") return MessageType::SENSOR_DATA;
    if (str == "ERROR") return MessageType::ERROR;
    return MessageType::ERROR;
}

static std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string ProtocolMessage::toJSON() const {
    json j = {
        {"type", messageTypeToString(type)},
        {"device_id", device_id},
        {"payload", payload},
        {"timestamp", timestamp}
    };
    return j.dump();
}

ProtocolMessage ProtocolMessage::fromJSON(const std::string& json_str) {
    json j = json::parse(json_str);
    
    ProtocolMessage msg;
    msg.type = stringToMessageType(j["type"].get<std::string>());
    msg.device_id = j["device_id"].get<std::string>();
    msg.payload = j["payload"];
    msg.timestamp = j["timestamp"].get<std::string>();
    
    return msg;
}

ProtocolMessage createHeartbeat(const std::string& device_id) {
    ProtocolMessage msg;
    msg.type = MessageType::HEARTBEAT;
    msg.device_id = device_id;
    msg.payload = {
        {"status", "alive"}
    };
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

ProtocolMessage createStatusReport(const std::string& device_id, const json& status) {
    ProtocolMessage msg;
    msg.type = MessageType::STATUS_REPORT;
    msg.device_id = device_id;
    msg.payload = status;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

ProtocolMessage createSensorData(const std::string& device_id, const json& data) {
    ProtocolMessage msg;
    msg.type = MessageType::SENSOR_DATA;
    msg.device_id = device_id;
    msg.payload = data;
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

ProtocolMessage createCommandAck(const std::string& device_id, const std::string& command_id, bool success) {
    ProtocolMessage msg;
    msg.type = MessageType::COMMAND_ACK;
    msg.device_id = device_id;
    msg.payload = {
        {"command_id", command_id},
        {"success", success}
    };
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

ProtocolMessage createError(const std::string& device_id, const std::string& error_msg) {
    ProtocolMessage msg;
    msg.type = MessageType::ERROR;
    msg.device_id = device_id;
    msg.payload = {
        {"error", error_msg}
    };
    msg.timestamp = getCurrentTimestamp();
    return msg;
}

} // namespace tc375

