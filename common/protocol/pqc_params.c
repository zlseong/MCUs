/**
 * @file pqc_params.c
 * @brief PQC Parameter Implementation
 */

#include "pqc_params.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Configuration Table
// ============================================================================

static const PQC_Config PQC_CONFIG_TABLE[PQC_CONFIG_COUNT] = {
    // ML-KEM-512 + ECDSA combinations
    {
        .kem = PQC_KEM_MLKEM512,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P256},
        .kem_name = "ML-KEM-512",
        .sig_name = "ECDSA-P256",
        .openssl_groups = "mlkem512",
        .openssl_sigalgs = "ecdsa_secp256r1_sha256",
        .kem_public_key_size = MLKEM512_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM512_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P256_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P256_SIGNATURE_SIZE,
        .security_bits = 128
    },
    {
        .kem = PQC_KEM_MLKEM512,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P384},
        .kem_name = "ML-KEM-512",
        .sig_name = "ECDSA-P384",
        .openssl_groups = "mlkem512",
        .openssl_sigalgs = "ecdsa_secp384r1_sha384",
        .kem_public_key_size = MLKEM512_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM512_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P384_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P384_SIGNATURE_SIZE,
        .security_bits = 128
    },
    {
        .kem = PQC_KEM_MLKEM512,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P521},
        .kem_name = "ML-KEM-512",
        .sig_name = "ECDSA-P521",
        .openssl_groups = "mlkem512",
        .openssl_sigalgs = "ecdsa_secp521r1_sha512",
        .kem_public_key_size = MLKEM512_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM512_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P521_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P521_SIGNATURE_SIZE,
        .security_bits = 128
    },
    
    // ML-KEM-768 + ECDSA combinations
    {
        .kem = PQC_KEM_MLKEM768,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P256},
        .kem_name = "ML-KEM-768",
        .sig_name = "ECDSA-P256",
        .openssl_groups = "mlkem768",
        .openssl_sigalgs = "ecdsa_secp256r1_sha256",
        .kem_public_key_size = MLKEM768_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM768_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P256_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P256_SIGNATURE_SIZE,
        .security_bits = 192
    },
    {
        .kem = PQC_KEM_MLKEM768,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P384},
        .kem_name = "ML-KEM-768",
        .sig_name = "ECDSA-P384",
        .openssl_groups = "mlkem768",
        .openssl_sigalgs = "ecdsa_secp384r1_sha384",
        .kem_public_key_size = MLKEM768_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM768_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P384_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P384_SIGNATURE_SIZE,
        .security_bits = 192
    },
    {
        .kem = PQC_KEM_MLKEM768,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P521},
        .kem_name = "ML-KEM-768",
        .sig_name = "ECDSA-P521",
        .openssl_groups = "mlkem768",
        .openssl_sigalgs = "ecdsa_secp521r1_sha512",
        .kem_public_key_size = MLKEM768_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM768_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P521_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P521_SIGNATURE_SIZE,
        .security_bits = 192
    },
    
    // ML-KEM-1024 + ECDSA combinations
    {
        .kem = PQC_KEM_MLKEM1024,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P256},
        .kem_name = "ML-KEM-1024",
        .sig_name = "ECDSA-P256",
        .openssl_groups = "mlkem1024",
        .openssl_sigalgs = "ecdsa_secp256r1_sha256",
        .kem_public_key_size = MLKEM1024_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM1024_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P256_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P256_SIGNATURE_SIZE,
        .security_bits = 256
    },
    {
        .kem = PQC_KEM_MLKEM1024,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P384},
        .kem_name = "ML-KEM-1024",
        .sig_name = "ECDSA-P384",
        .openssl_groups = "mlkem1024",
        .openssl_sigalgs = "ecdsa_secp384r1_sha384",
        .kem_public_key_size = MLKEM1024_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM1024_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P384_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P384_SIGNATURE_SIZE,
        .security_bits = 256
    },
    {
        .kem = PQC_KEM_MLKEM1024,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P521},
        .kem_name = "ML-KEM-1024",
        .sig_name = "ECDSA-P521",
        .openssl_groups = "mlkem1024",
        .openssl_sigalgs = "ecdsa_secp521r1_sha512",
        .kem_public_key_size = MLKEM1024_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM1024_CIPHERTEXT_SIZE,
        .sig_public_key_size = ECDSA_P521_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P521_SIGNATURE_SIZE,
        .security_bits = 256
    },
    
    // ML-KEM + ML-DSA (Pure PQC) combinations
    {
        .kem = PQC_KEM_MLKEM512,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA44},
        .kem_name = "ML-KEM-512",
        .sig_name = "ML-DSA-44",
        .openssl_groups = "mlkem512",
        .openssl_sigalgs = "mldsa44",
        .kem_public_key_size = MLKEM512_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM512_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA44_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA44_SIGNATURE_SIZE,
        .security_bits = 128
    },
    {
        .kem = PQC_KEM_MLKEM768,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA65},
        .kem_name = "ML-KEM-768",
        .sig_name = "ML-DSA-65",
        .openssl_groups = "mlkem768",
        .openssl_sigalgs = "mldsa65",
        .kem_public_key_size = MLKEM768_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM768_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA65_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA65_SIGNATURE_SIZE,
        .security_bits = 192
    },
    {
        .kem = PQC_KEM_MLKEM1024,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA87},
        .kem_name = "ML-KEM-1024",
        .sig_name = "ML-DSA-87",
        .openssl_groups = "mlkem1024",
        .openssl_sigalgs = "mldsa87",
        .kem_public_key_size = MLKEM1024_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM1024_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA87_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA87_SIGNATURE_SIZE,
        .security_bits = 256
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

