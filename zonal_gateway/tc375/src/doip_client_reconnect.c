/**
 * @file doip_client_reconnect.c
 * @brief DoIP Client with Auto-Reconnection Implementation
 */

#include "doip_client_reconnect.h"
#include "doip_client_mbedtls.h"
#include <string.h>
#include <stdio.h>

// FreeRTOS headers (if using RTOS)
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#define TASK_DELAY_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))
#define GET_TIME_MS() (xTaskGetTickCount() * portTICK_PERIOD_MS)
#else
#include <unistd.h>
#define TASK_DELAY_MS(ms) usleep((ms) * 1000)
#define GET_TIME_MS() 0  // TODO: Implement for bare metal
#endif

// ============================================================================
// Private Functions
// ============================================================================

static void set_state(DoIPClientReconnect_t* client, DoIPConnectionState_t new_state) {
    if (client->state != new_state) {
        DoIPConnectionState_t old_state = client->state;
        client->state = new_state;
        
        const char* state_names[] = {
            "DISCONNECTED", "CONNECTING", "CONNECTED", "RECONNECTING"
        };
        
        printf("[DoIP] State: %s -> %s\n", 
               state_names[old_state], state_names[new_state]);
    }
}

static int attempt_connection(DoIPClientReconnect_t* client) {
    printf("[DoIP] Connecting to %s:%u (attempt %u)...\n",
           client->server_host, client->server_port, client->reconnect_count + 1);
    
    // TODO: Replace with actual mbedTLS DoIP client connection
    // For now, this is a placeholder
    
    /*
    int ret = mbedtls_doip_client_connect(
        (mbedtls_doip_client*)client->client_ctx,
        client->server_host,
        client->server_port,
        client->cert_file,
        client->key_file,
        client->ca_file
    );
    
    return ret;
    */
    
    // Placeholder: simulate connection
    return -1;  // Connection failed (for now)
}

static void close_connection(DoIPClientReconnect_t* client) {
    if (client->client_ctx) {
        // TODO: Close mbedTLS connection
        // mbedtls_doip_client_close((mbedtls_doip_client*)client->client_ctx);
        client->client_ctx = NULL;
    }
    
    client->is_connected = false;
}

// ============================================================================
// Public API Implementation
// ============================================================================

int doip_client_reconnect_init(
    DoIPClientReconnect_t* client,
    const char* server_host,
    uint16_t server_port,
    const char* cert_file,
    const char* key_file,
    const char* ca_file
) {
    if (!client || !server_host) {
        return -1;
    }
    
    memset(client, 0, sizeof(DoIPClientReconnect_t));
    
    // Copy server info
    strncpy(client->server_host, server_host, sizeof(client->server_host) - 1);
    client->server_port = server_port;
    
    // Copy certificate paths
    if (cert_file) {
        strncpy(client->cert_file, cert_file, sizeof(client->cert_file) - 1);
    }
    if (key_file) {
        strncpy(client->key_file, key_file, sizeof(client->key_file) - 1);
    }
    if (ca_file) {
        strncpy(client->ca_file, ca_file, sizeof(client->ca_file) - 1);
    }
    
    // Initialize reconnection parameters
    client->state = DOIP_STATE_DISCONNECTED;
    client->is_connected = false;
    client->backoff_ms = DOIP_INITIAL_BACKOFF_MS;
    client->reconnect_count = 0;
    client->total_reconnects = 0;
    client->last_attempt_time_ms = 0;
    client->last_keepalive_time_ms = 0;
    
    printf("[DoIP] Client initialized for %s:%u\n", server_host, server_port);
    
    return 0;
}

int doip_client_reconnect_start(DoIPClientReconnect_t* client) {
    if (!client) {
        return -1;
    }
    
    if (client->state == DOIP_STATE_CONNECTED) {
        return 0;  // Already connected
    }
    
    set_state(client, DOIP_STATE_CONNECTING);
    client->last_attempt_time_ms = GET_TIME_MS();
    
    return 0;
}

