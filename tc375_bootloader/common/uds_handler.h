/**
 * @file uds_handler.h
 * @brief UDS (Unified Diagnostic Services) ISO 14229 Handler
 * 
 * Provides UDS request processing for TC375 MCU.
 */

#ifndef UDS_HANDLER_H
#define UDS_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UDS Service IDs (SID) */
#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL      0x10
#define UDS_SID_ECU_RESET                       0x11
#define UDS_SID_SECURITY_ACCESS                 0x27
#define UDS_SID_COMMUNICATION_CONTROL           0x28
#define UDS_SID_TESTER_PRESENT                  0x3E
#define UDS_SID_READ_DATA_BY_IDENTIFIER         0x22
#define UDS_SID_READ_MEMORY_BY_ADDRESS          0x23
#define UDS_SID_READ_DTC_INFORMATION            0x19
#define UDS_SID_WRITE_DATA_BY_IDENTIFIER        0x2E
#define UDS_SID_WRITE_MEMORY_BY_ADDRESS         0x3D
#define UDS_SID_CLEAR_DTC_INFORMATION           0x14
#define UDS_SID_ROUTINE_CONTROL                 0x31
#define UDS_SID_REQUEST_DOWNLOAD                0x34
#define UDS_SID_REQUEST_UPLOAD                  0x35
#define UDS_SID_TRANSFER_DATA                   0x36
#define UDS_SID_REQUEST_TRANSFER_EXIT           0x37

/* UDS Positive Response Offset */
#define UDS_POSITIVE_RESPONSE_OFFSET            0x40

/* UDS Negative Response Code */
#define UDS_NRC                                 0x7F

/* UDS Negative Response Codes (NRC) */
#define UDS_NRC_GENERAL_REJECT                  0x10
#define UDS_NRC_SERVICE_NOT_SUPPORTED           0x11
#define UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED      0x12
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH        0x13
#define UDS_NRC_CONDITIONS_NOT_CORRECT          0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR          0x24
#define UDS_NRC_REQUEST_OUT_OF_RANGE            0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED          0x33
#define UDS_NRC_INVALID_KEY                     0x35
#define UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS       0x36
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED    0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED         0x71
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE     0x72
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER    0x73
#define UDS_NRC_RESPONSE_PENDING                0x78

/* Diagnostic Session Types */
#define UDS_SESSION_DEFAULT                     0x01
#define UDS_SESSION_PROGRAMMING                 0x02
#define UDS_SESSION_EXTENDED_DIAGNOSTIC         0x03

/* ECU Reset Types */
#define UDS_RESET_HARD                          0x01
#define UDS_RESET_KEY_OFF_ON                    0x02
#define UDS_RESET_SOFT                          0x03

/* Security Access Levels */
#define UDS_SECURITY_LEVEL_1                    0x01  /* Request seed */
#define UDS_SECURITY_LEVEL_2                    0x02  /* Send key */

/* Routine Control Types */
#define UDS_ROUTINE_START                       0x01
#define UDS_ROUTINE_STOP                        0x02
#define UDS_ROUTINE_REQUEST_RESULTS             0x03

/* Common Data Identifiers (DID) */
#define UDS_DID_VIN                             0xF190
#define UDS_DID_ECU_SERIAL_NUMBER               0xF18C
#define UDS_DID_ECU_SOFTWARE_VERSION            0xF195
#define UDS_DID_ECU_HARDWARE_VERSION            0xF191
#define UDS_DID_BOOTLOADER_VERSION              0xF180
#define UDS_DID_APPLICATION_VERSION             0xF181

/* Configuration */
#define UDS_MAX_REQUEST_SIZE                    4095
#define UDS_MAX_RESPONSE_SIZE                   4095
#define UDS_SECURITY_ACCESS_ATTEMPTS            3
#define UDS_SECURITY_ACCESS_DELAY_MS            10000

/**
 * @brief UDS Session State
 */
typedef enum {
    UDS_SESSION_STATE_DEFAULT = 0,
    UDS_SESSION_STATE_PROGRAMMING,
    UDS_SESSION_STATE_EXTENDED_DIAGNOSTIC
} UDSSessionState_t;

