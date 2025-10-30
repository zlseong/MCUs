/**
 * @file doip_message.c
 * @brief DoIP Message Implementation
 */

#include "doip_message.h"
#include <string.h>

size_t doip_build_message(
    uint16_t payload_type,
    const uint8_t* payload,
    uint32_t payload_len,
    uint8_t* out_buf,
    size_t out_cap
) {
    if (!out_buf || out_cap < (DOIP_HEADER_SIZE + payload_len)) {
        return 0;
    }

    /* Build header */
    out_buf[0] = DOIP_PROTOCOL_VERSION;
    out_buf[1] = DOIP_INVERSE_PROTOCOL_VERSION;

    /* Payload type (network byte order) */
    uint16_t pt_be = DOIP_HTONS(payload_type);
    memcpy(&out_buf[2], &pt_be, sizeof(uint16_t));

    /* Payload length (network byte order) */
    uint32_t len_be = DOIP_HTONL(payload_len);
    memcpy(&out_buf[4], &len_be, sizeof(uint32_t));

    /* Copy payload */
    if (payload_len > 0 && payload) {
        memcpy(&out_buf[DOIP_HEADER_SIZE], payload, payload_len);
    }

    return DOIP_HEADER_SIZE + payload_len;
}

int doip_parse_message(
    const uint8_t* buf,
    size_t buf_len,
    DoIPHeader_t* header,
    const uint8_t** payload_out
) {
    if (!buf || !header || buf_len < DOIP_HEADER_SIZE) {
        return -1;
    }

    /* Parse header */
    header->protocol_version = buf[0];
    header->inverse_protocol_version = buf[1];

    /* Read payload type and length (convert from network byte order) */
    uint16_t pt_be;
    uint32_t len_be;
    memcpy(&pt_be, &buf[2], sizeof(uint16_t));
    memcpy(&len_be, &buf[4], sizeof(uint32_t));

    header->payload_type = DOIP_NTOHS(pt_be);
    header->payload_length = DOIP_NTOHL(len_be);

    /* Validate protocol version */
    if (header->protocol_version != DOIP_PROTOCOL_VERSION ||
        header->inverse_protocol_version != DOIP_INVERSE_PROTOCOL_VERSION) {
        return -2;
    }

    /* Check if buffer contains full payload */
    if (buf_len < (DOIP_HEADER_SIZE + header->payload_length)) {
        return -1;
    }

    /* Set payload pointer */
    if (payload_out) {
        if (header->payload_length > 0) {
            *payload_out = &buf[DOIP_HEADER_SIZE];
        } else {
            *payload_out = NULL;
        }
    }

    return 0;
}

bool doip_validate_header(const DoIPHeader_t* header) {
    if (!header) {
        return false;
    }
    return (header->protocol_version == DOIP_PROTOCOL_VERSION &&
            header->inverse_protocol_version == DOIP_INVERSE_PROTOCOL_VERSION);
}

size_t doip_build_routing_activation_req(
    uint16_t source_address,
    uint8_t activation_type,
    uint8_t* out_buf,
    size_t out_cap
) {
    /* Payload: source_address(2) + activation_type(1) + reserved(4) = 7 bytes */
    uint8_t payload[7];
    uint16_t sa_be = DOIP_HTONS(source_address);
    memcpy(&payload[0], &sa_be, sizeof(uint16_t));
    payload[2] = activation_type;
    memset(&payload[3], 0, 4); /* Reserved */

    return doip_build_message(
        DOIP_ROUTING_ACTIVATION_REQ,
        payload,
        sizeof(payload),
        out_buf,
        out_cap
    );
}

