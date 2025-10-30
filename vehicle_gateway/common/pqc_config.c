/**
 * PQC Configuration Implementation
 */

#include "pqc_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int pqc_configure_ssl_ctx(SSL_CTX* ctx, const PQC_Config* config) {
    if (!ctx || !config) return 0;
    
    // TLS 1.3 only
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    
    // Set KEM groups
    if (SSL_CTX_set1_groups_list(ctx, config->openssl_groups) != 1) {
        fprintf(stderr, "Failed to set groups: %s\n", config->openssl_groups);
        ERR_print_errors_fp(stderr);
        return 0;
    }
    
    // Set signature algorithms
    if (SSL_CTX_set1_sigalgs_list(ctx, config->openssl_sigalgs) != 1) {
        fprintf(stderr, "Failed to set sigalgs: %s\n", config->openssl_sigalgs);
        ERR_print_errors_fp(stderr);
        return 0;
    }
    
    // Cipher suite (TLS 1.3 default)
    if (SSL_CTX_set_ciphersuites(ctx, "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256") != 1) {
        fprintf(stderr, "Failed to set cipher suites\n");
        ERR_print_errors_fp(stderr);
        return 0;
    }
    
    printf("[PQC] Configured: KEM=%s, SIG=%s\n", 
           config->kem_name, config->sig_name);
    
    return 1;
}

int pqc_load_certificates(SSL_CTX* ctx,
                         const char* cert_file,
                         const char* key_file,
                         const char* ca_file) {
    if (!ctx) return 0;
    
    // Load certificate
    if (cert_file && SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "Failed to load certificate: %s\n", cert_file);
        ERR_print_errors_fp(stderr);
        return 0;
    }
    
    // Load private key
    if (key_file && SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "Failed to load private key: %s\n", key_file);
        ERR_print_errors_fp(stderr);
        return 0;
    }
    
    // Verify private key
    if (cert_file && key_file && !SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match certificate\n");
        ERR_print_errors_fp(stderr);
        return 0;
    }
    
    // Load CA certificate
    if (ca_file && SSL_CTX_load_verify_locations(ctx, ca_file, NULL) != 1) {
        fprintf(stderr, "Failed to load CA certificate: %s\n", ca_file);
        ERR_print_errors_fp(stderr);
        return 0;
    }
    
    // Verify peer
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    
    return 1;
}

const char* pqc_get_cert_filename(const PQC_Config* config, int is_server) {
    static char filename[256];
    
    const char* kem_prefix;
    const char* sig_prefix;
    
    // KEM name mapping
    switch (config->kem) {
        case KEM_MLKEM512: kem_prefix = "mlkem512"; break;
        case KEM_MLKEM768: kem_prefix = "mlkem768"; break;
        case KEM_MLKEM1024: kem_prefix = "mlkem1024"; break;
        default: kem_prefix = "mlkem768"; break;
    }
    
    // SIG name mapping
    switch (config->sig) {
        case SIG_ECDSA_P256: sig_prefix = "ecdsa_secp256r1_sha256"; break;
        case SIG_MLDSA44: sig_prefix = "mldsa44"; break;
        case SIG_MLDSA65: sig_prefix = "mldsa65"; break;
        case SIG_MLDSA87: sig_prefix = "mldsa87"; break;
        default: sig_prefix = "mldsa65"; break;
    }
    
    snprintf(filename, sizeof(filename), "%s_%s_%s",
             kem_prefix, sig_prefix, is_server ? "server" : "client");
    
    return filename;
}

void pqc_print_config(const PQC_Config* config) {
    printf("========================================\n");
    printf("PQC Configuration\n");
    printf("========================================\n");
    printf("KEM:         %s\n", config->kem_name);
    printf("Signature:   %s\n", config->sig_name);
    printf("OpenSSL Groups:  %s\n", config->openssl_groups);
    printf("OpenSSL Sigalgs: %s\n", config->openssl_sigalgs);
    printf("========================================\n");
}

