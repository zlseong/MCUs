/**
 * @file doip_client.c
 * @brief DoIP Client Implementation
 */

#include "doip_client.h"
#include <string.h>
#include <stdio.h>

int doip_client_init(
    DoIPClient_t* client,
    const char* server_ip,
    uint16_t server_port,
    uint16_t source_address,
    uint16_t target_address
) {
    if (!client || !server_ip) {
        return -1;
    }

    memset(client, 0, sizeof(DoIPClient_t));

    /* Parse server IP */
    if (doip_ip_str_to_addr(server_ip, &client->server_ip) != 0) {
        return -1;
    }

    /* Set configuration */
    client->server_port = (server_port == 0) ? DOIP_DEFAULT_PORT : server_port;
    client->source_address = source_address;
    client->target_address = target_address;
    client->tcp_socket = DOIP_INVALID_SOCKET;
    client->udp_socket = DOIP_INVALID_SOCKET;
    client->is_connected = false;
    client->routing_active = false;

    return 0;
}

int doip_client_connect(DoIPClient_t* client) {
    if (!client || client->is_connected) {
        return -1;
    }

    /* Create TCP socket */
    client->tcp_socket = doip_socket_tcp_create();
    if (client->tcp_socket == DOIP_INVALID_SOCKET) {
        return -1;
    }

    /* Connect to server */
    if (doip_socket_tcp_connect(client->tcp_socket, client->server_ip, client->server_port) != 0) {
        doip_socket_close(client->tcp_socket);
        client->tcp_socket = DOIP_INVALID_SOCKET;
        return -1;
    }

    client->is_connected = true;
    return 0;
}

void doip_client_disconnect(DoIPClient_t* client) {
    if (!client) {
        return;
    }

    if (client->tcp_socket != DOIP_INVALID_SOCKET) {
        doip_socket_close(client->tcp_socket);
        client->tcp_socket = DOIP_INVALID_SOCKET;
    }

    if (client->udp_socket != DOIP_INVALID_SOCKET) {
        doip_socket_close(client->udp_socket);
        client->udp_socket = DOIP_INVALID_SOCKET;
    }

    client->is_connected = false;
    client->routing_active = false;
}

int doip_client_vehicle_identification(DoIPClient_t* client, char* vin_out) {
    if (!client) {
        return -1;
    }

    /* Create UDP socket if not already created */
    if (client->udp_socket == DOIP_INVALID_SOCKET) {
        client->udp_socket = doip_socket_udp_create();
        if (client->udp_socket == DOIP_INVALID_SOCKET) {
            return -1;
        }
    }

    /* Build Vehicle Identification Request */
    size_t msg_len = doip_build_vehicle_id_req(client->tx_buffer, sizeof(client->tx_buffer));
    if (msg_len == 0) {
        return -1;
    }

    /* Send UDP broadcast */
    if (doip_socket_udp_broadcast(client->udp_socket, client->tx_buffer, msg_len, client->server_port) < 0) {
        return -1;
    }

    /* Receive response */
    int recv_len = doip_socket_udp_recv(
        client->udp_socket,
        client->rx_buffer,
        sizeof(client->rx_buffer),
        DOIP_SOCKET_TIMEOUT_MS,
        NULL
    );

    if (recv_len <= 0) {
        return -1;
    }

    /* Parse response */
    DoIPHeader_t header;
    const uint8_t* payload = NULL;
    if (doip_parse_message(client->rx_buffer, (size_t)recv_len, &header, &payload) != 0) {
        return -1;
    }

    if (header.payload_type != DOIP_VEHICLE_IDENTIFICATION_RES) {
        return -1;
    }

    /* Parse vehicle identification response */
    DoIPVehicleIdRes_t response;
    if (doip_parse_vehicle_id_res(payload, header.payload_length, &response) != 0) {
        return -1;
    }

    /* Store VIN */
    memcpy(client->vin, response.vin, DOIP_VIN_LENGTH);
    client->vin[DOIP_VIN_LENGTH] = '\0';
    client->entity_address = response.logical_address;

    if (vin_out) {
        memcpy(vin_out, client->vin, DOIP_VIN_LENGTH + 1);
    }

    return 0;
}

