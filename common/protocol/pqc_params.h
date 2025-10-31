/**
 * @file pqc_params.h
 * @brief PQC Parameter Definitions - Cross-platform compatible
 * 
 * Supports all NIST-standardized ML-KEM and ML-DSA/ECDSA parameters
 * for End-to-End simulation across VMG, Zonal Gateway, and ECU
 */

#ifndef PQC_PARAMS_H
#define PQC_PARAMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// KEM (Key Encapsulation Mechanism)
// ============================================================================

typedef enum {
    PQC_KEM_X25519    = 0,    // Classical ECC (baseline)
    PQC_KEM_MLKEM512  = 1,    // 128-bit security
    PQC_KEM_MLKEM768  = 2,    // 192-bit security [RECOMMENDED]
    PQC_KEM_MLKEM1024 = 3     // 256-bit security
} PQC_KEM_Type;

// X25519 Key Sizes (bytes)
#define X25519_PUBLIC_KEY_SIZE      32
#define X25519_SECRET_KEY_SIZE      32
#define X25519_SHARED_SECRET_SIZE   32

// ML-KEM Key Sizes (bytes)
#define MLKEM512_PUBLIC_KEY_SIZE    800
#define MLKEM512_SECRET_KEY_SIZE    1632
#define MLKEM512_CIPHERTEXT_SIZE    768
#define MLKEM512_SHARED_SECRET_SIZE 32

#define MLKEM768_PUBLIC_KEY_SIZE    1184
#define MLKEM768_SECRET_KEY_SIZE    2400
#define MLKEM768_CIPHERTEXT_SIZE    1088
#define MLKEM768_SHARED_SECRET_SIZE 32

#define MLKEM1024_PUBLIC_KEY_SIZE    1568
#define MLKEM1024_SECRET_KEY_SIZE    3168
#define MLKEM1024_CIPHERTEXT_SIZE    1568
#define MLKEM1024_SHARED_SECRET_SIZE 32

// ============================================================================
// ML-DSA (FIPS 204) - Digital Signature Algorithm
// ============================================================================

typedef enum {
    PQC_SIG_MLDSA44 = 0,      // 128-bit security (Dilithium2)
    PQC_SIG_MLDSA65 = 1,      // 192-bit security (Dilithium3) [RECOMMENDED]
    PQC_SIG_MLDSA87 = 2       // 256-bit security (Dilithium5)
} PQC_SIG_Type;

// ML-DSA Key/Signature Sizes (bytes)
#define MLDSA44_PUBLIC_KEY_SIZE     1312
#define MLDSA44_SECRET_KEY_SIZE     2560
#define MLDSA44_SIGNATURE_SIZE      2420

#define MLDSA65_PUBLIC_KEY_SIZE     1952
#define MLDSA65_SECRET_KEY_SIZE     4032
#define MLDSA65_SIGNATURE_SIZE      3309

#define MLDSA87_PUBLIC_KEY_SIZE     2592
#define MLDSA87_SECRET_KEY_SIZE     4896
#define MLDSA87_SIGNATURE_SIZE      4627

// ============================================================================
// ECDSA - Classical Digital Signature
// ============================================================================

typedef enum {
    PQC_ECDSA_P256 = 0,       // secp256r1 (128-bit security)
    PQC_ECDSA_P384 = 1,       // secp384r1 (192-bit security)
    PQC_ECDSA_P521 = 2        // secp521r1 (256-bit security)
} PQC_ECDSA_Type;

// ECDSA Key/Signature Sizes (bytes)
#define ECDSA_P256_PUBLIC_KEY_SIZE  65
#define ECDSA_P256_SECRET_KEY_SIZE  32
#define ECDSA_P256_SIGNATURE_SIZE   64

#define ECDSA_P384_PUBLIC_KEY_SIZE  97
#define ECDSA_P384_SECRET_KEY_SIZE  48
#define ECDSA_P384_SIGNATURE_SIZE   96

