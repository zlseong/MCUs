/**
 * @file pqc_simulator.c
 * @brief End-to-End PQC Parameter Simulation Tool
 * 
 * Tests all combinations of ML-KEM (512/768/1024) with ML-DSA/ECDSA
 * Simulates: Server -> VMG -> Zonal Gateway -> ECU
 */

#include "../common/protocol/pqc_params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Simulation results
typedef struct {
    uint8_t config_id;
    double handshake_time_ms;
    double data_transfer_time_ms;
    size_t total_bytes_transferred;
    bool success;
    char error_msg[256];
} SimulationResult;

// Simulate TLS handshake
static double simulate_handshake(const PQC_Config* config) {
    // Rough estimates based on benchmarks
    double base_time = 10.0; // ms
    
    // KEM overhead
    switch (config->kem) {
        case PQC_KEM_MLKEM512:  base_time += 2.0; break;
        case PQC_KEM_MLKEM768:  base_time += 3.0; break;
        case PQC_KEM_MLKEM1024: base_time += 4.0; break;
    }
    
    // Signature overhead
    if (config->sig_type == 1) {
        // ML-DSA
        switch (config->sig.mldsa) {
            case PQC_SIG_MLDSA44: base_time += 5.0; break;
            case PQC_SIG_MLDSA65: base_time += 7.0; break;
            case PQC_SIG_MLDSA87: base_time += 9.0; break;
        }
    } else {
        // ECDSA (faster)
        base_time += 1.0;
    }
    
    return base_time;
}

// Simulate data transfer
static double simulate_data_transfer(const PQC_Config* config, size_t data_size) {
    // Assume 100 Mbps network
    double bandwidth_mbps = 100.0;
    double transfer_time = (data_size * 8.0) / (bandwidth_mbps * 1000000.0) * 1000.0;
    
    // Add encryption overhead (negligible with AES)
    transfer_time += 1.0;
    
    return transfer_time;
}

// Run single configuration test
static SimulationResult run_simulation(uint8_t config_id) {
    SimulationResult result = {0};
    result.config_id = config_id;
    result.success = true;
    
    const PQC_Config* config = pqc_get_config(config_id);
    if (!config) {
        result.success = false;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid config ID");
        return result;
    }
    
    printf("\n[TEST %d] %s + %s (%d-bit)\n", 
           config_id, config->kem_name, config->sig_name, config->security_bits);
    printf("-----------------------------------------------------\n");
    
    // Phase 1: TLS Handshake
    printf("  [1] TLS Handshake...\n");
    result.handshake_time_ms = simulate_handshake(config);
    printf("      Time: %.2f ms\n", result.handshake_time_ms);
    printf("      KEM Public Key: %d bytes\n", config->kem_public_key_size);
    printf("      KEM Ciphertext: %d bytes\n", config->kem_ciphertext_size);
    printf("      SIG Public Key: %d bytes\n", config->sig_public_key_size);
    printf("      SIG Signature:  %d bytes\n", config->sig_signature_size);
    
    // Phase 2: Data Transfer (10 MB OTA package)
    printf("  [2] Data Transfer (10 MB OTA package)...\n");
    size_t data_size = 10 * 1024 * 1024;
    result.data_transfer_time_ms = simulate_data_transfer(config, data_size);
    result.total_bytes_transferred = data_size;
    printf("      Time: %.2f ms\n", result.data_transfer_time_ms);
    
    double total_time = result.handshake_time_ms + result.data_transfer_time_ms;
    printf("  [TOTAL] %.2f ms\n", total_time);
    
    // Calculate overhead
    size_t handshake_overhead = config->kem_public_key_size + 
                                 config->kem_ciphertext_size +
                                 config->sig_public_key_size +
                                 config->sig_signature_size;
    printf("  [OVERHEAD] %zu bytes (handshake)\n", handshake_overhead);
    
    return result;
}

