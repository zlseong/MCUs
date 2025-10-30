/**
 * @file example_unified_message.cpp
 * @brief Unified Message Format Usage Example
 * 
 * Demonstrates how to use the unified message format for
 * ECU ↔ VMG ↔ Server communication.
 */

#include "include/unified_message.hpp"
#include <iostream>

using namespace vmg;

void printMessage(const std::string& title, const UnifiedMessage& msg) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    std::cout << msg.toString() << std::endl;
}

int main() {
    std::cout << "╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Unified Message Format Examples      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    
    // ========================================
    // 1. ECU → VMG: Device Registration
    // ========================================
    json device_info = {
        {"ecu_serial", "TC375-SIM-001-20251030"},
        {"mac_address", "02:00:00:AA:BB:CC"},
        {"hardware_version", "TC375TP-LiteKit-v2.0"},
        {"vin", "KMHGH4JH1NU123456"},
        {"vehicle_model", "Genesis G80 EV"},
        {"vehicle_year", 2025},
        {"firmware_version", "1.0.0"},
        {"bootloader_version", "1.0.0"}
    };
    
    auto registration = MessageBuilder::createDeviceRegistration(
        "TC375-SIM-001-20251030",
        device_info
    );
    printMessage("ECU → VMG: Device Registration", registration);
    
    // ========================================
    // 2. ECU → VMG: Heartbeat
    // ========================================
    auto heartbeat = MessageBuilder::createHeartbeat("TC375-SIM-001-20251030");
    printMessage("ECU → VMG: Heartbeat", heartbeat);
    
    // ========================================
    // 3. ECU → VMG: Status Report
    // ========================================
    json status = {
        {"system", {
            {"uptime", 3600},
            {"cpu_usage", 45.2},
            {"memory_free", 2048}
        }},
        {"firmware", {
            {"active_bank", "A"},
            {"firmware_version", "1.0.0"}
        }}
    };
    
    auto status_report = MessageBuilder::createStatusReport(
        "TC375-SIM-001-20251030",
        status
    );
    printMessage("ECU → VMG: Status Report", status_report);
    
    // ========================================
    // 4. VMG → Server: Wakeup
    // ========================================
    json vehicle_info = {
        {"vehicle_model", "Genesis G80 EV"},
        {"vehicle_year", 2025},
        {"location", {
            {"latitude", 37.5665},
            {"longitude", 126.9780},
            {"country", "KR"}
        }},
        {"current_versions", {
            {"vmg_version", "2.0.0"},
            {"ecu_versions", json::array({
                {
                    {"ecu_id", "TC375-SIM-001-20251030"},
                    {"firmware_version", "1.0.0"}
                }
            })}
        }}
    };
    
    auto wakeup = MessageBuilder::createWakeup("KMHGH4JH1NU123456", vehicle_info);
    printMessage("VMG → Server: Wakeup", wakeup);
    
    // ========================================
    // 5. Server → VMG: Request VCI (Simulated)
    // ========================================
    UnifiedMessage request_vci(MessageType::REQUEST_VCI);
    request_vci.setSource({EntityType::SERVER, "OTA-SERVER-001"});
    request_vci.setTarget({EntityType::VMG, "VMG-001"});
    request_vci.setPayload({
        {"request_type", "full"},
        {"include_sections", json::array({"hardware", "software", "configuration"})}
    });
    printMessage("Server → VMG: Request VCI", request_vci);
    
    // ========================================
    // 6. VMG → Server: VCI Report
    // ========================================
    json vci_data = {
        {"vin", "KMHGH4JH1NU123456"},
        {"vehicle_info", {
            {"model", "Genesis G80 EV"},
            {"year", 2025},
            {"region", "KR"}
        }},
        {"ecus", json::array({
            {
                {"ecu_id", "TC375-SIM-001-20251030"},
                {"ecu_type", "Engine_Controller"},
                {"hardware_version", "TC375TP-LiteKit-v2.0"},
                {"firmware_version", "1.0.0"},
                {"capabilities", {
                    {"ota_capable", true},
                    {"delta_update", true}
                }}
            }
        })}
    };
    
    auto vci_report = MessageBuilder::createVCIReport(
        request_vci.getMessageId(),  // correlation_id
        vci_data
    );
    printMessage("VMG → Server: VCI Report", vci_report);
    
    // ========================================
    // 7. Server → VMG: Request Readiness (Simulated)
    // ========================================
    UnifiedMessage request_readiness(MessageType::REQUEST_READINESS);
    request_readiness.setSource({EntityType::SERVER, "OTA-SERVER-001"});
    request_readiness.setTarget({EntityType::VMG, "VMG-001"});
    request_readiness.setPayload({
        {"campaign_id", "OTA-2025-001"},
        {"update_packages", json::array({
            {
                {"ecu_id", "TC375-SIM-001-20251030"},
                {"package_id", "PKG-ENGINE-v1.1.0"},
                {"from_version", "1.0.0"},
                {"to_version", "1.1.0"},
                {"package_size_bytes", 10485760}
            }
        })}
    });
    printMessage("Server → VMG: Request Readiness", request_readiness);
    
    // ========================================
    // 8. VMG → Server: Readiness Response (Ready)
    // ========================================
    json checks = {
        {"battery_level", 85},
        {"available_storage_mb", 256},
        {"vehicle_state", "parked"},
        {"network_quality", "excellent"},
        {"user_consent", true}
    };
    
    auto readiness_ready = MessageBuilder::createReadinessResponse(
        request_readiness.getMessageId(),
        "OTA-2025-001",
        "ready",
        checks
    );
    printMessage("VMG → Server: Readiness Response (Ready)", readiness_ready);
    
    // ========================================
    // 9. VMG → Server: Readiness Response (Not Ready)
    // ========================================
    json checks_not_ready = {
        {"battery_level", 30},
        {"blocked_by", json::array({
            {
                {"check", "battery_level"},
                {"current_value", 30},
                {"required_value", 50},
                {"message", "Battery level too low"}
            }
        })},
        {"retry_after_sec", 600}
    };
    
    auto readiness_not_ready = MessageBuilder::createReadinessResponse(
        request_readiness.getMessageId(),
        "OTA-2025-001",
        "not_ready",
        checks_not_ready
    );
    printMessage("VMG → Server: Readiness Response (Not Ready)", readiness_not_ready);
    
    // ========================================
    // 10. VMG → Server: OTA Download Progress
    // ========================================
    auto ota_progress = MessageBuilder::createOTAProgress(
        "OTA-2025-001",
        "PKG-ENGINE-v1.1.0",
        45,                 // 45%
        4718592,            // ~4.5MB
        10485760            // 10MB
    );
    printMessage("VMG → Server: OTA Download Progress", ota_progress);
    
    // ========================================
    // 11. VMG → Server: OTA Update Result (Success)
    // ========================================
    json ecus_result_success = json::array({
        {
            {"ecu_id", "TC375-SIM-001-20251030"},
            {"package_id", "PKG-ENGINE-v1.1.0"},
            {"status", "success"},
            {"previous_version", "1.0.0"},
            {"current_version", "1.1.0"},
            {"verification_status", "passed"},
            {"rollback_performed", false}
        }
    });
    
    auto ota_result_success = MessageBuilder::createOTAResult(
        "OTA-2025-001",
        "success",
        ecus_result_success
    );
    printMessage("VMG → Server: OTA Result (Success)", ota_result_success);
    
    // ========================================
    // 12. VMG → Server: OTA Update Result (Failed)
    // ========================================
    json ecus_result_failed = json::array({
        {
            {"ecu_id", "TC375-SIM-001-20251030"},
            {"status", "failed"},
            {"error_code", "ERR_VERIFICATION_FAILED"},
            {"error_message", "Firmware signature verification failed"},
            {"rollback_performed", true},
            {"current_version", "1.0.0"}
        }
    });
    
    auto ota_result_failed = MessageBuilder::createOTAResult(
        "OTA-2025-001",
        "failed",
        ecus_result_failed
    );
    printMessage("VMG → Server: OTA Result (Failed)", ota_result_failed);
    
    // ========================================
    // 13. Error Message
    // ========================================
    json error_details = {
        {"target_ecu", "TC375-SIM-001-20251030"},
        {"last_response_time", "2025-10-30T15:29:30Z"},
        {"retry_count", 3}
    };
    
    auto error_msg = MessageBuilder::createError(
        "some-request-id",
        "ERR_CONNECTION_TIMEOUT",
        "Connection to ECU timed out after 30 seconds",
        error_details
    );
    printMessage("Error Message", error_msg);
    
    // ========================================
    // 14. Deserialization Test
    // ========================================
    std::cout << "\n=== Deserialization Test ===" << std::endl;
    std::string json_str = heartbeat.toString();
    std::cout << "Original JSON:\n" << json_str << std::endl;
    
    json parsed = json::parse(json_str);
    auto deserialized = UnifiedMessage::fromJson(parsed);
    std::cout << "\nDeserialized back:\n" << deserialized.toString() << std::endl;
    
    std::cout << "\n✅ All examples completed successfully!" << std::endl;
    
    return 0;
}

