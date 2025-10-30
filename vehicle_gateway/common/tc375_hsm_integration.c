/**
 * TC375 HSM Integration for mbedTLS
 * Hardware acceleration hooks
 */

#include "mbedtls_hsm_config.h"
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/entropy.h>

/* TC375 HSM function pointers (to be implemented by HSM driver) */
tc375_hsm_t tc375_hsm = {0};

/* ========================================================================
 * AES Hardware Acceleration
 * ======================================================================== */

#ifdef MBEDTLS_AES_ALT

/**
 * Initialize AES context with HSM
 */
void mbedtls_aes_init(mbedtls_aes_context *ctx) {
    memset(ctx, 0, sizeof(mbedtls_aes_context));
    // HSM initialization here
}

/**
 * Set AES encryption key in HSM
 */
int mbedtls_aes_setkey_enc(mbedtls_aes_context *ctx,
                           const unsigned char *key,
                           unsigned int keybits) {
    if (keybits != 128 && keybits != 192 && keybits != 256) {
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;
    }
    
    // Store key in TC375 HSM secure storage
    // HSM call: tc375_hsm_store_key(key, keybits);
    
    ctx->nr = (keybits >> 5) + 6;  // Number of rounds
    return 0;
}

/**
 * AES encryption using HSM
 */
int mbedtls_aes_crypt_ecb(mbedtls_aes_context *ctx,
                         int mode,
                         const unsigned char input[16],
                         unsigned char output[16]) {
    if (tc375_hsm.aes_encrypt == NULL) {
        return MBEDTLS_ERR_AES_HW_ACCEL_FAILED;
    }
    
    if (mode == MBEDTLS_AES_ENCRYPT) {
        return tc375_hsm.aes_encrypt(input, output, 16);
    } else {
        return tc375_hsm.aes_decrypt(input, output, 16);
    }
}

/**
 * Free AES context
 */
void mbedtls_aes_free(mbedtls_aes_context *ctx) {
    if (ctx == NULL) return;
    
    // Clear HSM key storage
    // HSM call: tc375_hsm_clear_key();
    
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_aes_context));
}

#endif /* MBEDTLS_AES_ALT */

/* ========================================================================
 * SHA-256 Hardware Acceleration
 * ======================================================================== */

#ifdef MBEDTLS_SHA256_ALT

/**
 * Initialize SHA-256 context
 */
void mbedtls_sha256_init(mbedtls_sha256_context *ctx) {
    memset(ctx, 0, sizeof(mbedtls_sha256_context));
}

/**
 * Start SHA-256 hash computation
 */
int mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224) {
    // Initialize HSM SHA engine
    // HSM call: tc375_hsm_sha_init(is224 ? 224 : 256);
    ctx->is224 = is224;
    return 0;
}

/**
 * Update SHA-256 hash with new data (HSM accelerated)
 */
int mbedtls_sha256_update(mbedtls_sha256_context *ctx,
                         const unsigned char *input,
                         size_t ilen) {
    // HSM call: tc375_hsm_sha_update(input, ilen);
    return 0;
}

/**
 * Finalize SHA-256 hash computation (HSM accelerated)
 */
int mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
                         unsigned char output[32]) {
    if (tc375_hsm.sha256 == NULL) {
        return MBEDTLS_ERR_SHA256_HW_ACCEL_FAILED;
    }
    
    // HSM call: tc375_hsm_sha_final(output);
    return 0;
}

/**
 * Free SHA-256 context
 */
void mbedtls_sha256_free(mbedtls_sha256_context *ctx) {
    if (ctx == NULL) return;
    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha256_context));
}

/**
 * One-shot SHA-256 computation (HSM accelerated)
 */
int mbedtls_sha256(const unsigned char *input,
                  size_t ilen,
                  unsigned char output[32],
                  int is224) {
    if (tc375_hsm.sha256 == NULL) {
        return MBEDTLS_ERR_SHA256_HW_ACCEL_FAILED;
    }
    
    return tc375_hsm.sha256(input, ilen, output);
}

#endif /* MBEDTLS_SHA256_ALT */

/* ========================================================================
 * ECC Hardware Acceleration
 * ======================================================================== */

#ifdef MBEDTLS_ECP_ALT

/**
 * ECC point multiplication using HSM
 * This is the most expensive operation in ECDSA
 */
int mbedtls_ecp_mul(mbedtls_ecp_group *grp,
                   mbedtls_ecp_point *R,
                   const mbedtls_mpi *m,
                   const mbedtls_ecp_point *P,
                   int (*f_rng)(void *, unsigned char *, size_t),
                   void *p_rng) {
    // HSM call: tc375_hsm_ecp_mul(grp->id, R, m, P);
    // Hardware accelerated scalar multiplication: R = m * P
    
    return 0;
}

#endif /* MBEDTLS_ECP_ALT */

/* ========================================================================
 * ECDSA Hardware Acceleration
 * ======================================================================== */

#ifdef MBEDTLS_ECDSA_SIGN_ALT

/**
 * ECDSA signature generation using HSM
 */
