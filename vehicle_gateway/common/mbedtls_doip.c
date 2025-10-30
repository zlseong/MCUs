/**
 * mbedTLS DoIP Implementation
 * Standard TLS 1.3 for DoIP communication
 */

#include "mbedtls_doip.h"
#include <stdio.h>
#include <string.h>

#define DEBUG_LEVEL 1

static void my_debug(void *ctx, int level,
                    const char *file, int line,
                    const char *str) {
    ((void) level);
    fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
}

// Server implementation
int mbedtls_doip_server_init(mbedtls_doip_server* server,
                             const char* cert_file,
                             const char* key_file,
                             const char* ca_file,
                             uint16_t port) {
    int ret;
    const char *pers = "doip_server";
    
    memset(server, 0, sizeof(mbedtls_doip_server));
    server->port = port;
    
    // Initialize contexts
    mbedtls_net_init(&server->listen_fd);
    mbedtls_ssl_config_init(&server->conf);
    mbedtls_x509_crt_init(&server->srvcert);
    mbedtls_pk_init(&server->pkey);
    mbedtls_x509_crt_init(&server->cacert);
    mbedtls_entropy_init(&server->entropy);
    mbedtls_ctr_drbg_init(&server->ctr_drbg);
    
    // Seed RNG
    if ((ret = mbedtls_ctr_drbg_seed(&server->ctr_drbg, mbedtls_entropy_func,
                                     &server->entropy,
                                     (const unsigned char *)pers,
                                     strlen(pers))) != 0) {
        printf("[mbedTLS] Failed to seed RNG: -0x%x\n", -ret);
        return -1;
    }
    
    // Load server certificate
    if ((ret = mbedtls_x509_crt_parse_file(&server->srvcert, cert_file)) != 0) {
        printf("[mbedTLS] Failed to load cert: -0x%x\n", -ret);
        return -1;
    }
    
    // Load private key
    if ((ret = mbedtls_pk_parse_keyfile(&server->pkey, key_file, NULL,
                                        mbedtls_ctr_drbg_random,
                                        &server->ctr_drbg)) != 0) {
        printf("[mbedTLS] Failed to load key: -0x%x\n", -ret);
        return -1;
    }
    
    // Load CA certificate
    if ((ret = mbedtls_x509_crt_parse_file(&server->cacert, ca_file)) != 0) {
        printf("[mbedTLS] Failed to load CA: -0x%x\n", -ret);
        return -1;
    }
    
    // Setup SSL config
    if ((ret = mbedtls_ssl_config_defaults(&server->conf,
                                          MBEDTLS_SSL_IS_SERVER,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        printf("[mbedTLS] Failed to set defaults: -0x%x\n", -ret);
        return -1;
    }
    
    // TLS 1.3 only
    mbedtls_ssl_conf_min_tls_version(&server->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&server->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    
    mbedtls_ssl_conf_rng(&server->conf, mbedtls_ctr_drbg_random, &server->ctr_drbg);
    mbedtls_ssl_conf_dbg(&server->conf, my_debug, stdout);
    
    // Set CA and require client certificate (mutual TLS)
    mbedtls_ssl_conf_ca_chain(&server->conf, &server->cacert, NULL);
    mbedtls_ssl_conf_authmode(&server->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    
    if ((ret = mbedtls_ssl_conf_own_cert(&server->conf, &server->srvcert,
                                         &server->pkey)) != 0) {
        printf("[mbedTLS] Failed to set own cert: -0x%x\n", -ret);
        return -1;
    }
    
    // Bind and listen
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);
    
    if ((ret = mbedtls_net_bind(&server->listen_fd, NULL, port_str,
                               MBEDTLS_NET_PROTO_TCP)) != 0) {
        printf("[mbedTLS] Failed to bind: -0x%x\n", -ret);
        return -1;
    }
    
    printf("[mbedTLS DoIP] Server listening on port %u\n", port);
    return 0;
}

int mbedtls_doip_server_accept(mbedtls_doip_server* server,
                               mbedtls_ssl_context* client_ssl) {
    int ret;
    mbedtls_net_context client_fd;
    
    mbedtls_net_init(&client_fd);
    mbedtls_ssl_init(client_ssl);
    
    // Accept connection
    if ((ret = mbedtls_net_accept(&server->listen_fd, &client_fd,
                                  NULL, 0, NULL)) != 0) {
        printf("[mbedTLS] Failed to accept: -0x%x\n", -ret);
        return -1;
    }
    
    printf("[mbedTLS DoIP] Client connected\n");
    
    // Setup SSL for this connection
    if ((ret = mbedtls_ssl_setup(client_ssl, &server->conf)) != 0) {
        printf("[mbedTLS] Failed to setup SSL: -0x%x\n", -ret);
        mbedtls_net_free(&client_fd);
        return -1;
    }
    
    mbedtls_ssl_set_bio(client_ssl, &client_fd, mbedtls_net_send,
                       mbedtls_net_recv, NULL);
    
    // Perform handshake
    while ((ret = mbedtls_ssl_handshake(client_ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("[mbedTLS] Handshake failed: -0x%x\n", -ret);
            mbedtls_net_free(&client_fd);
            mbedtls_ssl_free(client_ssl);
            return -1;
        }
    }
    
    printf("[mbedTLS DoIP] Handshake complete\n");
    printf("[mbedTLS DoIP] Cipher: %s\n", mbedtls_ssl_get_ciphersuite(client_ssl));
    
    return 0;
}

void mbedtls_doip_server_free(mbedtls_doip_server* server) {
    mbedtls_net_free(&server->listen_fd);
    mbedtls_x509_crt_free(&server->srvcert);
    mbedtls_pk_free(&server->pkey);
    mbedtls_x509_crt_free(&server->cacert);
    mbedtls_ssl_config_free(&server->conf);
    mbedtls_ctr_drbg_free(&server->ctr_drbg);
    mbedtls_entropy_free(&server->entropy);
}

// Client implementation
int mbedtls_doip_client_init(mbedtls_doip_client* client,
                             const char* host,
                             uint16_t port,
                             const char* cert_file,
                             const char* key_file,
                             const char* ca_file) {
    int ret;
    const char *pers = "doip_client";
    
    memset(client, 0, sizeof(mbedtls_doip_client));
    
    // Initialize
    mbedtls_net_init(&client->server_fd);
    mbedtls_ssl_init(&client->ssl);
    mbedtls_ssl_config_init(&client->conf);
    mbedtls_x509_crt_init(&client->cacert);
    mbedtls_x509_crt_init(&client->clicert);
    mbedtls_pk_init(&client->pkey);
    mbedtls_ctr_drbg_init(&client->ctr_drbg);
    mbedtls_entropy_init(&client->entropy);
    
    // Seed RNG
    if ((ret = mbedtls_ctr_drbg_seed(&client->ctr_drbg, mbedtls_entropy_func,
                                     &client->entropy,
                                     (const unsigned char *)pers,
                                     strlen(pers))) != 0) {
        printf("[mbedTLS] Failed to seed RNG: -0x%x\n", -ret);
        return -1;
    }
    
    // Load CA certificate
    if ((ret = mbedtls_x509_crt_parse_file(&client->cacert, ca_file)) != 0) {
        printf("[mbedTLS] Failed to load CA: -0x%x\n", -ret);
        return -1;
    }
    
    // Load client certificate
    if ((ret = mbedtls_x509_crt_parse_file(&client->clicert, cert_file)) != 0) {
        printf("[mbedTLS] Failed to load cert: -0x%x\n", -ret);
        return -1;
    }
    
    // Load private key
    if ((ret = mbedtls_pk_parse_keyfile(&client->pkey, key_file, NULL,
                                        mbedtls_ctr_drbg_random,
                                        &client->ctr_drbg)) != 0) {
        printf("[mbedTLS] Failed to load key: -0x%x\n", -ret);
        return -1;
    }
    
    // Connect to server
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);
    
    if ((ret = mbedtls_net_connect(&client->server_fd, host, port_str,
                                   MBEDTLS_NET_PROTO_TCP)) != 0) {
        printf("[mbedTLS] Failed to connect: -0x%x\n", -ret);
        return -1;
    }
    
    printf("[mbedTLS DoIP] Connected to %s:%u\n", host, port);
    
    // Setup SSL
    if ((ret = mbedtls_ssl_config_defaults(&client->conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        printf("[mbedTLS] Failed to set defaults: -0x%x\n", -ret);
        return -1;
    }
    
    // TLS 1.3 only
    mbedtls_ssl_conf_min_tls_version(&client->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&client->conf, MBEDTLS_SSL_VERSION_TLS1_3);
    
    mbedtls_ssl_conf_authmode(&client->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&client->conf, &client->cacert, NULL);
    mbedtls_ssl_conf_rng(&client->conf, mbedtls_ctr_drbg_random, &client->ctr_drbg);
    mbedtls_ssl_conf_dbg(&client->conf, my_debug, stdout);
    
    // Set client certificate (mutual TLS)
    if ((ret = mbedtls_ssl_conf_own_cert(&client->conf, &client->clicert,
                                         &client->pkey)) != 0) {
        printf("[mbedTLS] Failed to set client cert: -0x%x\n", -ret);
        return -1;
    }
    
    if ((ret = mbedtls_ssl_setup(&client->ssl, &client->conf)) != 0) {
        printf("[mbedTLS] Failed to setup SSL: -0x%x\n", -ret);
        return -1;
    }
    
    if ((ret = mbedtls_ssl_set_hostname(&client->ssl, host)) != 0) {
        printf("[mbedTLS] Failed to set hostname: -0x%x\n", -ret);
        return -1;
    }
    
    mbedtls_ssl_set_bio(&client->ssl, &client->server_fd,
                       mbedtls_net_send, mbedtls_net_recv, NULL);
    
    // Perform handshake
    while ((ret = mbedtls_ssl_handshake(&client->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("[mbedTLS] Handshake failed: -0x%x\n", -ret);
            return -1;
        }
    }
    
    printf("[mbedTLS DoIP] Handshake complete\n");
    printf("[mbedTLS DoIP] Cipher: %s\n", mbedtls_ssl_get_ciphersuite(&client->ssl));
    
    // Verify peer certificate
    uint32_t flags;
    if ((flags = mbedtls_ssl_get_verify_result(&client->ssl)) != 0) {
        char vrfy_buf[512];
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
        printf("[mbedTLS] Certificate verification failed:\n%s\n", vrfy_buf);
        return -1;
    }
    
    return 0;
}

int mbedtls_doip_client_write(mbedtls_doip_client* client,
                              const unsigned char* buf,
                              size_t len) {
    return mbedtls_ssl_write(&client->ssl, buf, len);
}

int mbedtls_doip_client_read(mbedtls_doip_client* client,
                             unsigned char* buf,
                             size_t len) {
    return mbedtls_ssl_read(&client->ssl, buf, len);
}

void mbedtls_doip_client_free(mbedtls_doip_client* client) {
    mbedtls_ssl_close_notify(&client->ssl);
    mbedtls_net_free(&client->server_fd);
    mbedtls_x509_crt_free(&client->clicert);
    mbedtls_x509_crt_free(&client->cacert);
    mbedtls_pk_free(&client->pkey);
    mbedtls_ssl_free(&client->ssl);
    mbedtls_ssl_config_free(&client->conf);
    mbedtls_ctr_drbg_free(&client->ctr_drbg);
    mbedtls_entropy_free(&client->entropy);
}

