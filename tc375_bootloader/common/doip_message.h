/**
 * @file doip_message.h
 * @brief DoIP (Diagnostics over IP) ISO 13400 Message Framing
 * 
 * Provides DoIP message construction and parsing for TC375 MCU.
 * Compatible with Python DoIPMessage class interface.
 */

#ifndef DOIP_MESSAGE_H
#define DOIP_MESSAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DoIP Protocol Constants */
#define DOIP_PROTOCOL_VERSION           0x02
#define DOIP_INVERSE_PROTOCOL_VERSION   0xFD
#define DOIP_HEADER_SIZE                8U

/* DoIP Payload Types */
#define DOIP_VEHICLE_IDENTIFICATION_REQ     0x0001
#define DOIP_VEHICLE_IDENTIFICATION_RES     0x0004
#define DOIP_ROUTING_ACTIVATION_REQ         0x0005
#define DOIP_ROUTING_ACTIVATION_RES         0x0006
#define DOIP_ALIVE_CHECK_REQ                0x0007
#define DOIP_ALIVE_CHECK_RES                0x0008
#define DOIP_DIAGNOSTIC_MESSAGE             0x8001
#define DOIP_DIAGNOSTIC_MESSAGE_POS_ACK     0x8002
#define DOIP_DIAGNOSTIC_MESSAGE_NEG_ACK     0x8003

/* Routing Activation Response Codes */
#define DOIP_RA_RES_SUCCESS                 0x10
#define DOIP_RA_RES_UNKNOWN_SOURCE          0x00
#define DOIP_RA_RES_NO_RESOURCES            0x01
#define DOIP_RA_RES_ALREADY_ACTIVE          0x02
#define DOIP_RA_RES_AUTH_REQUIRED           0x03
#define DOIP_RA_RES_AUTH_FAILED             0x04
#define DOIP_RA_RES_UNSUPPORTED_ACTIVATION  0x05
#define DOIP_RA_RES_TLS_REQUIRED            0x06

/* Diagnostic Message ACK Codes */
#define DOIP_DIAG_ACK_CONFIRM               0x00
#define DOIP_DIAG_NACK_INVALID_SA           0x02
#define DOIP_DIAG_NACK_UNKNOWN_TA           0x03
#define DOIP_DIAG_NACK_TOO_LARGE            0x04
#define DOIP_DIAG_NACK_OUT_OF_MEMORY        0x05
#define DOIP_DIAG_NACK_TARGET_UNREACHABLE   0x06

/* DoIP Configuration */
#define DOIP_VIN_LENGTH                     17U
#define DOIP_EID_LENGTH                     6U
#define DOIP_GID_LENGTH                     6U
#define DOIP_MAX_PAYLOAD_SIZE               4096U

/**
 * @brief DoIP Message Header (8 bytes, network byte order on wire)
 */
typedef struct {
    uint8_t  protocol_version;          /**< 0x02 */
    uint8_t  inverse_protocol_version;  /**< 0xFD */
    uint16_t payload_type;              /**< Host byte order */
    uint32_t payload_length;            /**< Host byte order */
} DoIPHeader_t;

/**
 * @brief Routing Activation Request Payload
 */
typedef struct {
    uint16_t source_address;    /**< Tester/Client logical address */
    uint8_t  activation_type;   /**< 0x00 = default, 0x01 = WWH-OBD, etc */
    uint32_t reserved;          /**< Reserved, set to 0 */
    uint32_t oem_specific;      /**< Optional OEM data */
} DoIPRoutingActivationReq_t;

/**
 * @brief Routing Activation Response Payload
 */
typedef struct {
    uint16_t tester_address;    /**< Echo of source address */
    uint16_t entity_address;    /**< DoIP entity logical address */
    uint8_t  response_code;     /**< Success = 0x10 */
    uint32_t reserved;          /**< Reserved */
    uint32_t oem_specific;      /**< Optional OEM data */
} DoIPRoutingActivationRes_t;

/**
 * @brief Vehicle Identification Response Payload
 */
typedef struct {
    char     vin[DOIP_VIN_LENGTH];          /**< Vehicle Identification Number */
    uint16_t logical_address;                /**< DoIP entity address */
    uint8_t  eid[DOIP_EID_LENGTH];          /**< Entity ID (MAC address) */
    uint8_t  gid[DOIP_GID_LENGTH];          /**< Group ID */
    uint8_t  further_action_required;        /**< 0x00 = no further action */
    uint8_t  vin_gid_sync_status;           /**< Optional */
} DoIPVehicleIdRes_t;

/**
 * @brief Diagnostic Message Header (within payload)
 */
typedef struct {
    uint16_t source_address;    /**< Tester logical address */
    uint16_t target_address;    /**< ECU logical address */
} DoIPDiagMessageHeader_t;

