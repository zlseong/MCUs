/**
 * @file unified_message.hpp
 * @brief Unified Message Format for VMG
 * 
 * Integrates OTA project message format with current VMG implementation.
 * Based on: https://github.com/zlseong/OTA-Project-with-PQC-hybrid-TLS.git
 */

#ifndef UNIFIED_MESSAGE_HPP
#define UNIFIED_MESSAGE_HPP

#include <string>
#include <nlohmann/json.hpp>
#include <chrono>
#include <random>

namespace vmg {

using json = nlohmann::json;

/**
 * @brief Message Entity Types
 */
enum class EntityType {
    VMG,
    SERVER,
    ECU
};

/**
 * @brief Message Types (Unified)
 */
enum class MessageType {
    // ECU ↔ VMG
    DEVICE_REGISTRATION,
    DEVICE_REGISTRATION_ACK,
    HEARTBEAT,
    SENSOR_DATA,
    STATUS_REPORT,
    
    // VMG ↔ Server (OTA)
    WAKEUP,
    WAKEUP_ACK,
    REQUEST_VCI,
    VCI_REPORT,
    REQUEST_READINESS,
    READINESS_RESPONSE,
    OTA_DOWNLOAD_PROGRESS,
    OTA_UPDATE_RESULT,
    
    // Common
    COMMAND_ACK,
    ERROR
};

/**
 * @brief Message Source/Target
 */
struct MessageEntity {
    EntityType entity;
    std::string identifier;
    
    json toJson() const {
        return {
            {"entity", entityTypeToString(entity)},
            {"identifier", identifier}
        };
    }
    
    static std::string entityTypeToString(EntityType type) {
        switch (type) {
            case EntityType::VMG: return "VMG";
            case EntityType::SERVER: return "SERVER";
            case EntityType::ECU: return "ECU";
            default: return "UNKNOWN";
        }
    }
};

/**
 * @brief Message Metadata
 */
struct MessageMetadata {
    std::string protocol_version = "1.0";
    std::string encryption = "ML-KEM-768";
    json signature;
    json extra;
    
    json toJson() const {
        json result = {
            {"protocol_version", protocol_version},
            {"encryption", encryption}
        };
        if (!signature.empty()) {
            result["signature"] = signature;
        }
        if (!extra.empty()) {
            result = json::object::merge(result, extra);
        }
        return result;
    }
};

/**
 * @brief Unified Message
 */
class UnifiedMessage {
public:
    UnifiedMessage(MessageType type)
        : message_type_(type),
          message_id_(generateUUID()),
          timestamp_(getCurrentTimestampISO8601()) {}
    
    // Setters
    void setCorrelationId(const std::string& id) { correlation_id_ = id; }
    void setSource(const MessageEntity& src) { source_ = src; }
    void setTarget(const MessageEntity& tgt) { target_ = tgt; }
    void setPayload(const json& payload) { payload_ = payload; }
    void setMetadata(const MessageMetadata& meta) { metadata_ = meta; }
    
    // Getters
    MessageType getMessageType() const { return message_type_; }
    std::string getMessageId() const { return message_id_; }
    std::string getCorrelationId() const { return correlation_id_; }
    std::string getTimestamp() const { return timestamp_; }
    const MessageEntity& getSource() const { return source_; }
    const MessageEntity& getTarget() const { return target_; }
    const json& getPayload() const { return payload_; }
    
    // Serialization
    json toJson() const {
        json result = {
            {"message_type", messageTypeToString(message_type_)},
            {"message_id", message_id_},
            {"timestamp", timestamp_},
            {"source", source_.toJson()},
            {"payload", payload_}
        };
        
        if (!correlation_id_.empty()) {
            result["correlation_id"] = correlation_id_;
        }
        
        if (target_.identifier != "") {
            result["target"] = target_.toJson();
        }
        
        if (!metadata_.protocol_version.empty()) {
            result["metadata"] = metadata_.toJson();
        }
        
        return result;
    }
    
    std::string toString() const {
        return toJson().dump(2);
    }
    
