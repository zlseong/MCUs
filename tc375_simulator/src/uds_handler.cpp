#include "uds_handler.hpp"
#include <iostream>
#include <random>
#include <cstring>

namespace tc375 {

// UDS Message Serialization
std::vector<uint8_t> UdsMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(service));
    if (sub_function != 0) {
        result.push_back(sub_function);
    }
    result.insert(result.end(), data.begin(), data.end());
    return result;
}

UdsMessage UdsMessage::deserialize(const std::vector<uint8_t>& raw) {
    UdsMessage msg;
    if (raw.empty()) {
        throw std::runtime_error("Empty UDS message");
    }
    
    msg.service = static_cast<UdsService>(raw[0]);
    msg.sub_function = (raw.size() > 1) ? raw[1] : 0;
    if (raw.size() > 2) {
        msg.data.assign(raw.begin() + 2, raw.end());
    }
    return msg;
}

// UDS Response Serialization
std::vector<uint8_t> UdsResponse::serialize() const {
    std::vector<uint8_t> result;
    
    if (positive) {
        // Positive response: 0x40 + ServiceID
        result.push_back(0x40 + static_cast<uint8_t>(service));
    } else {
        // Negative response: 0x7F + ServiceID + NRC
        result.push_back(0x7F);
        result.push_back(static_cast<uint8_t>(service));
        result.push_back(static_cast<uint8_t>(nrc));
    }
    
    result.insert(result.end(), data.begin(), data.end());
    return result;
}

UdsResponse UdsResponse::deserialize(const std::vector<uint8_t>& raw) {
    UdsResponse resp;
    if (raw.empty()) {
        throw std::runtime_error("Empty UDS response");
    }
    
    if (raw[0] == 0x7F) {
        // Negative response
        resp.positive = false;
        if (raw.size() >= 3) {
            resp.service = static_cast<UdsService>(raw[1]);
            resp.nrc = static_cast<NRC>(raw[2]);
        }
        if (raw.size() > 3) {
            resp.data.assign(raw.begin() + 3, raw.end());
        }
    } else {
        // Positive response
        resp.positive = true;
        resp.service = static_cast<UdsService>(raw[0] - 0x40);
        resp.nrc = NRC::POSITIVE_RESPONSE;
        if (raw.size() > 1) {
            resp.data.assign(raw.begin() + 1, raw.end());
        }
    }
    
    return resp;
}

// UDS Handler Implementation
UdsHandler::UdsHandler()
    : security_level_(SecurityLevel::LOCKED)
    , current_seed_(0)
    , current_session_(DiagnosticSession::DEFAULT)
{
    download_state_.active = false;
    download_state_.block_counter = 0;
    
    // Register default handlers
    registerServiceHandler(UdsService::DIAGNOSTIC_SESSION_CONTROL, 
        [this](const UdsMessage& msg) { return handleDiagnosticSession(msg); });
    registerServiceHandler(UdsService::ECU_RESET, 
        [this](const UdsMessage& msg) { return handleEcuReset(msg); });
    registerServiceHandler(UdsService::SECURITY_ACCESS, 
        [this](const UdsMessage& msg) { return handleSecurityAccess(msg); });
    registerServiceHandler(UdsService::TESTER_PRESENT, 
        [this](const UdsMessage& msg) { return handleTesterPresent(msg); });
    registerServiceHandler(UdsService::READ_DATA_BY_ID, 
        [this](const UdsMessage& msg) { return handleReadDataById(msg); });
    registerServiceHandler(UdsService::REQUEST_DOWNLOAD, 
        [this](const UdsMessage& msg) { return handleRequestDownload(msg); });
    registerServiceHandler(UdsService::TRANSFER_DATA, 
        [this](const UdsMessage& msg) { return handleTransferData(msg); });
    registerServiceHandler(UdsService::REQUEST_TRANSFER_EXIT, 
        [this](const UdsMessage& msg) { return handleRequestTransferExit(msg); });
}

UdsResponse UdsHandler::handleRequest(const UdsMessage& request) {
    std::cout << "[UDS] Handling service: 0x" << std::hex 
              << static_cast<int>(request.service) << std::dec << std::endl;

    auto it = service_handlers_.find(request.service);
    if (it == service_handlers_.end()) {
        return createNegativeResponse(request.service, NRC::SERVICE_NOT_SUPPORTED);
    }

    return it->second(request);
}

void UdsHandler::registerServiceHandler(UdsService service, ServiceHandler handler) {
    service_handlers_[service] = handler;
}

