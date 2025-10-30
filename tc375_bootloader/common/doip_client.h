/**
 * @file doip_client.h
 * @brief DoIP Client Implementation for TC375 MCU
 * 
 * Provides TCP/UDP socket abstraction and DoIP client functionality.
 * Compatible with Python DoIPClient class interface.
 */

#ifndef DOIP_CLIENT_H
#define DOIP_CLIENT_H

#include "doip_message.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define DOIP_DEFAULT_PORT           13400
#define DOIP_MAX_RESPONSE_SIZE      4096
#define DOIP_SOCKET_TIMEOUT_MS      5000
#define DOIP_ROUTING_TIMEOUT_MS     2000

/* Socket Handle Types (platform-specific) */
typedef int DoIPSocket_t;  /* Use lwIP socket descriptor or platform-specific */
#define DOIP_INVALID_SOCKET  (-1)

/**
 * @brief DoIP Client Context
 */
typedef struct {
    /* Connection state */
    DoIPSocket_t tcp_socket;
    DoIPSocket_t udp_socket;
    
    /* Server info */
    uint32_t server_ip;         /* Network byte order */
    uint16_t server_port;       /* Host byte order */
    
    /* Addressing */
    uint16_t source_address;    /* Tester address (e.g., 0x0E00) */
    uint16_t target_address;    /* ECU address (e.g., 0x0100) */
    
    /* State */
    bool is_connected;
    bool routing_active;
    
    /* Vehicle info (from identification) */
    char vin[DOIP_VIN_LENGTH + 1];  /* Null-terminated */
    uint16_t entity_address;
    
    /* Buffers */
    uint8_t tx_buffer[DOIP_MAX_RESPONSE_SIZE];
    uint8_t rx_buffer[DOIP_MAX_RESPONSE_SIZE];
} DoIPClient_t;

/**
 * @brief Initialize DoIP client
 * 
 * @param client Client context
 * @param server_ip Server IP address (e.g., "192.168.1.100")
 * @param server_port Server port (0 = use default 13400)
 * @param source_address Tester logical address
 * @param target_address ECU logical address
 * @return 0 on success, -1 on error
 */
int doip_client_init(
    DoIPClient_t* client,
    const char* server_ip,
    uint16_t server_port,
    uint16_t source_address,
    uint16_t target_address
);

/**
 * @brief Connect to DoIP server (TCP)
 * 
 * @param client Client context
 * @return 0 on success, -1 on error
 */
int doip_client_connect(DoIPClient_t* client);

/**
 * @brief Disconnect from DoIP server
 * 
 * @param client Client context
 */
void doip_client_disconnect(DoIPClient_t* client);

/**
 * @brief Send Vehicle Identification Request (UDP broadcast)
 * 
 * @param client Client context
 * @param vin_out Output buffer for VIN (must be >= 18 bytes for null terminator)
 * @return 0 on success, -1 on error
 */
int doip_client_vehicle_identification(DoIPClient_t* client, char* vin_out);

/**
 * @brief Send Routing Activation Request
 * 
 * @param client Client context
 * @param activation_type Activation type (0x00 = default)
 * @return 0 on success (routing active), -1 on error
 */
int doip_client_routing_activation(DoIPClient_t* client, uint8_t activation_type);

/**
 * @brief Send UDS Diagnostic Message
 * 
 * @param client Client context
 * @param uds_request UDS request data (e.g., [0x10, 0x01])
 * @param req_len Length of UDS request
 * @param uds_response Output buffer for UDS response
 * @param resp_cap Capacity of response buffer
 * @param resp_len_out Output: actual response length
 * @return 0 on success, -1 on error
 */
int doip_client_send_diagnostic(
    DoIPClient_t* client,
    const uint8_t* uds_request,
    size_t req_len,
    uint8_t* uds_response,
    size_t resp_cap,
    size_t* resp_len_out
);

/**
 * @brief Send Alive Check Response
 * 
 * @param client Client context
 * @param source_address Source address to echo
 * @return 0 on success, -1 on error
 */
int doip_client_alive_check_response(DoIPClient_t* client, uint16_t source_address);

/* Low-Level Socket Operations (platform-specific, implement in separate file) */

/**
 * @brief Create TCP socket
 * @return Socket descriptor or DOIP_INVALID_SOCKET on error
 */
DoIPSocket_t doip_socket_tcp_create(void);

/**
 * @brief Create UDP socket
 * @return Socket descriptor or DOIP_INVALID_SOCKET on error
 */
DoIPSocket_t doip_socket_udp_create(void);

/**
 * @brief Connect TCP socket to server
 * @param sock Socket descriptor
 * @param ip Server IP (network byte order)
 * @param port Server port (host byte order)
 * @return 0 on success, -1 on error
 */
int doip_socket_tcp_connect(DoIPSocket_t sock, uint32_t ip, uint16_t port);

/**
 * @brief Send data via TCP
 * @param sock Socket descriptor
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, or -1 on error
 */
int doip_socket_tcp_send(DoIPSocket_t sock, const uint8_t* data, size_t len);

/**
 * @brief Receive data via TCP
 * @param sock Socket descriptor
 * @param buf Receive buffer
 * @param cap Buffer capacity
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes received, 0 on timeout, -1 on error
 */
int doip_socket_tcp_recv(DoIPSocket_t sock, uint8_t* buf, size_t cap, uint32_t timeout_ms);

/**
 * @brief Send UDP broadcast
 * @param sock Socket descriptor
 * @param data Data to send
 * @param len Length of data
 * @param port Destination port
 * @return Number of bytes sent, or -1 on error
 */
int doip_socket_udp_broadcast(DoIPSocket_t sock, const uint8_t* data, size_t len, uint16_t port);

/**
 * @brief Receive UDP datagram
 * @param sock Socket descriptor
 * @param buf Receive buffer
 * @param cap Buffer capacity
 * @param timeout_ms Timeout in milliseconds
 * @param src_ip Output: source IP (can be NULL)
 * @return Number of bytes received, 0 on timeout, -1 on error
 */
int doip_socket_udp_recv(DoIPSocket_t sock, uint8_t* buf, size_t cap, uint32_t timeout_ms, uint32_t* src_ip);

/**
 * @brief Close socket
 * @param sock Socket descriptor
 */
void doip_socket_close(DoIPSocket_t sock);

/**
 * @brief Convert IP string to 32-bit value
 * @param ip_str IP string (e.g., "192.168.1.100")
 * @param ip_out Output: IP in network byte order
 * @return 0 on success, -1 on error
 */
int doip_ip_str_to_addr(const char* ip_str, uint32_t* ip_out);

#ifdef __cplusplus
}
#endif

#endif /* DOIP_CLIENT_H */

