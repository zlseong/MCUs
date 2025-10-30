/**
 * @file doip_client_mbedtls.h
 * @brief DoIP Client with mbedTLS for TC375
 * 
 * Standard TLS 1.3 (No PQC) for connecting to VMG
 */

#ifndef DOIP_CLIENT_MBEDTLS_H
#define DOIP_CLIENT_MBEDTLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration (mbedTLS types)
typedef struct mbedtls_doip_client mbedtls_doip_client;

/**
 * @brief Initialize DoIP client with mbedTLS
 * 
 * @param client Client context
 * @param vmg_host VMG IP address (e.g., "192.168.1.1")
 * @param vmg_port VMG DoIP port (default: 13400)
 * @param cert_file Client certificate file
 * @param key_file Client private key file
 * @param ca_file CA certificate file
 * @return 0 on success, -1 on error
 */
int doip_client_mbedtls_init(mbedtls_doip_client** client,
                              const char* vmg_host,
                              uint16_t vmg_port,
                              const char* cert_file,
                              const char* key_file,
                              const char* ca_file);

/**
 * @brief Send DoIP message to VMG
 * 
 * @param client Client context
 * @param data Message data
 * @param len Message length
 * @return Number of bytes sent, or -1 on error
 */
int doip_client_mbedtls_send(mbedtls_doip_client* client,
                              const unsigned char* data,
                              size_t len);

/**
 * @brief Receive DoIP message from VMG
 * 
 * @param client Client context
 * @param buffer Receive buffer
 * @param buffer_size Buffer size
 * @return Number of bytes received, or -1 on error
 */
int doip_client_mbedtls_receive(mbedtls_doip_client* client,
                                 unsigned char* buffer,
                                 size_t buffer_size);

/**
 * @brief Close connection and free resources
 * 
 * @param client Client context
 */
void doip_client_mbedtls_free(mbedtls_doip_client* client);

#ifdef __cplusplus
}
#endif

#endif // DOIP_CLIENT_MBEDTLS_H

