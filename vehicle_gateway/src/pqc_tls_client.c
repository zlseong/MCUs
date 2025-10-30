/**
 * Pure PQC TLS Client for VMG HTTPS/MQTT
 */

#include "pqc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

typedef struct {
    SSL_CTX* ctx;
    SSL* ssl;
    int fd;
    const PQC_Config* config;
} PQC_Client;

static int connect_to_host(const char* hostname, uint16_t port) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);
    
    struct addrinfo* result;
    if (getaddrinfo(hostname, port_str, &hints, &result) != 0) {
        fprintf(stderr, "Failed to resolve: %s\n", hostname);
        return -1;
    }
    
    int fd = -1;
    for (struct addrinfo* rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        
        close(fd);
        fd = -1;
    }
    
    freeaddrinfo(result);
    
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%u\n", hostname, port);
    }
    
    return fd;
}

PQC_Client* pqc_client_create(const char* hostname, uint16_t port,
                              const PQC_Config* config,
                              const char* cert_file,
                              const char* key_file,
                              const char* ca_file) {
    PQC_Client* client = calloc(1, sizeof(PQC_Client));
    if (!client) return NULL;
    
    client->config = config;
    
    // Create SSL context
    client->ctx = SSL_CTX_new(TLS_client_method());
    if (!client->ctx) {
        free(client);
        return NULL;
    }
    
    // Configure PQC
    if (!pqc_configure_ssl_ctx(client->ctx, config)) {
        SSL_CTX_free(client->ctx);
        free(client);
        return NULL;
    }
    
    // Load certificates (for mTLS)
    if (!pqc_load_certificates(client->ctx, cert_file, key_file, ca_file)) {
        SSL_CTX_free(client->ctx);
        free(client);
        return NULL;
    }
    
    // Connect
    client->fd = connect_to_host(hostname, port);
    if (client->fd < 0) {
        SSL_CTX_free(client->ctx);
        free(client);
        return NULL;
    }
    
    printf("[Client] Connected to %s:%u\n", hostname, port);
    
    // Create SSL
    client->ssl = SSL_new(client->ctx);
    if (!client->ssl) {
        close(client->fd);
        SSL_CTX_free(client->ctx);
        free(client);
        return NULL;
    }
    
    SSL_set_fd(client->ssl, client->fd);
    SSL_set_tlsext_host_name(client->ssl, hostname);
    
    // TLS handshake
    if (SSL_connect(client->ssl) <= 0) {
        fprintf(stderr, "[Client] TLS handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(client->ssl);
        close(client->fd);
        SSL_CTX_free(client->ctx);
        free(client);
        return NULL;
    }
    
    printf("[Client] TLS handshake successful\n");
    printf("[Client] Cipher: %s\n", SSL_get_cipher(client->ssl));
    printf("[Client] Protocol: %s\n", SSL_get_version(client->ssl));
    
    return client;
}

int pqc_client_write(PQC_Client* client, const void* data, size_t len) {
    if (!client || !client->ssl) return -1;
    return SSL_write(client->ssl, data, len);
}

int pqc_client_read(PQC_Client* client, void* buf, size_t len) {
    if (!client || !client->ssl) return -1;
    return SSL_read(client->ssl, buf, len);
}

void pqc_client_destroy(PQC_Client* client) {
    if (!client) return;
    
    if (client->ssl) {
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);
    }
    
    if (client->fd >= 0) {
        close(client->fd);
    }
    
    if (client->ctx) {
        SSL_CTX_free(client->ctx);
    }
    
    free(client);
}