UdsResponse UdsHandler::handleDiagnosticSession(const UdsMessage& request) {
    if (request.data.empty()) {
        return createNegativeResponse(request.service, NRC::INCORRECT_MESSAGE_LENGTH);
    }

    uint8_t session_type = request.data[0];
    
    switch (session_type) {
        case 0x01: // Default session
            current_session_ = DiagnosticSession::DEFAULT;
            break;
        case 0x02: // Programming session
            if (security_level_ != SecurityLevel::UNLOCKED) {
                return createNegativeResponse(request.service, NRC::SECURITY_ACCESS_DENIED);
            }
            current_session_ = DiagnosticSession::PROGRAMMING;
            break;
        case 0x03: // Extended diagnostic
            current_session_ = DiagnosticSession::EXTENDED;
            break;
        default:
            return createNegativeResponse(request.service, NRC::SUBFUNCTION_NOT_SUPPORTED);
    }

    std::cout << "[UDS] Session changed to: " << static_cast<int>(session_type) << std::endl;
    return createPositiveResponse(request.service, {session_type});
}

UdsResponse UdsHandler::handleEcuReset(const UdsMessage& request) {
    if (request.data.empty()) {
        return createNegativeResponse(request.service, NRC::INCORRECT_MESSAGE_LENGTH);
    }

    uint8_t reset_type = request.data[0];
    std::cout << "[UDS] ECU Reset requested: type=" << static_cast<int>(reset_type) << std::endl;

    // In real implementation:
    // 0x01: Hard reset
    // 0x02: Key off/on reset
    // 0x03: Soft reset
    
    return createPositiveResponse(request.service, {reset_type});
}

UdsResponse UdsHandler::handleSecurityAccess(const UdsMessage& request) {
    if (request.data.empty()) {
        return createNegativeResponse(request.service, NRC::INCORRECT_MESSAGE_LENGTH);
    }

    uint8_t sub_func = request.data[0];
    
    if ((sub_func & 0x01) == 0x01) {
        // Request seed (odd sub-functions)
        current_seed_ = generateSeed();
        security_level_ = SecurityLevel::SEED_SENT;
        
        std::vector<uint8_t> seed_data(4);
        seed_data[0] = (current_seed_ >> 24) & 0xFF;
        seed_data[1] = (current_seed_ >> 16) & 0xFF;
        seed_data[2] = (current_seed_ >> 8) & 0xFF;
        seed_data[3] = current_seed_ & 0xFF;
        
        std::cout << "[UDS] Seed sent: 0x" << std::hex << current_seed_ << std::dec << std::endl;
        return createPositiveResponse(request.service, seed_data);
        
    } else {
        // Send key (even sub-functions)
        if (security_level_ != SecurityLevel::SEED_SENT) {
            return createNegativeResponse(request.service, NRC::GENERAL_REJECT);
        }
        
        if (request.data.size() < 5) {
            return createNegativeResponse(request.service, NRC::INCORRECT_MESSAGE_LENGTH);
        }
        
        uint32_t key = (request.data[1] << 24) | (request.data[2] << 16) | 
                       (request.data[3] << 8) | request.data[4];
        
        if (verifySeedKey(current_seed_, key)) {
            security_level_ = SecurityLevel::UNLOCKED;
            std::cout << "[UDS] Security unlocked!" << std::endl;
            return createPositiveResponse(request.service, {sub_func});
        } else {
            security_level_ = SecurityLevel::LOCKED;
            std::cout << "[UDS] Invalid key!" << std::endl;
            return createNegativeResponse(request.service, NRC::INVALID_KEY);
        }
    }
}

UdsResponse UdsHandler::handleTesterPresent(const UdsMessage& request) {
    // Keep diagnostic session alive
    return createPositiveResponse(request.service, {0x00});
}

UdsResponse UdsHandler::handleReadDataById(const UdsMessage& request) {
    if (request.data.size() < 2) {
        return createNegativeResponse(request.service, NRC::INCORRECT_MESSAGE_LENGTH);
    }

    uint16_t data_id = (request.data[0] << 8) | request.data[1];
    
    // Example DIDs (Data Identifiers)
    switch (data_id) {
        case 0xF186: // Active Diagnostic Session
            return createPositiveResponse(request.service, {
                static_cast<uint8_t>(data_id >> 8), 
                static_cast<uint8_t>(data_id & 0xFF),
                static_cast<uint8_t>(current_session_)
            });
            
        case 0xF187: // ECU Manufacturing Date
            return createPositiveResponse(request.service, {
                static_cast<uint8_t>(data_id >> 8), 
                static_cast<uint8_t>(data_id & 0xFF),
                '2', '0', '2', '5', '1', '0', '2', '1'  // 20251021
            });
            
        case 0xF18A: // ECU Serial Number
            return createPositiveResponse(request.service, {
                static_cast<uint8_t>(data_id >> 8), 
                static_cast<uint8_t>(data_id & 0xFF),
                'T', 'C', '3', '7', '5', '-', '0', '0', '1'
            });
            
        default:
            return createNegativeResponse(request.service, NRC::REQUEST_OUT_OF_RANGE);
    }
}