int doip_client_routing_activation(DoIPClient_t* client, uint8_t activation_type) {
    if (!client || !client->is_connected) {
        return -1;
    }

    /* Build Routing Activation Request */
    size_t msg_len = doip_build_routing_activation_req(
        client->source_address,
        activation_type,
        client->tx_buffer,
        sizeof(client->tx_buffer)
    );

    if (msg_len == 0) {
        return -1;
    }

    /* Send request */
    if (doip_socket_tcp_send(client->tcp_socket, client->tx_buffer, msg_len) < 0) {
        return -1;
    }

    /* Receive response */
    int recv_len = doip_socket_tcp_recv(
        client->tcp_socket,
        client->rx_buffer,
        sizeof(client->rx_buffer),
        DOIP_ROUTING_TIMEOUT_MS
    );

    if (recv_len <= 0) {
        return -1;
    }

    /* Parse response */
    DoIPHeader_t header;
    const uint8_t* payload = NULL;
    if (doip_parse_message(client->rx_buffer, (size_t)recv_len, &header, &payload) != 0) {
        return -1;
    }

    if (header.payload_type != DOIP_ROUTING_ACTIVATION_RES) {
        return -1;
    }

    /* Parse routing activation response */
    DoIPRoutingActivationRes_t response;
    if (doip_parse_routing_activation_res(payload, header.payload_length, &response) != 0) {
        return -1;
    }

    /* Check response code */
    if (response.response_code == DOIP_RA_RES_SUCCESS) {
        client->routing_active = true;
        return 0;
    }

    return -1;
}

int doip_client_send_diagnostic(
    DoIPClient_t* client,
    const uint8_t* uds_request,
    size_t req_len,
    uint8_t* uds_response,
    size_t resp_cap,
    size_t* resp_len_out
) {
    if (!client || !client->is_connected || !client->routing_active || !uds_request || !uds_response) {
        return -1;
    }

    /* Build diagnostic message */
    size_t msg_len = doip_build_diagnostic_message(
        client->source_address,
        client->target_address,
        uds_request,
        req_len,
        client->tx_buffer,
        sizeof(client->tx_buffer)
    );

    if (msg_len == 0) {
        return -1;
    }

    /* Send request */
    if (doip_socket_tcp_send(client->tcp_socket, client->tx_buffer, msg_len) < 0) {
        return -1;
    }

    /* Receive ACK (optional, some implementations skip this) */
    int recv_len = doip_socket_tcp_recv(
        client->tcp_socket,
        client->rx_buffer,
        sizeof(client->rx_buffer),
        DOIP_SOCKET_TIMEOUT_MS
    );

    if (recv_len <= 0) {
        return -1;
    }

    /* Parse first response (could be ACK or diagnostic response) */
    DoIPHeader_t header;
    const uint8_t* payload = NULL;
    if (doip_parse_message(client->rx_buffer, (size_t)recv_len, &header, &payload) != 0) {
        return -1;
    }

    /* If ACK, receive actual diagnostic response */
    if (header.payload_type == DOIP_DIAGNOSTIC_MESSAGE_POS_ACK) {
        recv_len = doip_socket_tcp_recv(
            client->tcp_socket,
            client->rx_buffer,
            sizeof(client->rx_buffer),
            DOIP_SOCKET_TIMEOUT_MS
        );

        if (recv_len <= 0) {
            return -1;
        }

        if (doip_parse_message(client->rx_buffer, (size_t)recv_len, &header, &payload) != 0) {
            return -1;
        }
    } else if (header.payload_type == DOIP_DIAGNOSTIC_MESSAGE_NEG_ACK) {
        return -1;
    }

    /* Verify it's a diagnostic message response */
    if (header.payload_type != DOIP_DIAGNOSTIC_MESSAGE) {
        return -1;
    }

    /* Parse diagnostic message */
    uint16_t source_addr, target_addr;
    const uint8_t* uds_data = NULL;
    size_t uds_len = 0;

    if (doip_parse_diagnostic_message(payload, header.payload_length, &source_addr, &target_addr, &uds_data, &uds_len) != 0) {
        return -1;
    }

    /* Verify addresses */
    if (source_addr != client->target_address || target_addr != client->source_address) {
        return -1;
    }

    /* Copy UDS response */
    if (uds_len > resp_cap) {
        return -1;
    }

    if (uds_data && uds_len > 0) {
        memcpy(uds_response, uds_data, uds_len);
    }

    if (resp_len_out) {
        *resp_len_out = uds_len;
    }

    return 0;
}

int doip_client_alive_check_response(DoIPClient_t* client, uint16_t source_address) {
    if (!client || !client->is_connected) {
        return -1;
    }

    /* Build alive check response */
    uint8_t payload[2];
    uint16_t sa_be = DOIP_HTONS(source_address);
    memcpy(payload, &sa_be, sizeof(uint16_t));

    size_t msg_len = doip_build_message(
        DOIP_ALIVE_CHECK_RES,
        payload,
        sizeof(payload),
        client->tx_buffer,
        sizeof(client->tx_buffer)
    );

    if (msg_len == 0) {
        return -1;
    }

    /* Send response */
    if (doip_socket_tcp_send(client->tcp_socket, client->tx_buffer, msg_len) < 0) {
        return -1;
    }

    return 0;
}