int doip_parse_routing_activation_res(
    const uint8_t* payload,
    size_t payload_len,
    DoIPRoutingActivationRes_t* response
) {
    if (!payload || !response || payload_len < 9) {
        return -1;
    }

    /* Parse response */
    uint16_t tester_be, entity_be;
    memcpy(&tester_be, &payload[0], sizeof(uint16_t));
    memcpy(&entity_be, &payload[2], sizeof(uint16_t));

    response->tester_address = DOIP_NTOHS(tester_be);
    response->entity_address = DOIP_NTOHS(entity_be);
    response->response_code = payload[4];

    /* Reserved (4 bytes) */
    memcpy(&response->reserved, &payload[5], sizeof(uint32_t));
    response->reserved = DOIP_NTOHL(response->reserved);

    /* OEM specific (optional) */
    if (payload_len >= 13) {
        memcpy(&response->oem_specific, &payload[9], sizeof(uint32_t));
        response->oem_specific = DOIP_NTOHL(response->oem_specific);
    } else {
        response->oem_specific = 0;
    }

    return 0;
}

size_t doip_build_diagnostic_message(
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t* uds_data,
    size_t uds_len,
    uint8_t* out_buf,
    size_t out_cap
) {
    /* Payload: source_address(2) + target_address(2) + uds_data */
    size_t payload_len = 4 + uds_len;
    
    if (!out_buf || out_cap < (DOIP_HEADER_SIZE + payload_len)) {
        return 0;
    }

    /* Build payload */
    uint8_t* payload = &out_buf[DOIP_HEADER_SIZE];
    uint16_t sa_be = DOIP_HTONS(source_address);
    uint16_t ta_be = DOIP_HTONS(target_address);

    memcpy(&payload[0], &sa_be, sizeof(uint16_t));
    memcpy(&payload[2], &ta_be, sizeof(uint16_t));

    if (uds_len > 0 && uds_data) {
        memcpy(&payload[4], uds_data, uds_len);
    }

    /* Build complete message */
    return doip_build_message(
        DOIP_DIAGNOSTIC_MESSAGE,
        payload,
        (uint32_t)payload_len,
        out_buf,
        out_cap
    );
}

int doip_parse_diagnostic_message(
    const uint8_t* payload,
    size_t payload_len,
    uint16_t* source_address,
    uint16_t* target_address,
    const uint8_t** uds_data_out,
    size_t* uds_len_out
) {
    if (!payload || payload_len < 4) {
        return -1;
    }

    /* Parse addresses */
    uint16_t sa_be, ta_be;
    memcpy(&sa_be, &payload[0], sizeof(uint16_t));
    memcpy(&ta_be, &payload[2], sizeof(uint16_t));

    if (source_address) {
        *source_address = DOIP_NTOHS(sa_be);
    }
    if (target_address) {
        *target_address = DOIP_NTOHS(ta_be);
    }

    /* Extract UDS data */
    size_t uds_len = payload_len - 4;
    if (uds_data_out) {
        if (uds_len > 0) {
            *uds_data_out = &payload[4];
        } else {
            *uds_data_out = NULL;
        }
    }
    if (uds_len_out) {
        *uds_len_out = uds_len;
    }

    return 0;
}

size_t doip_build_vehicle_id_req(uint8_t* out_buf, size_t out_cap) {
    /* Vehicle Identification Request has no payload */
    return doip_build_message(
        DOIP_VEHICLE_IDENTIFICATION_REQ,
        NULL,
        0,
        out_buf,
        out_cap
    );
}

int doip_parse_vehicle_id_res(
    const uint8_t* payload,
    size_t payload_len,
    DoIPVehicleIdRes_t* response
) {
    if (!payload || !response || payload_len < 33) {
        return -1;
    }

    /* Parse VIN (17 bytes) */
    memcpy(response->vin, payload, DOIP_VIN_LENGTH);

    /* Logical address (2 bytes) */
    uint16_t addr_be;
    memcpy(&addr_be, &payload[17], sizeof(uint16_t));
    response->logical_address = DOIP_NTOHS(addr_be);

    /* EID (6 bytes) */
    memcpy(response->eid, &payload[19], DOIP_EID_LENGTH);

    /* GID (6 bytes) */
    memcpy(response->gid, &payload[25], DOIP_GID_LENGTH);

    /* Further action required (1 byte) */
    response->further_action_required = payload[31];

    /* VIN/GID sync status (optional, 1 byte) */
    if (payload_len >= 33) {
        response->vin_gid_sync_status = payload[32];
    } else {
        response->vin_gid_sync_status = 0;
    }

    return 0;
}

