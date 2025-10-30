/**
 * JSON Output Implementation
 */

#include "json_output.h"
#include <stdio.h>

void json_output_metrics(const TLS_Metrics* m, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"handshake_ms\": %.2f,\n", m->t_handshake_total_ms);
    fprintf(f, "  \"bytes_tx\": %lu,\n", m->bytes_tx_handshake);
    fprintf(f, "  \"bytes_rx\": %lu,\n", m->bytes_rx_handshake);
    fprintf(f, "  \"kem_keyshare_bytes\": %u,\n", m->kem_keyshare_len);
    fprintf(f, "  \"signature_bytes\": %u,\n", m->sig_len);
    fprintf(f, "  \"cert_chain_bytes\": %u,\n", m->cert_chain_size);
    fprintf(f, "  \"success\": %s\n", m->success ? "true" : "false");
    fprintf(f, "}\n");
    
    fclose(f);
}