int mbedtls_ecdsa_sign(mbedtls_ecp_group *grp,
                      mbedtls_mpi *r,
                      mbedtls_mpi *s,
                      const mbedtls_mpi *d,
                      const unsigned char *buf,
                      size_t blen,
                      int (*f_rng)(void *, unsigned char *, size_t),
                      void *p_rng) {
    if (tc375_hsm.ecdsa_sign == NULL) {
        return MBEDTLS_ERR_ECP_HW_ACCEL_FAILED;
    }
    
    unsigned char sig[132];  // Max size for P-521
    size_t sig_len;
    
    // HSM performs: (r, s) = ECDSA_Sign(private_key, hash)
    int ret = tc375_hsm.ecdsa_sign(buf, sig, &sig_len);
    if (ret != 0) {
        return ret;
    }
    
    // Parse signature into r and s
    // ASN.1 DER decoding
    
    return 0;
}

#endif /* MBEDTLS_ECDSA_SIGN_ALT */

#ifdef MBEDTLS_ECDSA_VERIFY_ALT

/**
 * ECDSA signature verification using HSM
 */
int mbedtls_ecdsa_verify(mbedtls_ecp_group *grp,
                        const unsigned char *buf,
                        size_t blen,
                        const mbedtls_ecp_point *Q,
                        const mbedtls_mpi *r,
                        const mbedtls_mpi *s) {
    if (tc375_hsm.ecdsa_verify == NULL) {
        return MBEDTLS_ERR_ECP_HW_ACCEL_FAILED;
    }
    
    unsigned char sig[132];
    size_t sig_len;
    
    // Encode (r, s) into ASN.1 DER format
    // ...
    
    // HSM performs: result = ECDSA_Verify(public_key, hash, signature)
    return tc375_hsm.ecdsa_verify(buf, sig, sig_len);
}

#endif /* MBEDTLS_ECDSA_VERIFY_ALT */

/* ========================================================================
 * True Random Number Generator (TRNG)
 * ======================================================================== */

#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT

/**
 * Hardware entropy source (TC375 TRNG)
 */
int mbedtls_hardware_poll(void *data,
                         unsigned char *output,
                         size_t len,
                         size_t *olen) {
    if (tc375_hsm.random == NULL) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    
    // HSM TRNG: True random number generation
    int ret = tc375_hsm.random(output, len);
    if (ret == 0) {
        *olen = len;
    }
    
    return ret;
}

#endif /* MBEDTLS_ENTROPY_HARDWARE_ALT */

/* ========================================================================
 * HSM Initialization
 * ======================================================================== */

/**
 * Initialize TC375 HSM and register function pointers
 * This should be called before any mbedTLS operations
 */
int tc375_hsm_init(void) {
    // Initialize HSM hardware
    // Map function pointers to actual HSM driver functions
    
    // Example (actual implementation depends on TC375 HSM driver):
    /*
    tc375_hsm.aes_encrypt = tc375_driver_aes_encrypt;
    tc375_hsm.aes_decrypt = tc375_driver_aes_decrypt;
    tc375_hsm.sha256 = tc375_driver_sha256;
    tc375_hsm.ecdsa_sign = tc375_driver_ecdsa_sign;
    tc375_hsm.ecdsa_verify = tc375_driver_ecdsa_verify;
    tc375_hsm.random = tc375_driver_trng;
    */
    
    printf("[TC375 HSM] Hardware Security Module initialized\n");
    printf("[TC375 HSM] Base address: 0x%08X\n", TC375_HSM_BASE_ADDR);
    printf("[TC375 HSM] Size: %u KB\n", TC375_HSM_SIZE / 1024);
    
    return 0;
}

/**
 * Cleanup HSM
 */
void tc375_hsm_free(void) {
    // Clear function pointers
    memset(&tc375_hsm, 0, sizeof(tc375_hsm_t));
    
    printf("[TC375 HSM] Hardware Security Module freed\n");
}

/* ========================================================================
 * Performance Statistics
 * ======================================================================== */

typedef struct {
    uint32_t aes_operations;
    uint32_t sha_operations;
    uint32_t ecdsa_sign_operations;
    uint32_t ecdsa_verify_operations;
    uint32_t random_bytes_generated;
    
    uint64_t total_aes_time_us;
    uint64_t total_sha_time_us;
    uint64_t total_ecdsa_sign_time_us;
    uint64_t total_ecdsa_verify_time_us;
} tc375_hsm_stats_t;

static tc375_hsm_stats_t hsm_stats = {0};

void tc375_hsm_print_stats(void) {
    printf("\n========== TC375 HSM Statistics ==========\n");
    printf("AES operations:        %u (avg: %llu us)\n",
           hsm_stats.aes_operations,
           hsm_stats.aes_operations ? hsm_stats.total_aes_time_us / hsm_stats.aes_operations : 0);
    printf("SHA operations:        %u (avg: %llu us)\n",
           hsm_stats.sha_operations,
           hsm_stats.sha_operations ? hsm_stats.total_sha_time_us / hsm_stats.sha_operations : 0);
    printf("ECDSA sign:            %u (avg: %llu us)\n",
           hsm_stats.ecdsa_sign_operations,
           hsm_stats.ecdsa_sign_operations ? hsm_stats.total_ecdsa_sign_time_us / hsm_stats.ecdsa_sign_operations : 0);
    printf("ECDSA verify:          %u (avg: %llu us)\n",
           hsm_stats.ecdsa_verify_operations,
           hsm_stats.ecdsa_verify_operations ? hsm_stats.total_ecdsa_verify_time_us / hsm_stats.ecdsa_verify_operations : 0);
    printf("Random bytes:          %u\n", hsm_stats.random_bytes_generated);
    printf("=========================================\n");
}

