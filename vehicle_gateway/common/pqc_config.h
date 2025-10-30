/**
 * PQC Configuration for VMG
 * Based on Benchmark_mTLS_with_PQC project
 */

#ifndef PQC_CONFIG_H
#define PQC_CONFIG_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdint.h>

// PQC Algorithm Types (Pure PQC only)
typedef enum {
    KEM_MLKEM512 = 0,       // Pure PQC (128-bit security)
    KEM_MLKEM768,           // Pure PQC (192-bit security) - RECOMMENDED
    KEM_MLKEM1024           // Pure PQC (256-bit security)
} PQC_KEM_Type;

typedef enum {
    SIG_ECDSA_P256 = 0,    // ECDSA with P-256 curve
    SIG_MLDSA44,           // ML-DSA-44 (Dilithium2, 128-bit)
    SIG_MLDSA65,           // ML-DSA-65 (Dilithium3, 192-bit) - RECOMMENDED
    SIG_MLDSA87            // ML-DSA-87 (Dilithium5, 256-bit)
} PQC_SIG_Type;

// Algorithm Configuration
typedef struct {
    PQC_KEM_Type kem;
    PQC_SIG_Type sig;
    const char* kem_name;
    const char* sig_name;
    const char* openssl_groups;
    const char* openssl_sigalgs;
} PQC_Config;

// Predefined configurations (ML-KEM + ML-DSA or ECDSA)
static const PQC_Config PQC_CONFIGS[] = {
    // ML-KEM + ECDSA (Lighter signature)
    {KEM_MLKEM512, SIG_ECDSA_P256,
     "ML-KEM-512", "ECDSA-P256",
     "mlkem512", "ecdsa_secp256r1_sha256"},
    
    {KEM_MLKEM768, SIG_ECDSA_P256,
     "ML-KEM-768", "ECDSA-P256",
     "mlkem768", "ecdsa_secp256r1_sha256"},
    
    {KEM_MLKEM1024, SIG_ECDSA_P256,
     "ML-KEM-1024", "ECDSA-P256",
     "mlkem1024", "ecdsa_secp256r1_sha256"},
    
    // ML-KEM + ML-DSA (Pure PQC)
    {KEM_MLKEM512, SIG_MLDSA44,
     "ML-KEM-512", "ML-DSA-44",
     "mlkem512", "mldsa44"},
    
    {KEM_MLKEM768, SIG_MLDSA65,
     "ML-KEM-768", "ML-DSA-65",
     "mlkem768", "mldsa65"},
    
    {KEM_MLKEM1024, SIG_MLDSA87,
     "ML-KEM-1024", "ML-DSA-87",
     "mlkem1024", "mldsa87"}
};

#define PQC_CONFIG_COUNT (sizeof(PQC_CONFIGS) / sizeof(PQC_Config))

// Function prototypes
int pqc_configure_ssl_ctx(SSL_CTX* ctx, const PQC_Config* config);
int pqc_load_certificates(SSL_CTX* ctx, 
                          const char* cert_file,
                          const char* key_file,
                          const char* ca_file);
const char* pqc_get_cert_filename(const PQC_Config* config, int is_server);
void pqc_print_config(const PQC_Config* config);

#endif // PQC_CONFIG_H

