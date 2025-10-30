/**
 * @file uds_handler.c
 * @brief UDS Handler Implementation
 */

#include "uds_handler.h"
#include <string.h>

/* Service handler lookup table */
typedef struct {
    uint8_t sid;
    UDSServiceHandler_fn handler;
} UDSServiceEntry_t;

/* Forward declarations */
static const UDSServiceEntry_t service_table[] = {
    {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, uds_service_diagnostic_session_control},
    {UDS_SID_ECU_RESET, uds_service_ecu_reset},
    {UDS_SID_SECURITY_ACCESS, uds_service_security_access},
    {UDS_SID_TESTER_PRESENT, uds_service_tester_present},
    {UDS_SID_READ_DATA_BY_IDENTIFIER, uds_service_read_data_by_id},
    {UDS_SID_WRITE_DATA_BY_IDENTIFIER, uds_service_write_data_by_id},
    {UDS_SID_REQUEST_DOWNLOAD, uds_service_request_download},
    {UDS_SID_TRANSFER_DATA, uds_service_transfer_data},
    {UDS_SID_REQUEST_TRANSFER_EXIT, uds_service_request_transfer_exit},
};
#define SERVICE_TABLE_SIZE (sizeof(service_table) / sizeof(service_table[0]))

void uds_handler_init(UDSHandler_t* handler) {
    if (!handler) {
        return;
    }

    memset(handler, 0, sizeof(UDSHandler_t));
    handler->session = UDS_SESSION_STATE_DEFAULT;
    handler->security = UDS_SECURITY_LOCKED;
}

int uds_handler_process(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (!handler || !request || req_len == 0 || !response || !resp_len) {
        return -1;
    }

    uint8_t sid = request[0];

    /* Look up service handler */
    UDSServiceHandler_fn service_handler = NULL;
    for (size_t i = 0; i < SERVICE_TABLE_SIZE; i++) {
        if (service_table[i].sid == sid) {
            service_handler = service_table[i].handler;
            break;
        }
    }

    if (!service_handler) {
        /* Service not supported */
        return uds_build_negative_response(sid, UDS_NRC_SERVICE_NOT_SUPPORTED, response, resp_cap, resp_len);
    }

    /* Call service handler */
    int result = service_handler(handler, request, req_len, response, resp_cap, resp_len);
    
    if (result < 0) {
        /* Handler returned negative response code */
        return uds_build_negative_response(sid, (uint8_t)(-result), response, resp_cap, resp_len);
    }

    return 0;
}

int uds_build_positive_response(
    uint8_t sid,
    const uint8_t* data,
    size_t data_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (!response || !resp_len || resp_cap < (1 + data_len)) {
        return -1;
    }

    response[0] = sid + UDS_POSITIVE_RESPONSE_OFFSET;
    if (data && data_len > 0) {
        memcpy(&response[1], data, data_len);
    }

    *resp_len = 1 + data_len;
    return 0;
}

int uds_build_negative_response(
    uint8_t sid,
    uint8_t nrc,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (!response || !resp_len || resp_cap < 3) {
        return -1;
    }

    response[0] = UDS_NRC;
    response[1] = sid;
    response[2] = nrc;
    *resp_len = 3;

    return 0;
}

/* Service Handlers */

int uds_service_diagnostic_session_control(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 2) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    uint8_t session_type = request[1];

    /* Update session state */
    switch (session_type) {
        case UDS_SESSION_DEFAULT:
            handler->session = UDS_SESSION_STATE_DEFAULT;
            handler->security = UDS_SECURITY_LOCKED;
            break;

        case UDS_SESSION_PROGRAMMING:
            handler->session = UDS_SESSION_STATE_PROGRAMMING;
            handler->security = UDS_SECURITY_LOCKED;
            break;

        case UDS_SESSION_EXTENDED_DIAGNOSTIC:
            handler->session = UDS_SESSION_STATE_EXTENDED_DIAGNOSTIC;
            break;

        default:
            return -UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
    }

    /* Build positive response: SID + session_type */
    uint8_t resp_data = session_type;
    return uds_build_positive_response(
        UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
        &resp_data,
        1,
        response,
        resp_cap,
        resp_len
    ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
}