#define ECDSA_P521_PUBLIC_KEY_SIZE  133
#define ECDSA_P521_SECRET_KEY_SIZE  66
#define ECDSA_P521_SIGNATURE_SIZE   132

// ============================================================================
// Algorithm Configuration
// ============================================================================

typedef struct {
    PQC_KEM_Type kem;
    uint8_t sig_type;         // 0=ECDSA, 1=ML-DSA
    union {
        PQC_SIG_Type mldsa;
        PQC_ECDSA_Type ecdsa;
    } sig;
    
    // String names (for OpenSSL)
    const char* kem_name;
    const char* sig_name;
    const char* openssl_groups;
    const char* openssl_sigalgs;
    
    // Sizes (for buffer allocation)
    uint16_t kem_public_key_size;
    uint16_t kem_ciphertext_size;
    uint16_t sig_public_key_size;
    uint16_t sig_signature_size;
    
    // Security level
    uint8_t security_bits;    // 128, 192, or 256
} PQC_Config;

// ============================================================================
// Predefined Configurations
// ============================================================================

// Configuration IDs (13 combinations)
#define PQC_CONFIG_X25519_ECDSA_P256      0   // Classical baseline

#define PQC_CONFIG_MLKEM512_ECDSA_P256    1   // Hybrid 128-bit
#define PQC_CONFIG_MLKEM768_ECDSA_P256    2   // Hybrid 192-bit [DEFAULT]
#define PQC_CONFIG_MLKEM1024_ECDSA_P256   3   // Hybrid 256-bit

#define PQC_CONFIG_MLKEM512_MLDSA44       4   // Pure PQC 128-bit
#define PQC_CONFIG_MLKEM512_MLDSA65       5
#define PQC_CONFIG_MLKEM512_MLDSA87       6

#define PQC_CONFIG_MLKEM768_MLDSA44       7   // Pure PQC 192-bit
#define PQC_CONFIG_MLKEM768_MLDSA65       8   // Pure PQC 192-bit
#define PQC_CONFIG_MLKEM768_MLDSA87       9

#define PQC_CONFIG_MLKEM1024_MLDSA44      10  // Pure PQC 256-bit
#define PQC_CONFIG_MLKEM1024_MLDSA65      11
#define PQC_CONFIG_MLKEM1024_MLDSA87      12

#define PQC_CONFIG_COUNT                  13

// Recommended configurations
#define PQC_CONFIG_RECOMMENDED       PQC_CONFIG_MLKEM768_ECDSA_P256  // Hybrid [2]
#define PQC_CONFIG_PURE_PQC          PQC_CONFIG_MLKEM768_MLDSA65     // Pure PQC [8]
#define PQC_CONFIG_LIGHTWEIGHT       PQC_CONFIG_MLKEM512_ECDSA_P256  // Fast [1]
#define PQC_CONFIG_HIGH_SECURITY     PQC_CONFIG_MLKEM1024_MLDSA87    // Max security [12]

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get predefined PQC configuration by ID
 * @param config_id Configuration ID (0-11)
 * @return Pointer to PQC_Config, or NULL if invalid
 */
const PQC_Config* pqc_get_config(uint8_t config_id);

/**
 * @brief Get configuration by KEM and signature types
 * @param kem KEM type
 * @param sig_mldsa ML-DSA type (or -1 if using ECDSA)
 * @param sig_ecdsa ECDSA type (or -1 if using ML-DSA)
 * @return Pointer to PQC_Config, or NULL if not found
 */
const PQC_Config* pqc_find_config(PQC_KEM_Type kem, int8_t sig_mldsa, int8_t sig_ecdsa);

/**
 * @brief Print configuration details
 * @param config PQC configuration
 */
void pqc_print_config(const PQC_Config* config);

/**
 * @brief Get configuration name
 * @param config_id Configuration ID
 * @return Configuration name string
 */
const char* pqc_get_config_name(uint8_t config_id);

#ifdef __cplusplus
}
#endif

#endif // PQC_PARAMS_H