    // Deserialization
    static UnifiedMessage fromJson(const json& j) {
        MessageType type = stringToMessageType(j["message_type"]);
        UnifiedMessage msg(type);
        
        msg.message_id_ = j["message_id"];
        msg.timestamp_ = j["timestamp"];
        
        if (j.contains("correlation_id")) {
            msg.correlation_id_ = j["correlation_id"];
        }
        
        // Parse source
        if (j.contains("source")) {
            msg.source_.entity = EntityType::VMG; // TODO: parse from string
            msg.source_.identifier = j["source"]["identifier"];
        }
        
        // Parse target
        if (j.contains("target")) {
            msg.target_.entity = EntityType::SERVER;
            msg.target_.identifier = j["target"]["identifier"];
        }
        
        msg.payload_ = j["payload"];
        
        return msg;
    }

private:
    MessageType message_type_;
    std::string message_id_;
    std::string correlation_id_;
    std::string timestamp_;
    MessageEntity source_;
    MessageEntity target_;
    json payload_;
    MessageMetadata metadata_;
    
    static std::string generateUUID() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);
        
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++) ss << dis(gen);
        ss << "-4"; // UUID version 4
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; i++) ss << dis(gen);
        return ss.str();
    }
    
    static std::string getCurrentTimestampISO8601() {
        auto now = std::chrono::system_clock::now();
        auto itt = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&itt), "%Y-%m-%dT%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
        return ss.str();
    }
    
    static std::string messageTypeToString(MessageType type) {
        switch (type) {
            case MessageType::DEVICE_REGISTRATION: return "DEVICE_REGISTRATION";
            case MessageType::DEVICE_REGISTRATION_ACK: return "DEVICE_REGISTRATION_ACK";
            case MessageType::HEARTBEAT: return "HEARTBEAT";
            case MessageType::SENSOR_DATA: return "SENSOR_DATA";
            case MessageType::STATUS_REPORT: return "STATUS_REPORT";
            case MessageType::WAKEUP: return "WAKEUP";
            case MessageType::WAKEUP_ACK: return "WAKEUP_ACK";
            case MessageType::REQUEST_VCI: return "REQUEST_VCI";
            case MessageType::VCI_REPORT: return "VCI_REPORT";
            case MessageType::REQUEST_READINESS: return "REQUEST_READINESS";
            case MessageType::READINESS_RESPONSE: return "READINESS_RESPONSE";
            case MessageType::OTA_DOWNLOAD_PROGRESS: return "OTA_DOWNLOAD_PROGRESS";
            case MessageType::OTA_UPDATE_RESULT: return "OTA_UPDATE_RESULT";
            case MessageType::COMMAND_ACK: return "COMMAND_ACK";
            case MessageType::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    static MessageType stringToMessageType(const std::string& str) {
        if (str == "DEVICE_REGISTRATION") return MessageType::DEVICE_REGISTRATION;
        if (str == "DEVICE_REGISTRATION_ACK") return MessageType::DEVICE_REGISTRATION_ACK;
        if (str == "HEARTBEAT") return MessageType::HEARTBEAT;
        if (str == "SENSOR_DATA") return MessageType::SENSOR_DATA;
        if (str == "STATUS_REPORT") return MessageType::STATUS_REPORT;
        if (str == "WAKEUP") return MessageType::WAKEUP;
        if (str == "WAKEUP_ACK") return MessageType::WAKEUP_ACK;
        if (str == "REQUEST_VCI") return MessageType::REQUEST_VCI;
        if (str == "VCI_REPORT") return MessageType::VCI_REPORT;
        if (str == "REQUEST_READINESS") return MessageType::REQUEST_READINESS;
        if (str == "READINESS_RESPONSE") return MessageType::READINESS_RESPONSE;
        if (str == "OTA_DOWNLOAD_PROGRESS") return MessageType::OTA_DOWNLOAD_PROGRESS;
        if (str == "OTA_UPDATE_RESULT") return MessageType::OTA_UPDATE_RESULT;
        if (str == "COMMAND_ACK") return MessageType::COMMAND_ACK;
        if (str == "ERROR") return MessageType::ERROR;
        return MessageType::ERROR;
    }
};

/**
 * @brief Message Builder (Factory)
 */
class MessageBuilder {
public:
    // ECU → VMG
    static UnifiedMessage createDeviceRegistration(
        const std::string& ecu_serial,
        const json& device_info) {
        
        UnifiedMessage msg(MessageType::DEVICE_REGISTRATION);
        msg.setSource({EntityType::ECU, ecu_serial});
        msg.setTarget({EntityType::VMG, "VMG-001"});
        msg.setPayload({
            {"device_info", device_info}
        });
        return msg;
    }
    
    static UnifiedMessage createHeartbeat(const std::string& device_id) {
        UnifiedMessage msg(MessageType::HEARTBEAT);
        msg.setSource({EntityType::ECU, device_id});
        msg.setPayload({
            {"status", "alive"}
        });
        return msg;
    }
    
    static UnifiedMessage createStatusReport(
        const std::string& device_id,
        const json& status) {
        
        UnifiedMessage msg(MessageType::STATUS_REPORT);
        msg.setSource({EntityType::ECU, device_id});
        msg.setPayload(status);
        return msg;
    }
    
    // VMG → Server
    static UnifiedMessage createWakeup(
        const std::string& vin,
        const json& vehicle_info) {
        
        UnifiedMessage msg(MessageType::WAKEUP);
        msg.setSource({EntityType::VMG, "VMG-001"});
        msg.setTarget({EntityType::SERVER, "OTA-SERVER-001"});
        
        json payload = {
            {"vin", vin},
            {"wakeup_reason", "ignition_on"}
        };
        payload.update(vehicle_info);
        
        msg.setPayload(payload);
        return msg;
    }
    
    static UnifiedMessage createVCIReport(
        const std::string& correlation_id,
        const json& vci_data) {
        
        UnifiedMessage msg(MessageType::VCI_REPORT);
        msg.setCorrelationId(correlation_id);
        msg.setSource({EntityType::VMG, "VMG-001"});
        msg.setTarget({EntityType::SERVER, "OTA-SERVER-001"});
        msg.setPayload(vci_data);
        return msg;
    }
    
    static UnifiedMessage createReadinessResponse(
        const std::string& correlation_id,
        const std::string& campaign_id,
        const std::string& status,
        const json& checks) {
        
        UnifiedMessage msg(MessageType::READINESS_RESPONSE);
        msg.setCorrelationId(correlation_id);
        msg.setSource({EntityType::VMG, "VMG-001"});
        msg.setTarget({EntityType::SERVER, "OTA-SERVER-001"});
        msg.setPayload({
            {"campaign_id", campaign_id},
            {"readiness_status", status},
            {"checks", checks}
        });
        return msg;
    }
    
    static UnifiedMessage createOTAProgress(
        const std::string& campaign_id,
        const std::string& package_id,
        int progress_percentage,
        uint64_t bytes_downloaded,
        uint64_t total_bytes) {
        
        UnifiedMessage msg(MessageType::OTA_DOWNLOAD_PROGRESS);
        msg.setSource({EntityType::VMG, "VMG-001"});
        msg.setPayload({
            {"campaign_id", campaign_id},
            {"package_id", package_id},
            {"status", "downloading"},
            {"progress_percentage", progress_percentage},
            {"bytes_downloaded", bytes_downloaded},
            {"total_bytes", total_bytes}
        });
        return msg;
    }
    
    static UnifiedMessage createOTAResult(
        const std::string& campaign_id,
        const std::string& overall_status,
        const json& ecus_result) {
        
        UnifiedMessage msg(MessageType::OTA_UPDATE_RESULT);
        msg.setSource({EntityType::VMG, "VMG-001"});
        msg.setTarget({EntityType::SERVER, "OTA-SERVER-001"});
        msg.setPayload({
            {"campaign_id", campaign_id},
            {"overall_status", overall_status},
            {"ecus", ecus_result}
        });
        return msg;
    }
    
    // Error
    static UnifiedMessage createError(
        const std::string& correlation_id,
        const std::string& error_code,
        const std::string& message,
        const json& details = {}) {
        
        UnifiedMessage msg(MessageType::ERROR);
        if (!correlation_id.empty()) {
            msg.setCorrelationId(correlation_id);
        }
        msg.setPayload({
            {"error_code", error_code},
            {"error_category", "system"},
            {"severity", "error"},
            {"message", message},
            {"details", details}
        });
        return msg;
    }
};

} // namespace vmg

#endif // UNIFIED_MESSAGE_HPP

