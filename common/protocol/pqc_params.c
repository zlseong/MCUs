/**
 * @file pqc_params.c
 * @brief PQC Parameter Implementation - 13 Combinations
 */

#include "pqc_params.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Configuration Table (13 Combinations)
// ============================================================================

static const PQC_Config PQC_CONFIG_TABLE[PQC_CONFIG_COUNT] = {
    // [0] X25519 + ECDSA-P256 (Classical baseline)
    {
        .kem = PQC_KEM_X25519,
        .sig_type = 0,
        .sig = {.ecdsa = PQC_ECDSA_P256},
        .kem_name = "X25519",
        .sig_name = "ECDSA-P256",
        .openssl_groups = "x25519",
        .openssl_sigalgs = "ecdsa_secp256r1_sha256",
        .kem_public_key_size = X25519_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = X25519_PUBLIC_KEY_SIZE,
        .sig_public_key_size = ECDSA_P256_PUBLIC_KEY_SIZE,
        .sig_signature_size = ECDSA_P256_SIGNATURE_SIZE,
        .security_bits = 128
    },
    
    // [1-3] ML-KEM + ECDSA-P256 (Hybrid)
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
    
    // [4-6] ML-KEM-512 + ML-DSA (Pure PQC, 128-bit)
    {
        .kem = PQC_KEM_MLKEM512,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA44},
        .kem_name = "ML-KEM-512",
        .sig_name = "ML-DSA-44",
        .openssl_groups = "mlkem512",
        .openssl_sigalgs = "dilithium2",
        .kem_public_key_size = MLKEM512_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM512_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA44_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA44_SIGNATURE_SIZE,
        .security_bits = 128
    },
    {
        .kem = PQC_KEM_MLKEM512,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA65},
        .kem_name = "ML-KEM-512",
        .sig_name = "ML-DSA-65",
        .openssl_groups = "mlkem512",
        .openssl_sigalgs = "dilithium3",
        .kem_public_key_size = MLKEM512_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM512_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA65_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA65_SIGNATURE_SIZE,
        .security_bits = 128
    },
    {
        .kem = PQC_KEM_MLKEM512,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA87},
        .kem_name = "ML-KEM-512",
        .sig_name = "ML-DSA-87",
        .openssl_groups = "mlkem512",
        .openssl_sigalgs = "dilithium5",
        .kem_public_key_size = MLKEM512_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM512_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA87_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA87_SIGNATURE_SIZE,
        .security_bits = 128
    },
    
    // [7-9] ML-KEM-768 + ML-DSA (Pure PQC, 192-bit)
    {
        .kem = PQC_KEM_MLKEM768,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA44},
        .kem_name = "ML-KEM-768",
        .sig_name = "ML-DSA-44",
        .openssl_groups = "mlkem768",
        .openssl_sigalgs = "dilithium2",
        .kem_public_key_size = MLKEM768_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM768_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA44_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA44_SIGNATURE_SIZE,
        .security_bits = 192
    },
    {
        .kem = PQC_KEM_MLKEM768,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA65},
        .kem_name = "ML-KEM-768",
        .sig_name = "ML-DSA-65",
        .openssl_groups = "mlkem768",
        .openssl_sigalgs = "dilithium3",
        .kem_public_key_size = MLKEM768_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM768_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA65_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA65_SIGNATURE_SIZE,
        .security_bits = 192
    },
    {
        .kem = PQC_KEM_MLKEM768,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA87},
        .kem_name = "ML-KEM-768",
        .sig_name = "ML-DSA-87",
        .openssl_groups = "mlkem768",
        .openssl_sigalgs = "dilithium5",
        .kem_public_key_size = MLKEM768_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM768_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA87_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA87_SIGNATURE_SIZE,
        .security_bits = 192
    },
    
    // [10-12] ML-KEM-1024 + ML-DSA (Pure PQC, 256-bit)
    {
        .kem = PQC_KEM_MLKEM1024,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA44},
        .kem_name = "ML-KEM-1024",
        .sig_name = "ML-DSA-44",
        .openssl_groups = "mlkem1024",
        .openssl_sigalgs = "dilithium2",
        .kem_public_key_size = MLKEM1024_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM1024_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA44_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA44_SIGNATURE_SIZE,
        .security_bits = 256
    },
    {
        .kem = PQC_KEM_MLKEM1024,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA65},
        .kem_name = "ML-KEM-1024",
        .sig_name = "ML-DSA-65",
        .openssl_groups = "mlkem1024",
        .openssl_sigalgs = "dilithium3",
        .kem_public_key_size = MLKEM1024_PUBLIC_KEY_SIZE,
        .kem_ciphertext_size = MLKEM1024_CIPHERTEXT_SIZE,
        .sig_public_key_size = MLDSA65_PUBLIC_KEY_SIZE,
        .sig_signature_size = MLDSA65_SIGNATURE_SIZE,
        .security_bits = 256
    },
    {
        .kem = PQC_KEM_MLKEM1024,
        .sig_type = 1,
        .sig = {.mldsa = PQC_SIG_MLDSA87},
        .kem_name = "ML-KEM-1024",
        .sig_name = "ML-DSA-87",
        .openssl_groups = "mlkem1024",
        .openssl_sigalgs = "dilithium5",
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

void pqc_print_config(const PQC_Config* config) {
    if (!config) {
        printf("[PQC] Invalid configuration\n");
        return;
    }
    
    printf("[PQC] Configuration:\n");
    printf("      KEM:      %s\n", config->kem_name);
    printf("      SIG:      %s\n", config->sig_name);
    printf("      Security: %u-bit\n", config->security_bits);
    printf("      Type:     %s\n", 
           config->sig_type == 0 ? "Hybrid/Classical" : "Pure PQC");
}

void pqc_print_all_configs(void) {
    printf("\n============================================\n");
    printf("  Available PQC Configurations (13 total)\n");
    printf("============================================\n\n");
    
    for (uint8_t i = 0; i < PQC_CONFIG_COUNT; i++) {
        const PQC_Config* cfg = &PQC_CONFIG_TABLE[i];
        printf("[%2d] %-12s + %-12s (%3u-bit) %s\n",
               i,
               cfg->kem_name,
               cfg->sig_name,
               cfg->security_bits,
               i == PQC_CONFIG_RECOMMENDED ? "<- DEFAULT" :
               i == PQC_CONFIG_PURE_PQC ? "<- PURE PQC" : "");
    }
    
    printf("\n============================================\n");
    printf("  Recommended: [%d] %s + %s\n",
           PQC_CONFIG_RECOMMENDED,
           PQC_CONFIG_TABLE[PQC_CONFIG_RECOMMENDED].kem_name,
           PQC_CONFIG_TABLE[PQC_CONFIG_RECOMMENDED].sig_name);
    printf("============================================\n\n");
}

size_t pqc_estimate_handshake_size(const PQC_Config* config) {
    if (!config) {
        return 0;
    }
    
    // Rough estimate: KEM exchange + Signature verification
    return config->kem_public_key_size + 
           config->kem_ciphertext_size +
           config->sig_public_key_size +
           config->sig_signature_size +
           1024;  // TLS overhead
}