// Print comparison table
static void print_comparison(SimulationResult* results, int count) {
    printf("\n");
    printf("=============================================================================\n");
    printf("                     End-to-End PQC Performance Comparison                   \n");
    printf("=============================================================================\n");
    printf("ID | Configuration              | Handshake | Transfer | Total   | Overhead \n");
    printf("---|----------------------------|-----------|----------|---------|----------\n");
    
    for (int i = 0; i < count; i++) {
        if (!results[i].success) continue;
        
        const PQC_Config* cfg = pqc_get_config(results[i].config_id);
        if (!cfg) continue;
        
        char config_name[32];
        snprintf(config_name, sizeof(config_name), "%s+%s", 
                 cfg->kem_name, cfg->sig_name);
        
        size_t overhead = cfg->kem_public_key_size + cfg->kem_ciphertext_size +
                          cfg->sig_public_key_size + cfg->sig_signature_size;
        
        double total = results[i].handshake_time_ms + results[i].data_transfer_time_ms;
        
        printf("%2d | %-26s | %7.2fms | %6.2fms | %7.2fms | %5zu B\n",
               results[i].config_id, config_name,
               results[i].handshake_time_ms,
               results[i].data_transfer_time_ms,
               total,
               overhead);
    }
    
    printf("=============================================================================\n");
}

// Print recommendations
static void print_recommendations(SimulationResult* results, int count) {
    printf("\n[RECOMMENDATIONS]\n");
    printf("-----------------------------------------------------\n");
    
    // Find fastest
    double min_time = 1e9;
    int fastest_id = -1;
    for (int i = 0; i < count; i++) {
        if (!results[i].success) continue;
        double total = results[i].handshake_time_ms + results[i].data_transfer_time_ms;
        if (total < min_time) {
            min_time = total;
            fastest_id = i;
        }
    }
    
    // Print categories
    printf("  [FASTEST]        : #%d - %s\n", 
           fastest_id, pqc_get_config_name(fastest_id));
    printf("  [RECOMMENDED]    : #%d - %s (balanced)\n", 
           PQC_CONFIG_RECOMMENDED, pqc_get_config_name(PQC_CONFIG_RECOMMENDED));
    printf("  [LIGHTWEIGHT]    : #%d - %s (embedded)\n", 
           PQC_CONFIG_LIGHTWEIGHT, pqc_get_config_name(PQC_CONFIG_LIGHTWEIGHT));
    printf("  [HIGH SECURITY]  : #%d - %s (critical)\n", 
           PQC_CONFIG_HIGH_SECURITY, pqc_get_config_name(PQC_CONFIG_HIGH_SECURITY));
    printf("\n");
}

int main(int argc, char** argv) {
    printf("=============================================================================\n");
    printf("             PQC Parameter End-to-End Simulation Tool\n");
    printf("=============================================================================\n");
    printf("Testing: Server -> VMG -> Zonal Gateway -> ECU\n");
    printf("Scenario: 10 MB OTA firmware download over PQC-TLS\n");
    printf("\n");
    
    SimulationResult results[PQC_CONFIG_COUNT];
    
    if (argc > 1) {
        // Test specific configuration
        int config_id = atoi(argv[1]);
        if (config_id < 0 || config_id >= PQC_CONFIG_COUNT) {
            fprintf(stderr, "Error: Invalid config ID %d (valid: 0-%d)\n", 
                    config_id, PQC_CONFIG_COUNT - 1);
            return 1;
        }
        
        results[0] = run_simulation(config_id);
        print_comparison(results, 1);
    } else {
        // Test all configurations
        printf("[MODE] Full test - all %d configurations\n", PQC_CONFIG_COUNT);
        
        for (int i = 0; i < PQC_CONFIG_COUNT; i++) {
            results[i] = run_simulation(i);
        }
        
        print_comparison(results, PQC_CONFIG_COUNT);
        print_recommendations(results, PQC_CONFIG_COUNT);
    }
    
    printf("\n[USAGE]\n");
    printf("  Full test:       ./pqc_simulator\n");
    printf("  Single test:     ./pqc_simulator <config_id>\n");
    printf("  Config IDs:      0-%d\n", PQC_CONFIG_COUNT - 1);
    printf("\n");
    
    return 0;
}

