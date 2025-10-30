/**
 * @file doip_client_mbedtls.c
 * @brief DoIP Client with mbedTLS Implementation
 */

#include "doip_client_mbedtls.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include full mbedTLS types
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>

// mbedTLS DoIP Client (full definition)
struct mbedtls_doip_client {
    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
};

int doip_client_mbedtls_init(mbedtls_doip_client** client,
                              const char* vmg_host,
                              uint16_t vmg_port,
                              const char* cert_file,
                              const char* key_file,
                              const char* ca_file) {
    int ret;
    const char* pers = "doip_client";
    char port_str[16];
    
    *client = (mbedtls_doip_client*)malloc(sizeof(mbedtls_doip_client));
    if (!*client) {
        return -1;
    }
    
    mbedtls_doip_client* ctx = *client;
    
    // Initialize contexts
    mbedtls_net_init(&ctx->server_fd);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_x509_crt_init(&ctx->cacert);
    mbedtls_x509_crt_init(&ctx->clicert);
    mbedtls_pk_init(&ctx->pkey);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    
    // Seed RNG
    if ((ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func,
                                     &ctx->entropy,
                                     (const unsigned char*)pers,
                                     strlen(pers))) != 0) {
        printf("[DoIP Client] Failed to seed RNG: -0x%x\n", -ret);
        goto error;
    }
    
    // Load CA certificate
    if ((ret = mbedtls_x509_crt_parse_file(&ctx->cacert, ca_file)) != 0) {
        printf("[DoIP Client] Failed to load CA cert: -0x%x\n", -ret);
        goto error;
    }
    
    // Load client certificate
    if ((ret = mbedtls_x509_crt_parse_file(&ctx->clicert, cert_file)) != 0) {
        printf("[DoIP Client] Failed to load client cert: -0x%x\n", -ret);
        goto error;
    }
    
    // Load client private key
    if ((ret = mbedtls_pk_parse_keyfile(&ctx->pkey, key_file, NULL, 
                                        mbedtls_ctr_drbg_random, 
                                        &ctx->ctr_drbg)) != 0) {
        printf("[DoIP Client] Failed to load private key: -0x%x\n", -ret);
        goto error;
    }
    
    // Connect to VMG
    snprintf(port_str, sizeof(port_str), "%u", vmg_port);
    if ((ret = mbedtls_net_connect(&ctx->server_fd, vmg_host, port_str,
                                   MBEDTLS_NET_PROTO_TCP)) != 0) {
        printf("[DoIP Client] Failed to connect to %s:%u: -0x%x\n", 
               vmg_host, vmg_port, -ret);
        goto error;
    }
    
    printf("[DoIP Client] Connected to %s:%u\n", vmg_host, vmg_port);
    
    // Setup SSL/TLS
    if ((ret = mbedtls_ssl_config_defaults(&ctx->conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        printf("[DoIP Client] Failed to set SSL defaults: -0x%x\n", -ret);
        goto error;
    }
    
    // Configure mTLS
    mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL);
    mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->clicert, &ctx->pkey);
    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    
    // Setup SSL context
    if ((ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf)) != 0) {
        printf("[DoIP Client] Failed to setup SSL: -0x%x\n", -ret);
        goto error;
    }
    
    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->server_fd, mbedtls_net_send,
                        mbedtls_net_recv, NULL);
    
    // Perform TLS handshake
    printf("[DoIP Client] Performing TLS handshake...\n");
    while ((ret = mbedtls_ssl_handshake(&ctx->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && 
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("[DoIP Client] TLS handshake failed: -0x%x\n", -ret);
            goto error;
        }
    }
    
    printf("[DoIP Client] TLS handshake successful\n");
    printf("[DoIP Client] Cipher suite: %s\n", 
           mbedtls_ssl_get_ciphersuite(&ctx->ssl));
    printf("[DoIP Client] Protocol version: %s\n", 
           mbedtls_ssl_get_version(&ctx->ssl));
    
    return 0;

error:
    doip_client_mbedtls_free(*client);
    *client = NULL;
    return -1;
}

int doip_client_mbedtls_send(mbedtls_doip_client* client,
                              const unsigned char* data,
                              size_t len) {
    if (!client) return -1;
    
    int ret = mbedtls_ssl_write(&client->ssl, data, len);
    if (ret < 0) {
        printf("[DoIP Client] Write error: -0x%x\n", -ret);
        return -1;
    }
    
    return ret;
}

int doip_client_mbedtls_receive(mbedtls_doip_client* client,
                                 unsigned char* buffer,
                                 size_t buffer_size) {
    if (!client) return -1;
    
    int ret = mbedtls_ssl_read(&client->ssl, buffer, buffer_size);
    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        printf("[DoIP Client] Connection closed by peer\n");
        return 0;
    }
    
    if (ret < 0) {
        printf("[DoIP Client] Read error: -0x%x\n", -ret);
        return -1;
    }
    
    return ret;
}

void doip_client_mbedtls_free(mbedtls_doip_client* client) {
    if (!client) return;
    
    mbedtls_ssl_close_notify(&client->ssl);
    mbedtls_net_free(&client->server_fd);
    mbedtls_x509_crt_free(&client->cacert);
    mbedtls_x509_crt_free(&client->clicert);
    mbedtls_pk_free(&client->pkey);
    mbedtls_ssl_free(&client->ssl);
    mbedtls_ssl_config_free(&client->conf);
    mbedtls_ctr_drbg_free(&client->ctr_drbg);
    mbedtls_entropy_free(&client->entropy);
    
    free(client);
}

