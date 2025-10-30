/**
 * @file uds_service_handler.hpp
 * @brief UDS Service Handler for VMG Gateway
 * 
 * Implements common UDS services for diagnostic communication.
 */

#ifndef UDS_SERVICE_HANDLER_HPP
#define UDS_SERVICE_HANDLER_HPP

#include <vector>
#include <map>
#include <string>
#include <functional>
#include <cstdint>

namespace vmg {

/**
 * @brief UDS Service IDs
 */
enum class UDSServiceID : uint8_t {
    DiagnosticSessionControl = 0x10,
    ECUReset = 0x11,
    SecurityAccess = 0x27,
    CommunicationControl = 0x28,
    TesterPresent = 0x3E,
    ReadDataByIdentifier = 0x22,
    ReadMemoryByAddress = 0x23,
    ReadDTCInformation = 0x19,
    WriteDataByIdentifier = 0x2E,
    WriteMemoryByAddress = 0x3D,
    ClearDTCInformation = 0x14,
    RoutineControl = 0x31,
    RequestDownload = 0x34,
    RequestUpload = 0x35,
    TransferData = 0x36,
    RequestTransferExit = 0x37
};

/**
 * @brief UDS Negative Response Codes
 */
enum class UDSNRC : uint8_t {
    GeneralReject = 0x10,
    ServiceNotSupported = 0x11,
    SubFunctionNotSupported = 0x12,
    IncorrectMessageLength = 0x13,
    ConditionsNotCorrect = 0x22,
    RequestSequenceError = 0x24,
    RequestOutOfRange = 0x31,
    SecurityAccessDenied = 0x33,
    InvalidKey = 0x35,
    ExceedNumberOfAttempts = 0x36,
    RequiredTimeDelayNotExpired = 0x37
};

/**
 * @brief UDS Data Identifiers (DID)
 */
enum class UDSDID : uint16_t {
    VIN = 0xF190,
    ECUSerialNumber = 0xF18C,
    ECUSoftwareVersion = 0xF195,
    ECUHardwareVersion = 0xF191,
    BootloaderVersion = 0xF180,
    ApplicationVersion = 0xF181
};

/**
 * @brief UDS Service Handler
 * 
 * Processes UDS requests and generates responses.
 */
class UDSServiceHandler {
public:
    UDSServiceHandler();
    ~UDSServiceHandler() = default;

    // Process UDS request
    std::vector<uint8_t> processRequest(const std::vector<uint8_t>& request);

    // Set vehicle/ECU information
    void setVIN(const std::string& vin);
    void setECUSerialNumber(const std::string& serial);
    void setSoftwareVersion(const std::string& version);
    void setHardwareVersion(const std::string& version);

    // Register custom DID handler
    using DIDHandler = std::function<std::vector<uint8_t>(uint16_t did)>;
    void registerDIDReadHandler(uint16_t did, DIDHandler handler);

private:
    // Service handlers
    std::vector<uint8_t> handleDiagnosticSessionControl(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleECUReset(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleSecurityAccess(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleTesterPresent(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleReadDataByIdentifier(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleWriteDataByIdentifier(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleReadDTCInformation(const std::vector<uint8_t>& request);
    std::vector<uint8_t> handleRoutineControl(const std::vector<uint8_t>& request);

    // Response builders
    std::vector<uint8_t> buildPositiveResponse(uint8_t sid, const std::vector<uint8_t>& data = {});
    std::vector<uint8_t> buildNegativeResponse(uint8_t sid, UDSNRC nrc);

    // Data storage
    std::string vin_;
    std::string ecu_serial_;
    std::string software_version_;
    std::string hardware_version_;

    // Custom DID handlers
    std::map<uint16_t, DIDHandler> did_handlers_;

    // State
    uint8_t current_session_;
    bool security_unlocked_;
    uint32_t security_seed_;
    uint8_t security_attempts_;
};

} // namespace vmg

#endif // UDS_SERVICE_HANDLER_HPP

