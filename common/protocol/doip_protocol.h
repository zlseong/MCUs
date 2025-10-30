/**
 * @file doip_protocol.h
 * @brief DoIP (Diagnostics over IP) ISO 13400 Protocol Definitions
 * 
 * Common DoIP protocol definitions shared across all components:
 * - VMG (Vehicle Management Gateway)
 * - ZG (Zonal Gateway) - TC375 and Linux
 * - ECU (End Node) - TC375
 */

#ifndef DOIP_PROTOCOL_H
#define DOIP_PROTOCOL_H

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
#define DOIP_DEFAULT_PORT               13400

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

/* Byte Order Conversion Macros */
#define DOIP_HTONS(x) ((uint16_t)(((x) >> 8) | ((x) << 8)))
#define DOIP_HTONL(x) ((uint32_t)(((x) >> 24) | (((x) & 0x00FF0000) >> 8) | \
                                  (((x) & 0x0000FF00) << 8) | ((x) << 24)))
#define DOIP_NTOHS(x) DOIP_HTONS(x)
#define DOIP_NTOHL(x) DOIP_HTONL(x)

/**
 * @brief DoIP Message Header (8 bytes)
 */
typedef struct {
    uint8_t  protocol_version;          /**< 0x02 */
    uint8_t  inverse_protocol_version;  /**< 0xFD */
    uint16_t payload_type;              /**< Host byte order */
    uint32_t payload_length;            /**< Host byte order */
} DoIPHeader_t;

#ifdef __cplusplus
}
#endif

#endif /* DOIP_PROTOCOL_H */

