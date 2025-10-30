/**
 * mbedTLS DoIP Server/Client for TC375
 * No PQC - using standard TLS 1.3
 */

#ifndef MBEDTLS_DOIP_H
#define MBEDTLS_DOIP_H

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
#include <stdint.h>

// mbedTLS DoIP Server
typedef struct {
    mbedtls_net_context listen_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
    mbedtls_x509_crt cacert;
    uint16_t port;
} mbedtls_doip_server;

// mbedTLS DoIP Client
typedef struct {
    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
} mbedtls_doip_client;

// Server functions
int mbedtls_doip_server_init(mbedtls_doip_server* server,
                             const char* cert_file,
                             const char* key_file,
                             const char* ca_file,
                             uint16_t port);

int mbedtls_doip_server_accept(mbedtls_doip_server* server,
                               mbedtls_ssl_context* client_ssl);

void mbedtls_doip_server_free(mbedtls_doip_server* server);

// Client functions
int mbedtls_doip_client_init(mbedtls_doip_client* client,
                             const char* host,
                             uint16_t port,
                             const char* cert_file,
                             const char* key_file,
                             const char* ca_file);

int mbedtls_doip_client_write(mbedtls_doip_client* client,
                              const unsigned char* buf,
                              size_t len);

int mbedtls_doip_client_read(mbedtls_doip_client* client,
                             unsigned char* buf,
                             size_t len);

void mbedtls_doip_client_free(mbedtls_doip_client* client);

#endif // MBEDTLS_DOIP_H