int doip_client_reconnect_process(DoIPClientReconnect_t* client) {
    if (!client) {
        return -1;
    }
    
    uint32_t current_time = GET_TIME_MS();
    
    switch (client->state) {
        case DOIP_STATE_DISCONNECTED:
            // Not started yet
            return -1;
        
        case DOIP_STATE_CONNECTING:
        case DOIP_STATE_RECONNECTING:
            // Check if enough time has passed since last attempt
            if (current_time - client->last_attempt_time_ms < client->backoff_ms) {
                return -2;  // Still waiting
            }
            
            // Attempt connection
            client->last_attempt_time_ms = current_time;
            
            if (attempt_connection(client) == 0) {
                // Connection successful!
                printf("[DoIP] Connected successfully!\n");
                set_state(client, DOIP_STATE_CONNECTED);
                client->is_connected = true;
                client->reconnect_count = 0;
                client->backoff_ms = DOIP_INITIAL_BACKOFF_MS;
                client->last_keepalive_time_ms = current_time;
                return 0;
            }
            
            // Connection failed
            printf("[DoIP] Connection failed, will retry in %u ms\n", client->backoff_ms);
            
            client->reconnect_count++;
            client->total_reconnects++;
            
            // Exponential backoff
            client->backoff_ms *= 2;
            if (client->backoff_ms > DOIP_MAX_BACKOFF_MS) {
                client->backoff_ms = DOIP_MAX_BACKOFF_MS;
            }
            
            set_state(client, DOIP_STATE_RECONNECTING);
            return -2;  // Still connecting
        
        case DOIP_STATE_CONNECTED:
            // Check keepalive
            if (current_time - client->last_keepalive_time_ms > DOIP_KEEPALIVE_INTERVAL_MS) {
                // Send keepalive (Alive Check)
                // TODO: Implement actual keepalive
                /*
                int ret = mbedtls_doip_client_send_alive_check(
                    (mbedtls_doip_client*)client->client_ctx
                );
                
                if (ret != 0) {
                    printf("[DoIP] Keepalive failed, connection lost\n");
                    close_connection(client);
                    set_state(client, DOIP_STATE_RECONNECTING);
                    client->backoff_ms = DOIP_INITIAL_BACKOFF_MS;
                    client->total_keepalive_failures++;
                    return -1;
                }
                */
                
                client->last_keepalive_time_ms = current_time;
            }
            
            return 0;  // Connected
    }
    
    return -1;
}

bool doip_client_reconnect_is_connected(const DoIPClientReconnect_t* client) {
    return client && client->is_connected && client->state == DOIP_STATE_CONNECTED;
}

int doip_client_reconnect_send(
    DoIPClientReconnect_t* client,
    const uint8_t* data,
    size_t len
) {
    if (!client || !data || !client->is_connected) {
        return -1;
    }
    
    // TODO: Implement actual send
    /*
    int ret = mbedtls_doip_client_send(
        (mbedtls_doip_client*)client->client_ctx,
        data,
        len
    );
    
    if (ret < 0) {
        // Send failed, connection might be lost
        printf("[DoIP] Send failed, connection lost\n");
        close_connection(client);
        set_state(client, DOIP_STATE_RECONNECTING);
        client->backoff_ms = DOIP_INITIAL_BACKOFF_MS;
    }
    
    return ret;
    */
    
    return -1;  // Placeholder
}

int doip_client_reconnect_recv(
    DoIPClientReconnect_t* client,
    uint8_t* buf,
    size_t cap,
    uint32_t timeout_ms
) {
    if (!client || !buf || !client->is_connected) {
        return -1;
    }
    
    // TODO: Implement actual receive
    /*
    int ret = mbedtls_doip_client_recv(
        (mbedtls_doip_client*)client->client_ctx,
        buf,
        cap,
        timeout_ms
    );
    
    if (ret < 0) {
        // Receive failed, connection might be lost
        printf("[DoIP] Receive failed, connection lost\n");
        close_connection(client);
        set_state(client, DOIP_STATE_RECONNECTING);
        client->backoff_ms = DOIP_INITIAL_BACKOFF_MS;
    }
    
    return ret;
    */
    
    return 0;  // Placeholder (timeout)
}

int doip_client_reconnect_reset(DoIPClientReconnect_t* client) {
    if (!client) {
        return -1;
    }
    
    printf("[DoIP] Forcing reconnection...\n");
    
    close_connection(client);
    set_state(client, DOIP_STATE_RECONNECTING);
    client->backoff_ms = DOIP_INITIAL_BACKOFF_MS;
    client->last_attempt_time_ms = 0;  // Immediate retry
    
    return 0;
}

void doip_client_reconnect_get_stats(
    const DoIPClientReconnect_t* client,
    uint32_t* total_reconnects,
    uint32_t* current_backoff_ms
) {
    if (!client) {
        return;
    }
    
    if (total_reconnects) {
        *total_reconnects = client->total_reconnects;
    }
    
    if (current_backoff_ms) {
        *current_backoff_ms = client->backoff_ms;
    }
}

void doip_client_reconnect_cleanup(DoIPClientReconnect_t* client) {
    if (!client) {
        return;
    }
    
    close_connection(client);
    
    printf("[DoIP] Client cleaned up\n");
}

// ============================================================================
// FreeRTOS Task
// ============================================================================

void doip_client_reconnect_task(void* params) {
    DoIPClientReconnect_t* client = (DoIPClientReconnect_t*)params;
    
    if (!client) {
        printf("[DoIP] Error: Invalid client parameter\n");
        return;
    }
    
    printf("[DoIP] Client task started\n");
    
    // Start connection
    doip_client_reconnect_start(client);
    
    while (1) {
        // Process reconnection logic
        int status = doip_client_reconnect_process(client);
        
        // If connected, try to receive messages
        if (status == 0) {
            uint8_t buf[4096];
            int len = doip_client_reconnect_recv(client, buf, sizeof(buf), 100);
            
            if (len > 0) {
                // Process received message
                printf("[DoIP] Received %d bytes\n", len);
                // TODO: Call message handler
                // handle_doip_message(buf, len);
            }
        }
        
        // Small delay to avoid busy loop
        TASK_DELAY_MS(100);
    }
}

