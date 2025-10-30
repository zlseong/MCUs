/**
 * @file uds_service_handler.cpp
 * @brief UDS Service Handler Implementation
 */

#include "uds_service_handler.hpp"
#include <iostream>
#include <random>
#include <cstring>

namespace vmg {

constexpr uint8_t UDS_POSITIVE_RESPONSE_OFFSET = 0x40;
constexpr uint8_t UDS_NEGATIVE_RESPONSE = 0x7F;

UDSServiceHandler::UDSServiceHandler()
    : vin_("WBADT43452G296403"),
      ecu_serial_("ECU123456789"),
      software_version_("v1.0.0"),
      hardware_version_("HW_REV_A"),
      current_session_(0x01),
      security_unlocked_(false),
      security_seed_(0),
      security_attempts_(0) {
}

std::vector<uint8_t> UDSServiceHandler::processRequest(const std::vector<uint8_t>& request) {
    if (request.empty()) {
        return buildNegativeResponse(0x00, UDSNRC::IncorrectMessageLength);
    }

    uint8_t sid = request[0];

    try {
        switch (static_cast<UDSServiceID>(sid)) {
            case UDSServiceID::DiagnosticSessionControl:
                return handleDiagnosticSessionControl(request);

            case UDSServiceID::ECUReset:
                return handleECUReset(request);

            case UDSServiceID::SecurityAccess:
                return handleSecurityAccess(request);

            case UDSServiceID::TesterPresent:
                return handleTesterPresent(request);

            case UDSServiceID::ReadDataByIdentifier:
                return handleReadDataByIdentifier(request);

            case UDSServiceID::WriteDataByIdentifier:
                return handleWriteDataByIdentifier(request);

            case UDSServiceID::ReadDTCInformation:
                return handleReadDTCInformation(request);

            case UDSServiceID::RoutineControl:
                return handleRoutineControl(request);

            default:
                return buildNegativeResponse(sid, UDSNRC::ServiceNotSupported);
        }
    } catch (const std::exception& e) {
        std::cerr << "UDS service error: " << e.what() << std::endl;
        return buildNegativeResponse(sid, UDSNRC::GeneralReject);
    }
}

void UDSServiceHandler::setVIN(const std::string& vin) {
    vin_ = vin;
}

void UDSServiceHandler::setECUSerialNumber(const std::string& serial) {
    ecu_serial_ = serial;
}

void UDSServiceHandler::setSoftwareVersion(const std::string& version) {
    software_version_ = version;
}

void UDSServiceHandler::setHardwareVersion(const std::string& version) {
    hardware_version_ = version;
}

void UDSServiceHandler::registerDIDReadHandler(uint16_t did, DIDHandler handler) {
    did_handlers_[did] = handler;
}

// ============================================================================
// Service Handlers
// ============================================================================

std::vector<uint8_t> UDSServiceHandler::handleDiagnosticSessionControl(
    const std::vector<uint8_t>& request) {
    
    if (request.size() < 2) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    uint8_t session_type = request[1];

    // Validate session type
    if (session_type < 0x01 || session_type > 0x03) {
        return buildNegativeResponse(request[0], UDSNRC::SubFunctionNotSupported);
    }

    current_session_ = session_type;
    
    // Reset security on session change
    if (session_type == 0x01) {  // Default session
        security_unlocked_ = false;
    }

    std::cout << "Session control: type=0x" << std::hex 
              << static_cast<int>(session_type) << std::dec << std::endl;

    // Response: echo session type
    return buildPositiveResponse(request[0], {session_type});
}

std::vector<uint8_t> UDSServiceHandler::handleECUReset(const std::vector<uint8_t>& request) {
    if (request.size() < 2) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    uint8_t reset_type = request[1];

    // Validate reset type (0x01 = hard, 0x02 = key off/on, 0x03 = soft)
    if (reset_type < 0x01 || reset_type > 0x03) {
        return buildNegativeResponse(request[0], UDSNRC::SubFunctionNotSupported);
    }

    std::cout << "ECU reset requested: type=0x" << std::hex 
              << static_cast<int>(reset_type) << std::dec << std::endl;

    // In gateway, we don't actually reset - just acknowledge
    return buildPositiveResponse(request[0], {reset_type});
}

std::vector<uint8_t> UDSServiceHandler::handleSecurityAccess(const std::vector<uint8_t>& request) {
    if (request.size() < 2) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    uint8_t sub_function = request[1];

    if (sub_function == 0x01) {
        // Request seed
        if (security_unlocked_) {
            // Already unlocked, return seed = 0
            return buildPositiveResponse(request[0], {0x01, 0x00, 0x00, 0x00, 0x00});
        }

        // Check attempt limit
        if (security_attempts_ >= 3) {
            return buildNegativeResponse(request[0], UDSNRC::ExceedNumberOfAttempts);
        }

        // Generate random seed
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0x10000000, 0xFFFFFFFF);
        security_seed_ = dis(gen);

        std::vector<uint8_t> response_data = {0x01};
        response_data.push_back((security_seed_ >> 24) & 0xFF);
        response_data.push_back((security_seed_ >> 16) & 0xFF);
        response_data.push_back((security_seed_ >> 8) & 0xFF);
        response_data.push_back(security_seed_ & 0xFF);

        std::cout << "Security access: seed=0x" << std::hex << security_seed_ << std::dec << std::endl;

        return buildPositiveResponse(request[0], response_data);

    } else if (sub_function == 0x02) {
        // Send key
        if (request.size() < 6) {
            return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
        }

        uint32_t received_key = (static_cast<uint32_t>(request[2]) << 24) |
                                 (static_cast<uint32_t>(request[3]) << 16) |
                                 (static_cast<uint32_t>(request[4]) << 8) |
                                 static_cast<uint32_t>(request[5]);

        // Simple key algorithm: key = seed XOR 0xABCD1234 (EXAMPLE ONLY)
        uint32_t expected_key = security_seed_ ^ 0xABCD1234;

        if (received_key == expected_key) {
            security_unlocked_ = true;
            security_attempts_ = 0;
            std::cout << "Security access: UNLOCKED" << std::endl;
            return buildPositiveResponse(request[0], {0x02});
        } else {
            security_attempts_++;
            std::cout << "Security access: INVALID KEY (attempt " 
                      << static_cast<int>(security_attempts_) << ")" << std::endl;
            
            if (security_attempts_ >= 3) {
                return buildNegativeResponse(request[0], UDSNRC::ExceedNumberOfAttempts);
            }
            return buildNegativeResponse(request[0], UDSNRC::InvalidKey);
        }
    }

