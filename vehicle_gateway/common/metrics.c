/**
 * Metrics Implementation
 */

#include "metrics.h"
#include <stdio.h>
#include <string.h>

void metrics_init(TLS_Metrics* m) {
    memset(m, 0, sizeof(TLS_Metrics));
}

void metrics_print(const TLS_Metrics* m) {
    printf("\n========== TLS Metrics ==========\n");
    printf("Handshake:      %.2f ms\n", m->t_handshake_total_ms);
    printf("TX:             %lu bytes\n", m->bytes_tx_handshake);
    printf("RX:             %lu bytes\n", m->bytes_rx_handshake);
    printf("KEM keyshare:   %u bytes\n", m->kem_keyshare_len);
    printf("Signature:      %u bytes\n", m->sig_len);
    printf("Certificate:    %u bytes\n", m->cert_chain_size);
    printf("Success:        %s\n", m->success ? "YES" : "NO");
    if (!m->success && m->error_msg) {
        printf("Error:          %s\n", m->error_msg);
    }
    printf("=================================\n");
}