/* Byte Order Conversion Macros (for network/host conversion) */
#define DOIP_HTONS(x) ((uint16_t)(((x) >> 8) | ((x) << 8)))
#define DOIP_HTONL(x) ((uint32_t)(((x) >> 24) | (((x) & 0x00FF0000) >> 8) | \
                                  (((x) & 0x0000FF00) << 8) | ((x) << 24)))
#define DOIP_NTOHS(x) DOIP_HTONS(x)
#define DOIP_NTOHL(x) DOIP_HTONL(x)

/**
 * @brief Build DoIP message header + payload into buffer
 * 
 * @param payload_type DoIP payload type (e.g., DOIP_DIAGNOSTIC_MESSAGE)
 * @param payload Pointer to payload data (can be NULL if payload_len == 0)
 * @param payload_len Length of payload in bytes
 * @param out_buf Output buffer (must be >= 8 + payload_len)
 * @param out_cap Capacity of output buffer
 * @return Number of bytes written, or 0 on error
 */
size_t doip_build_message(
    uint16_t payload_type,
    const uint8_t* payload,
    uint32_t payload_len,
    uint8_t* out_buf,
    size_t out_cap
);

/**
 * @brief Parse DoIP message from byte stream
 * 
 * @param buf Input buffer containing DoIP message
 * @param buf_len Length of input buffer
 * @param header Output: parsed header (host byte order)
 * @param payload_out Output: pointer to payload within buf (can be NULL)
 * @return 0 on success, -1 if insufficient data, -2 if invalid header
 */
int doip_parse_message(
    const uint8_t* buf,
    size_t buf_len,
    DoIPHeader_t* header,
    const uint8_t** payload_out
);

/**
 * @brief Validate DoIP header
 * 
 * @param header Parsed header
 * @return true if valid (protocol version matches)
 */
bool doip_validate_header(const DoIPHeader_t* header);

/**
 * @brief Build Routing Activation Request
 * 
 * @param source_address Tester logical address (e.g., 0x0E00)
 * @param activation_type Activation type (0x00 = default)
 * @param out_buf Output buffer (must be >= 8 + 7)
 * @param out_cap Capacity of output buffer
 * @return Number of bytes written, or 0 on error
 */
size_t doip_build_routing_activation_req(
    uint16_t source_address,
    uint8_t activation_type,
    uint8_t* out_buf,
    size_t out_cap
);

/**
 * @brief Parse Routing Activation Response
 * 
 * @param payload Payload data (after DoIP header)
 * @param payload_len Length of payload
 * @param response Output: parsed response
 * @return 0 on success, -1 on error
 */
int doip_parse_routing_activation_res(
    const uint8_t* payload,
    size_t payload_len,
    DoIPRoutingActivationRes_t* response
);

/**
 * @brief Build Diagnostic Message (UDS over DoIP)
 * 
 * @param source_address Tester logical address
 * @param target_address ECU logical address
 * @param uds_data UDS request data (e.g., [0x10, 0x01])
 * @param uds_len Length of UDS data
 * @param out_buf Output buffer (must be >= 8 + 4 + uds_len)
 * @param out_cap Capacity of output buffer
 * @return Number of bytes written, or 0 on error
 */
size_t doip_build_diagnostic_message(
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t* uds_data,
    size_t uds_len,
    uint8_t* out_buf,
    size_t out_cap
);

/**
 * @brief Parse Diagnostic Message
 * 
 * @param payload Payload data (after DoIP header)
 * @param payload_len Length of payload
 * @param source_address Output: source address
 * @param target_address Output: target address
 * @param uds_data_out Output: pointer to UDS data within payload
 * @param uds_len_out Output: length of UDS data
 * @return 0 on success, -1 on error
 */
int doip_parse_diagnostic_message(
    const uint8_t* payload,
    size_t payload_len,
    uint16_t* source_address,
    uint16_t* target_address,
    const uint8_t** uds_data_out,
    size_t* uds_len_out
);

/**
 * @brief Build Vehicle Identification Request (typically UDP broadcast)
 * 
 * @param out_buf Output buffer (must be >= 8)
 * @param out_cap Capacity of output buffer
 * @return Number of bytes written (8 bytes), or 0 on error
 */
size_t doip_build_vehicle_id_req(uint8_t* out_buf, size_t out_cap);

/**
 * @brief Parse Vehicle Identification Response
 * 
 * @param payload Payload data
 * @param payload_len Length of payload
 * @param response Output: parsed response
 * @return 0 on success, -1 on error
 */
int doip_parse_vehicle_id_res(
    const uint8_t* payload,
    size_t payload_len,
    DoIPVehicleIdRes_t* response
);

#ifdef __cplusplus
}
#endif

#endif /* DOIP_MESSAGE_H */