    return buildNegativeResponse(request[0], UDSNRC::SubFunctionNotSupported);
}

std::vector<uint8_t> UDSServiceHandler::handleTesterPresent(const std::vector<uint8_t>& request) {
    if (request.size() < 2) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    uint8_t sub_function = request[1];
    return buildPositiveResponse(request[0], {sub_function});
}

std::vector<uint8_t> UDSServiceHandler::handleReadDataByIdentifier(
    const std::vector<uint8_t>& request) {
    
    if (request.size() < 3) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    uint16_t did = (static_cast<uint16_t>(request[1]) << 8) | request[2];

    std::cout << "Read DID: 0x" << std::hex << did << std::dec << std::endl;

    // Check custom handlers first
    auto it = did_handlers_.find(did);
    if (it != did_handlers_.end()) {
        try {
            std::vector<uint8_t> data = it->second(did);
            std::vector<uint8_t> response_data = {
                static_cast<uint8_t>(did >> 8),
                static_cast<uint8_t>(did & 0xFF)
            };
            response_data.insert(response_data.end(), data.begin(), data.end());
            return buildPositiveResponse(request[0], response_data);
        } catch (const std::exception& e) {
            std::cerr << "DID handler error: " << e.what() << std::endl;
            return buildNegativeResponse(request[0], UDSNRC::RequestOutOfRange);
        }
    }

    // Built-in DIDs
    std::string data_str;
    switch (static_cast<UDSDID>(did)) {
        case UDSDID::VIN:
            data_str = vin_;
            data_str.resize(17, ' ');  // Pad to 17 bytes
            break;

        case UDSDID::ECUSerialNumber:
            data_str = ecu_serial_;
            break;

        case UDSDID::ECUSoftwareVersion:
            data_str = software_version_;
            break;

        case UDSDID::ECUHardwareVersion:
            data_str = hardware_version_;
            break;

        default:
            return buildNegativeResponse(request[0], UDSNRC::RequestOutOfRange);
    }

    std::vector<uint8_t> response_data = {
        static_cast<uint8_t>(did >> 8),
        static_cast<uint8_t>(did & 0xFF)
    };
    response_data.insert(response_data.end(), data_str.begin(), data_str.end());

    return buildPositiveResponse(request[0], response_data);
}

std::vector<uint8_t> UDSServiceHandler::handleWriteDataByIdentifier(
    const std::vector<uint8_t>& request) {
    
    if (request.size() < 4) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    // Check security
    if (!security_unlocked_) {
        return buildNegativeResponse(request[0], UDSNRC::SecurityAccessDenied);
    }

    uint16_t did = (static_cast<uint16_t>(request[1]) << 8) | request[2];

    std::cout << "Write DID: 0x" << std::hex << did << std::dec << std::endl;

    // Echo DID in response
    return buildPositiveResponse(request[0], {
        static_cast<uint8_t>(did >> 8),
        static_cast<uint8_t>(did & 0xFF)
    });
}

std::vector<uint8_t> UDSServiceHandler::handleReadDTCInformation(
    const std::vector<uint8_t>& request) {
    
    if (request.size() < 2) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    uint8_t sub_function = request[1];

    std::cout << "Read DTC: sub=0x" << std::hex << static_cast<int>(sub_function) << std::dec << std::endl;

    // Return empty DTC list (no faults)
    return buildPositiveResponse(request[0], {sub_function, 0x00});
}

std::vector<uint8_t> UDSServiceHandler::handleRoutineControl(
    const std::vector<uint8_t>& request) {
    
    if (request.size() < 4) {
        return buildNegativeResponse(request[0], UDSNRC::IncorrectMessageLength);
    }

    uint8_t sub_function = request[1];
    uint16_t routine_id = (static_cast<uint16_t>(request[2]) << 8) | request[3];

    std::cout << "Routine control: sub=0x" << std::hex << static_cast<int>(sub_function)
              << ", routine=0x" << routine_id << std::dec << std::endl;

    // Echo sub-function and routine ID
    return buildPositiveResponse(request[0], {
        sub_function,
        static_cast<uint8_t>(routine_id >> 8),
        static_cast<uint8_t>(routine_id & 0xFF)
    });
}

// ============================================================================
// Response Builders
// ============================================================================

std::vector<uint8_t> UDSServiceHandler::buildPositiveResponse(
    uint8_t sid, const std::vector<uint8_t>& data) {
    
    std::vector<uint8_t> response;
    response.push_back(sid + UDS_POSITIVE_RESPONSE_OFFSET);
    response.insert(response.end(), data.begin(), data.end());
    return response;
}

std::vector<uint8_t> UDSServiceHandler::buildNegativeResponse(uint8_t sid, UDSNRC nrc) {
    return {UDS_NEGATIVE_RESPONSE, sid, static_cast<uint8_t>(nrc)};
}

} // namespace vmg

