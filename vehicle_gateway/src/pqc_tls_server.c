/**
 * Pure PQC TLS Server for VMG DoIP
 * Based on Benchmark_mTLS_with_PQC
 */

#include "pqc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define LISTEN_BACKLOG 10

typedef struct {
    SSL_CTX* ctx;
    int listen_fd;
    uint16_t port;
    const PQC_Config* config;
} PQC_Server;

static int create_listen_socket(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    
    if (listen(fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    
    printf("[Server] Listening on port %u\n", port);
    return fd;
}

PQC_Server* pqc_server_create(uint16_t port, const PQC_Config* config,
                              const char* cert_file,
                              const char* key_file,
                              const char* ca_file) {
    PQC_Server* server = calloc(1, sizeof(PQC_Server));
    if (!server) return NULL;
    
    server->port = port;
    server->config = config;
    
    // Create SSL context
    server->ctx = SSL_CTX_new(TLS_server_method());
    if (!server->ctx) {
        fprintf(stderr, "Failed to create SSL context\n");
        free(server);
        return NULL;
    }
    
    // Configure PQC
    if (!pqc_configure_ssl_ctx(server->ctx, config)) {
        SSL_CTX_free(server->ctx);
        free(server);
        return NULL;
    }
    
    // Load certificates
    if (!pqc_load_certificates(server->ctx, cert_file, key_file, ca_file)) {
        SSL_CTX_free(server->ctx);
        free(server);
        return NULL;
    }
    
    // Create listen socket
    server->listen_fd = create_listen_socket(port);
    if (server->listen_fd < 0) {
        SSL_CTX_free(server->ctx);
        free(server);
        return NULL;
    }
    
    return server;
}

int pqc_server_accept(PQC_Server* server, SSL** out_ssl, int* out_fd) {
    if (!server || !out_ssl || !out_fd) return 0;
    
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    
    int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &len);
    if (client_fd < 0) {
        perror("accept");
        return 0;
    }
    
    printf("[Server] Client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
    
    SSL* ssl = SSL_new(server->ctx);
    if (!ssl) {
        close(client_fd);
        return 0;
    }
    
    SSL_set_fd(ssl, client_fd);
    
    // TLS handshake
    if (SSL_accept(ssl) <= 0) {
        fprintf(stderr, "[Server] TLS handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(client_fd);
        return 0;
    }
    
    printf("[Server] TLS handshake successful\n");
    printf("[Server] Cipher: %s\n", SSL_get_cipher(ssl));
    printf("[Server] Protocol: %s\n", SSL_get_version(ssl));
    
    *out_ssl = ssl;
    *out_fd = client_fd;
    
    return 1;
}

void pqc_server_destroy(PQC_Server* server) {
    if (!server) return;
    
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }
    
    if (server->ctx) {
        SSL_CTX_free(server->ctx);
    }
    
    free(server);
}

// Example usage for DoIP
int pqc_doip_server_example(uint16_t port) {
    // Use ML-KEM-768 + ML-DSA-65 (recommended)
    const PQC_Config* config = &PQC_CONFIGS[1]; // mlkem768 + mldsa65
    
    pqc_print_config(config);
    
    PQC_Server* server = pqc_server_create(
        port, config,
        "certs/mlkem768_mldsa65_server.crt",
        "certs/mlkem768_mldsa65_server.key",
        "certs/ca.crt"
    );
    
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return -1;
    }
    
    printf("[VMG] DoIP Server started with PQC-Hybrid TLS\n");
    
    while (1) {
        SSL* ssl = NULL;
        int client_fd = 0;
        
        if (pqc_server_accept(server, &ssl, &client_fd)) {
            // Handle DoIP communication here
            // For now, just echo
            char buf[4096];
            int n = SSL_read(ssl, buf, sizeof(buf));
            if (n > 0) {
                printf("[Server] Received %d bytes\n", n);
                SSL_write(ssl, buf, n);
            }
            
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(client_fd);
        }
    }
    
    pqc_server_destroy(server);
    return 0;
}