UdsResponse UdsHandler::handleWriteDataById(const UdsMessage& request) {
    if (current_session_ != DiagnosticSession::PROGRAMMING && 
        current_session_ != DiagnosticSession::EXTENDED) {
        return createNegativeResponse(request.service, NRC::GENERAL_REJECT);
    }
    
    // Write data logic here
    return createPositiveResponse(request.service);
}

UdsResponse UdsHandler::handleRoutineControl(const UdsMessage& request) {
    // Routine control (e.g., self-test, erase memory)
    return createPositiveResponse(request.service);
}

UdsResponse UdsHandler::handleRequestDownload(const UdsMessage& request) {
    if (current_session_ != DiagnosticSession::PROGRAMMING) {
        return createNegativeResponse(request.service, NRC::GENERAL_REJECT);
    }
    
    if (security_level_ != SecurityLevel::UNLOCKED) {
        return createNegativeResponse(request.service, NRC::SECURITY_ACCESS_DENIED);
    }
    
    // Parse download request
    // Format: [dataFormatId] [addressAndLengthFormatId] [memoryAddress] [memorySize]
    
    download_state_.active = true;
    download_state_.block_counter = 1;
    download_state_.address = 0;  // Parse from request
    download_state_.size = 0;     // Parse from request
    
    std::cout << "[UDS] Download session started" << std::endl;
    
    // Response: max block length
    uint16_t max_block = 0x1000;  // 4KB blocks
    return createPositiveResponse(request.service, {
        0x20,  // lengthFormatIdentifier
        static_cast<uint8_t>(max_block >> 8),
        static_cast<uint8_t>(max_block & 0xFF)
    });
}

UdsResponse UdsHandler::handleTransferData(const UdsMessage& request) {
    if (!download_state_.active) {
        return createNegativeResponse(request.service, NRC::UPLOAD_DOWNLOAD_NOT_ACCEPTED);
    }
    
    if (request.data.empty()) {
        return createNegativeResponse(request.service, NRC::INCORRECT_MESSAGE_LENGTH);
    }
    
    uint8_t block_counter = request.data[0];
    
    // Verify block sequence
    if (block_counter != download_state_.block_counter) {
        std::cout << "[UDS] Wrong block sequence: expected " 
                  << static_cast<int>(download_state_.block_counter)
                  << ", got " << static_cast<int>(block_counter) << std::endl;
        return createNegativeResponse(request.service, NRC::WRONG_BLOCK_SEQUENCE_COUNTER);
    }
    
    // Write data (skip first byte which is block counter)
    std::vector<uint8_t> block_data(request.data.begin() + 1, request.data.end());
    
    std::cout << "[UDS] Transfer block " << static_cast<int>(block_counter) 
              << " (" << block_data.size() << " bytes)" << std::endl;
    
    // In real implementation: write to flash/memory
    // writeToFlash(download_state_.address, block_data.data(), block_data.size());
    
    // Increment block counter (wraps at 0xFF)
    download_state_.block_counter = (download_state_.block_counter % 0xFF) + 1;
    
    return createPositiveResponse(request.service, {block_counter});
}

UdsResponse UdsHandler::handleRequestTransferExit(const UdsMessage& request) {
    if (!download_state_.active) {
        return createNegativeResponse(request.service, NRC::UPLOAD_DOWNLOAD_NOT_ACCEPTED);
    }
    
    std::cout << "[UDS] Transfer completed, exiting download session" << std::endl;
    download_state_.active = false;
    
    return createPositiveResponse(request.service);
}

uint32_t UdsHandler::generateSeed() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0x10000000, 0xFFFFFFFF);
    return dis(gen);
}

bool UdsHandler::verifySeedKey(uint32_t seed, uint32_t key) {
    // Simple algorithm: key = seed XOR 0xA5A5A5A5
    // In production: use cryptographic algorithm
    uint32_t expected_key = seed ^ 0xA5A5A5A5;
    return key == expected_key;
}

UdsResponse UdsHandler::createPositiveResponse(UdsService service, const std::vector<uint8_t>& data) {
    UdsResponse resp;
    resp.positive = true;
    resp.service = service;
    resp.nrc = NRC::POSITIVE_RESPONSE;
    resp.data = data;
    return resp;
}

UdsResponse UdsHandler::createNegativeResponse(UdsService service, NRC nrc) {
    UdsResponse resp;
    resp.positive = false;
    resp.service = service;
    resp.nrc = nrc;
    return resp;
}

} // namespace tc375