/**
 * @brief UDS Security State
 */
typedef enum {
    UDS_SECURITY_LOCKED = 0,
    UDS_SECURITY_UNLOCKED
} UDSSecurityState_t;

/**
 * @brief UDS Handler Context
 */
typedef struct {
    /* Session state */
    UDSSessionState_t session;
    UDSSecurityState_t security;
    
    /* Security access tracking */
    uint32_t seed;
    uint8_t security_attempts;
    uint32_t security_lockout_time;
    
    /* Transfer state (for firmware download) */
    bool transfer_active;
    uint8_t block_sequence_counter;
    uint32_t transfer_address;
    uint32_t transfer_size;
    uint32_t transfer_received;
    
    /* Buffers */
    uint8_t response_buffer[UDS_MAX_RESPONSE_SIZE];
    size_t response_length;
} UDSHandler_t;

/**
 * @brief UDS Service Handler Function Type
 * 
 * @param handler UDS handler context
 * @param request Request data (including SID)
 * @param req_len Request length
 * @param response Response buffer
 * @param resp_cap Response buffer capacity
 * @param resp_len Output: response length
 * @return 0 on success, negative NRC on error
 */
typedef int (*UDSServiceHandler_fn)(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Initialize UDS handler
 * 
 * @param handler Handler context
 */
void uds_handler_init(UDSHandler_t* handler);

/**
 * @brief Process UDS request
 * 
 * @param handler Handler context
 * @param request Request data
 * @param req_len Request length
 * @param response Response buffer
 * @param resp_cap Response capacity
 * @param resp_len Output: response length
 * @return 0 on success, -1 on error
 */
int uds_handler_process(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Build positive response
 * 
 * @param sid Service ID
 * @param data Response data (optional)
 * @param data_len Length of response data
 * @param response Output buffer
 * @param resp_cap Buffer capacity
 * @param resp_len Output: response length
 * @return 0 on success, -1 on error
 */
int uds_build_positive_response(
    uint8_t sid,
    const uint8_t* data,
    size_t data_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Build negative response
 * 
 * @param sid Service ID
 * @param nrc Negative response code
 * @param response Output buffer
 * @param resp_cap Buffer capacity
 * @param resp_len Output: response length
 * @return 0 on success, -1 on error
 */
int uds_build_negative_response(
    uint8_t sid,
    uint8_t nrc,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/* Individual Service Handlers */

/**
 * @brief Diagnostic Session Control (0x10)
 */
int uds_service_diagnostic_session_control(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief ECU Reset (0x11)
 */
int uds_service_ecu_reset(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Security Access (0x27)
 */
int uds_service_security_access(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Tester Present (0x3E)
 */
int uds_service_tester_present(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Read Data By Identifier (0x22)
 */
int uds_service_read_data_by_id(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Write Data By Identifier (0x2E)
 */
int uds_service_write_data_by_id(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Request Download (0x34) - for firmware OTA
 */
int uds_service_request_download(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Transfer Data (0x36) - for firmware OTA
 */
int uds_service_transfer_data(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/**
 * @brief Request Transfer Exit (0x37) - for firmware OTA
 */
int uds_service_request_transfer_exit(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
);

/* Platform-specific functions (implement separately) */

/**
 * @brief Perform ECU reset
 * @param reset_type Reset type (hard/soft)
 */
void uds_platform_ecu_reset(uint8_t reset_type);

/**
 * @brief Get system tick (milliseconds)
 * @return Current system tick
 */
uint32_t uds_platform_get_tick_ms(void);

/**
 * @brief Generate security seed
 * @return Random seed value
 */
uint32_t uds_platform_generate_seed(void);

/**
 * @brief Calculate security key from seed
 * @param seed Seed value
 * @return Expected key value
 */
uint32_t uds_platform_calculate_key(uint32_t seed);

/**
 * @brief Write firmware data to flash
 * @param address Target address
 * @param data Data to write
 * @param len Length of data
 * @return 0 on success, -1 on error
 */
int uds_platform_write_firmware(uint32_t address, const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UDS_HANDLER_H */

