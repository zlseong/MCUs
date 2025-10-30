/**
 * Metrics for PQC TLS Performance
 * Based on Benchmark project
 */

#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>
#include <time.h>

typedef struct {
    // Timing
    double t_handshake_total_ms;
    double t_clienthello_to_serverhello_ms;
    double t_cert_verify_ms;
    double t_finished_flight_ms;
    
    // Traffic
    uint64_t bytes_tx_handshake;
    uint64_t bytes_rx_handshake;
    uint32_t records_count;
    
    // Crypto
    uint32_t kem_keyshare_len;
    double kem_encap_ms_client;
    double kem_decap_ms_server;
    uint32_t sig_len;
    double sign_ms;
    double verify_ms;
    uint32_t cert_chain_size;
    
    // Result
    int success;
    const char* error_msg;
    
} TLS_Metrics;

void metrics_init(TLS_Metrics* m);
void metrics_print(const TLS_Metrics* m);

#endif