int uds_service_ecu_reset(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 2) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    uint8_t reset_type = request[1];

    /* Validate reset type */
    if (reset_type < UDS_RESET_HARD || reset_type > UDS_RESET_SOFT) {
        return -UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
    }

    /* Build positive response before reset */
    uint8_t resp_data = reset_type;
    int result = uds_build_positive_response(
        UDS_SID_ECU_RESET,
        &resp_data,
        1,
        response,
        resp_cap,
        resp_len
    );

    if (result == 0) {
        /* Trigger platform reset (non-blocking) */
        uds_platform_ecu_reset(reset_type);
    }

    return result == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
}

int uds_service_security_access(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 2) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    uint8_t sub_function = request[1];

    /* Check for lockout */
    if (handler->security_attempts >= UDS_SECURITY_ACCESS_ATTEMPTS) {
        uint32_t current_time = uds_platform_get_tick_ms();
        if ((current_time - handler->security_lockout_time) < UDS_SECURITY_ACCESS_DELAY_MS) {
            return -UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
        }
        /* Reset attempts after delay */
        handler->security_attempts = 0;
    }

    if (sub_function == UDS_SECURITY_LEVEL_1) {
        /* Request seed */
        if (handler->security == UDS_SECURITY_UNLOCKED) {
            /* Already unlocked, return seed = 0 */
            uint8_t resp_data[5] = {sub_function, 0, 0, 0, 0};
            return uds_build_positive_response(
                UDS_SID_SECURITY_ACCESS,
                resp_data,
                5,
                response,
                resp_cap,
                resp_len
            ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
        }

        /* Generate new seed */
        handler->seed = uds_platform_generate_seed();

        uint8_t resp_data[5];
        resp_data[0] = sub_function;
        resp_data[1] = (uint8_t)(handler->seed >> 24);
        resp_data[2] = (uint8_t)(handler->seed >> 16);
        resp_data[3] = (uint8_t)(handler->seed >> 8);
        resp_data[4] = (uint8_t)(handler->seed);

        return uds_build_positive_response(
            UDS_SID_SECURITY_ACCESS,
            resp_data,
            5,
            response,
            resp_cap,
            resp_len
        ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;

    } else if (sub_function == UDS_SECURITY_LEVEL_2) {
        /* Send key */
        if (req_len < 6) {
            return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
        }

        uint32_t received_key = ((uint32_t)request[2] << 24) |
                                 ((uint32_t)request[3] << 16) |
                                 ((uint32_t)request[4] << 8) |
                                 ((uint32_t)request[5]);

        uint32_t expected_key = uds_platform_calculate_key(handler->seed);

        if (received_key == expected_key) {
            handler->security = UDS_SECURITY_UNLOCKED;
            handler->security_attempts = 0;

            uint8_t resp_data = sub_function;
            return uds_build_positive_response(
                UDS_SID_SECURITY_ACCESS,
                &resp_data,
                1,
                response,
                resp_cap,
                resp_len
            ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
        } else {
            handler->security_attempts++;
            if (handler->security_attempts >= UDS_SECURITY_ACCESS_ATTEMPTS) {
                handler->security_lockout_time = uds_platform_get_tick_ms();
                return -UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS;
            }
            return -UDS_NRC_INVALID_KEY;
        }
    }

    return -UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
}

int uds_service_tester_present(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 2) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    uint8_t sub_function = request[1];
    
    /* Echo sub-function in response */
    return uds_build_positive_response(
        UDS_SID_TESTER_PRESENT,
        &sub_function,
        1,
        response,
        resp_cap,
        resp_len
    ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
}

int uds_service_read_data_by_id(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 3) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    uint16_t did = ((uint16_t)request[1] << 8) | request[2];

    /* Example: Read VIN */
    if (did == UDS_DID_VIN) {
        const char* vin = "WBADT43452G296403"; /* Example VIN */
        uint8_t resp_data[19];
        resp_data[0] = (uint8_t)(did >> 8);
        resp_data[1] = (uint8_t)(did);
        memcpy(&resp_data[2], vin, 17);

        return uds_build_positive_response(
            UDS_SID_READ_DATA_BY_IDENTIFIER,
            resp_data,
            19,
            response,
            resp_cap,
            resp_len
        ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
    }

    /* Add more DIDs as needed */
    return -UDS_NRC_REQUEST_OUT_OF_RANGE;
}

int uds_service_write_data_by_id(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 4) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    /* Check security */
    if (handler->security != UDS_SECURITY_UNLOCKED) {
        return -UDS_NRC_SECURITY_ACCESS_DENIED;
    }

    uint16_t did = ((uint16_t)request[1] << 8) | request[2];
    const uint8_t* data = &request[3];
    size_t data_len = req_len - 3;

    /* TODO: Implement write logic based on DID */
    (void)data;
    (void)data_len;

    /* Build positive response */
    uint8_t resp_data[2] = {(uint8_t)(did >> 8), (uint8_t)(did)};
    return uds_build_positive_response(
        UDS_SID_WRITE_DATA_BY_IDENTIFIER,
        resp_data,
        2,
        response,
        resp_cap,
        resp_len
    ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
}

int uds_service_request_download(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 4) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    /* Check session */
    if (handler->session != UDS_SESSION_STATE_PROGRAMMING) {
        return -UDS_NRC_CONDITIONS_NOT_CORRECT;
    }

    /* Check security */
    if (handler->security != UDS_SECURITY_UNLOCKED) {
        return -UDS_NRC_SECURITY_ACCESS_DENIED;
    }

    /* Parse request: dataFormatIdentifier + addressAndLengthFormatIdentifier + address + size */
    uint8_t addr_len_format = request[2];
    uint8_t addr_bytes = (addr_len_format >> 4) & 0x0F;
    uint8_t size_bytes = addr_len_format & 0x0F;

    if (req_len < (3U + addr_bytes + size_bytes)) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    /* Extract address */
    uint32_t address = 0;
    for (uint8_t i = 0; i < addr_bytes; i++) {
        address = (address << 8) | request[3 + i];
    }

    /* Extract size */
    uint32_t size = 0;
    for (uint8_t i = 0; i < size_bytes; i++) {
        size = (size << 8) | request[3 + addr_bytes + i];
    }

    /* Initialize transfer */
    handler->transfer_active = true;
    handler->block_sequence_counter = 1;
    handler->transfer_address = address;
    handler->transfer_size = size;
    handler->transfer_received = 0;

    /* Build positive response: lengthFormatIdentifier + maxNumberOfBlockLength */
    uint8_t resp_data[3];
    resp_data[0] = 0x20;  /* lengthFormatIdentifier (2 bytes) */
    resp_data[1] = 0x04;  /* maxNumberOfBlockLength (1024 bytes) */
    resp_data[2] = 0x00;

    return uds_build_positive_response(
        UDS_SID_REQUEST_DOWNLOAD,
        resp_data,
        3,
        response,
        resp_cap,
        resp_len
    ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
}

int uds_service_transfer_data(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    if (req_len < 2) {
        return -UDS_NRC_INCORRECT_MESSAGE_LENGTH;
    }

    if (!handler->transfer_active) {
        return -UDS_NRC_REQUEST_SEQUENCE_ERROR;
    }

    uint8_t block_seq = request[1];

    /* Verify sequence counter */
    if (block_seq != handler->block_sequence_counter) {
        return -UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER;
    }

    /* Extract transfer data */
    const uint8_t* data = &request[2];
    size_t data_len = req_len - 2;

    /* Write to flash */
    uint32_t write_addr = handler->transfer_address + handler->transfer_received;
    if (uds_platform_write_firmware(write_addr, data, data_len) != 0) {
        return -UDS_NRC_GENERAL_PROGRAMMING_FAILURE;
    }

    handler->transfer_received += data_len;
    handler->block_sequence_counter++;
    if (handler->block_sequence_counter == 0) {
        handler->block_sequence_counter = 1;  /* Wrap around, skip 0 */
    }

    /* Build positive response: blockSequenceCounter */
    return uds_build_positive_response(
        UDS_SID_TRANSFER_DATA,
        &block_seq,
        1,
        response,
        resp_cap,
        resp_len
    ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
}

int uds_service_request_transfer_exit(
    UDSHandler_t* handler,
    const uint8_t* request,
    size_t req_len,
    uint8_t* response,
    size_t resp_cap,
    size_t* resp_len
) {
    (void)request;
    (void)req_len;

    if (!handler->transfer_active) {
        return -UDS_NRC_REQUEST_SEQUENCE_ERROR;
    }

    /* Verify transfer completed */
    if (handler->transfer_received != handler->transfer_size) {
        return -UDS_NRC_GENERAL_PROGRAMMING_FAILURE;
    }

    /* Finalize transfer */
    handler->transfer_active = false;

    /* Build positive response (no data) */
    return uds_build_positive_response(
        UDS_SID_REQUEST_TRANSFER_EXIT,
        NULL,
        0,
        response,
        resp_cap,
        resp_len
    ) == 0 ? 0 : -UDS_NRC_GENERAL_REJECT;
}