const PQC_Config* pqc_get_config(uint8_t config_id) {
    if (config_id >= PQC_CONFIG_COUNT) {
        return NULL;
    }
    return &PQC_CONFIG_TABLE[config_id];
}

const PQC_Config* pqc_find_config(PQC_KEM_Type kem, int8_t sig_mldsa, int8_t sig_ecdsa) {
    for (uint8_t i = 0; i < PQC_CONFIG_COUNT; i++) {
        const PQC_Config* cfg = &PQC_CONFIG_TABLE[i];
        
        if (cfg->kem != kem) continue;
        
        if (sig_mldsa >= 0 && cfg->sig_type == 1) {
            if (cfg->sig.mldsa == (PQC_SIG_Type)sig_mldsa) {
                return cfg;
            }
        } else if (sig_ecdsa >= 0 && cfg->sig_type == 0) {
            if (cfg->sig.ecdsa == (PQC_ECDSA_Type)sig_ecdsa) {
                return cfg;
            }
        }
    }
    return NULL;
}

void pqc_print_config(const PQC_Config* config) {
    if (!config) return;
    
    printf("\n[PQC Configuration]\n");
    printf("  KEM:      %s\n", config->kem_name);
    printf("  Signature: %s\n", config->sig_name);
    printf("  Security:  %d-bit\n", config->security_bits);
    printf("  Type:      %s\n", config->sig_type == 1 ? "Pure PQC" : "Hybrid (PQC KEM + Classical SIG)");
    printf("\n[Sizes]\n");
    printf("  KEM Public Key:  %d bytes\n", config->kem_public_key_size);
    printf("  KEM Ciphertext:  %d bytes\n", config->kem_ciphertext_size);
    printf("  SIG Public Key:  %d bytes\n", config->sig_public_key_size);
    printf("  SIG Signature:   %d bytes\n", config->sig_signature_size);
    printf("\n");
}

const char* pqc_get_config_name(uint8_t config_id) {
    const PQC_Config* cfg = pqc_get_config(config_id);
    if (!cfg) return "Invalid";
    
    static char name_buf[64];
    snprintf(name_buf, sizeof(name_buf), "%s + %s", cfg->kem_name, cfg->sig_name);
    return name_buf;
}

