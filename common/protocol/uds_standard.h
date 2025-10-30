/**
 * @file uds_standard.h
 * @brief UDS (Unified Diagnostic Services) ISO 14229 Standard Definitions
 * 
 * Common UDS protocol definitions shared across all components
 */

#ifndef UDS_STANDARD_H
#define UDS_STANDARD_H

#include <stdint.h>

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
#define UDS_SID_WRITE_DATA_BY_IDENTIFIER        0x2E
#define UDS_SID_ROUTINE_CONTROL                 0x31
#define UDS_SID_REQUEST_DOWNLOAD                0x34
#define UDS_SID_REQUEST_UPLOAD                  0x35
#define UDS_SID_TRANSFER_DATA                   0x36
#define UDS_SID_REQUEST_TRANSFER_EXIT           0x37
#define UDS_SID_READ_DTC_INFORMATION            0x19
#define UDS_SID_CLEAR_DTC_INFORMATION           0x14

/* UDS Negative Response Code (NRC) */
#define UDS_NRC_GENERAL_REJECT                  0x10
#define UDS_NRC_SERVICE_NOT_SUPPORTED           0x11
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED       0x12
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH        0x13
#define UDS_NRC_CONDITIONS_NOT_CORRECT          0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR          0x24
#define UDS_NRC_REQUEST_OUT_OF_RANGE            0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED          0x33
#define UDS_NRC_INVALID_KEY                     0x35
#define UDS_NRC_EXCEEDED_ATTEMPTS               0x36
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED    0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED         0x71
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE     0x72
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER    0x73
#define UDS_NRC_RESPONSE_PENDING                0x78

/* UDS Positive Response Offset */
#define UDS_POSITIVE_RESPONSE_OFFSET            0x40

/* Diagnostic Session Types */
#define UDS_SESSION_DEFAULT                     0x01
#define UDS_SESSION_PROGRAMMING                 0x02
#define UDS_SESSION_EXTENDED                    0x03

/* ECU Reset Types */
#define UDS_RESET_HARD                          0x01
#define UDS_RESET_KEY_OFF_ON                    0x02
#define UDS_RESET_SOFT                          0x03

/* Security Access Levels */
#define UDS_SECURITY_REQUEST_SEED_L1            0x01
#define UDS_SECURITY_SEND_KEY_L1                0x02
#define UDS_SECURITY_REQUEST_SEED_L2            0x03
#define UDS_SECURITY_SEND_KEY_L2                0x04

/* Routine Control Types */
#define UDS_ROUTINE_START                       0x01
#define UDS_ROUTINE_STOP                        0x02
#define UDS_ROUTINE_REQUEST_RESULTS             0x03

/* Common Data Identifiers (DID) */
#define UDS_DID_VIN                             0xF190  /* Vehicle Identification Number */
#define UDS_DID_ECU_MANUFACTURING_DATE          0xF18B
#define UDS_DID_ECU_SERIAL_NUMBER               0xF18C
#define UDS_DID_SYSTEM_SUPPLIER_ID              0xF18A
#define UDS_DID_ECU_SOFTWARE_NUMBER             0xF191
#define UDS_DID_ECU_SOFTWARE_VERSION            0xF195
#define UDS_DID_ECU_HARDWARE_NUMBER             0xF191
#define UDS_DID_ECU_HARDWARE_VERSION            0xF193
#define UDS_DID_VEHICLE_MANUFACTURER_ID         0xF10A
#define UDS_DID_BOOT_SOFTWARE_ID                0xF184
#define UDS_DID_APPLICATION_SOFTWARE_ID         0xF185
#define UDS_DID_ACTIVE_DIAGNOSTIC_SESSION       0xF186
#define UDS_DID_PROGRAMMING_COUNTER             0xF199
#define UDS_DID_FINGERPRINT                     0xF15B

/* Routine Identifiers */
#define UDS_ROUTINE_ERASE_MEMORY                0xFF00
#define UDS_ROUTINE_CHECK_PROGRAMMING_DEPS      0xFF01
#define UDS_ROUTINE_CHECK_MEMORY                0xFF02
#define UDS_ROUTINE_INSTALL_UPDATE              0xFF03

#ifdef __cplusplus
}
#endif

#endif /* UDS_STANDARD_H */

