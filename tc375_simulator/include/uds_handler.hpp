#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace tc375 {

// UDS Service IDs (ISO 14229)
enum class UdsService : uint8_t {
    DIAGNOSTIC_SESSION_CONTROL = 0x10,
    ECU_RESET = 0x11,
    SECURITY_ACCESS = 0x27,
    COMMUNICATION_CONTROL = 0x28,
    TESTER_PRESENT = 0x3E,
    READ_DATA_BY_ID = 0x22,
    WRITE_DATA_BY_ID = 0x2E,
    ROUTINE_CONTROL = 0x31,
    REQUEST_DOWNLOAD = 0x34,
    REQUEST_UPLOAD = 0x35,
    TRANSFER_DATA = 0x36,
    REQUEST_TRANSFER_EXIT = 0x37,
    READ_DTC = 0x19,
    CLEAR_DTC = 0x14
};

// Negative Response Codes (NRC)
enum class NRC : uint8_t {
    POSITIVE_RESPONSE = 0x00,
    GENERAL_REJECT = 0x10,
    SERVICE_NOT_SUPPORTED = 0x11,
    SUBFUNCTION_NOT_SUPPORTED = 0x12,
    INCORRECT_MESSAGE_LENGTH = 0x13,
    REQUEST_OUT_OF_RANGE = 0x31,
    SECURITY_ACCESS_DENIED = 0x33,
    INVALID_KEY = 0x35,
    UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70,
    TRANSFER_DATA_SUSPENDED = 0x71,
    GENERAL_PROGRAMMING_FAILURE = 0x72,
    WRONG_BLOCK_SEQUENCE_COUNTER = 0x73,
    REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING = 0x78
};

// UDS Request/Response
struct UdsMessage {
    UdsService service;
    uint8_t sub_function;
    std::vector<uint8_t> data;
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static UdsMessage deserialize(const std::vector<uint8_t>& raw);
};

struct UdsResponse {
    bool positive;
    UdsService service;
    NRC nrc;  // Negative Response Code
    std::vector<uint8_t> data;
    
    std::vector<uint8_t> serialize() const;
    static UdsResponse deserialize(const std::vector<uint8_t>& raw);
};

// UDS Handler
class UdsHandler {
public:
    UdsHandler();
    ~UdsHandler() = default;

    // Process UDS request
    UdsResponse handleRequest(const UdsMessage& request);

    // Service handlers
    using ServiceHandler = std::function<UdsResponse(const UdsMessage&)>;
    void registerServiceHandler(UdsService service, ServiceHandler handler);

    // Pre-defined handlers
    UdsResponse handleDiagnosticSession(const UdsMessage& request);
    UdsResponse handleEcuReset(const UdsMessage& request);
    UdsResponse handleSecurityAccess(const UdsMessage& request);
    UdsResponse handleTesterPresent(const UdsMessage& request);
    UdsResponse handleReadDataById(const UdsMessage& request);
    UdsResponse handleWriteDataById(const UdsMessage& request);
    UdsResponse handleRoutineControl(const UdsMessage& request);
    
    // OTA-related services
    UdsResponse handleRequestDownload(const UdsMessage& request);
    UdsResponse handleTransferData(const UdsMessage& request);
    UdsResponse handleRequestTransferExit(const UdsMessage& request);

private:
    std::map<UdsService, ServiceHandler> service_handlers_;
    
    // Security state
    enum class SecurityLevel {
        LOCKED,
        SEED_SENT,
        UNLOCKED
    };
    SecurityLevel security_level_;
    uint32_t current_seed_;
    
    // Session state
    enum class DiagnosticSession {
        DEFAULT = 0x01,
        PROGRAMMING = 0x02,
        EXTENDED = 0x03
    };
    DiagnosticSession current_session_;
    
    // OTA download state
    struct DownloadState {
        bool active;
        uint32_t address;
        uint32_t size;
        uint32_t bytes_received;
        uint8_t block_counter;
    };
    DownloadState download_state_;

    // Helper functions
    uint32_t generateSeed();
    bool verifySeedKey(uint32_t seed, uint32_t key);
    UdsResponse createPositiveResponse(UdsService service, const std::vector<uint8_t>& data = {});
    UdsResponse createNegativeResponse(UdsService service, NRC nrc);
};

} // namespace tc375

