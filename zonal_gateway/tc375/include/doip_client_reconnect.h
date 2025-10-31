/**
 * @file doip_client_reconnect.h
 * @brief DoIP Client with Auto-Reconnection for TC375
 * 
 * Provides automatic reconnection with exponential backoff
 * for reliable in-vehicle network communication.
 */

#ifndef DOIP_CLIENT_RECONNECT_H
#define DOIP_CLIENT_RECONNECT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

#define DOIP_RECONNECT_INTERVAL_MS    5000   // Initial retry interval
#define DOIP_MAX_RECONNECT_ATTEMPTS   0      // 0 = infinite retries
#define DOIP_INITIAL_BACKOFF_MS       1000   // 1 second
#define DOIP_MAX_BACKOFF_MS           30000  // 30 seconds max
#define DOIP_KEEPALIVE_INTERVAL_MS    30000  // 30 seconds
#define DOIP_KEEPALIVE_TIMEOUT_MS     5000   // 5 seconds

// ============================================================================
// Types
// ============================================================================

/**
 * @brief Connection state
 */
typedef enum {
    DOIP_STATE_DISCONNECTED = 0,
    DOIP_STATE_CONNECTING,
    DOIP_STATE_CONNECTED,
    DOIP_STATE_RECONNECTING
} DoIPConnectionState_t;

/**
 * @brief DoIP Client with Reconnection Support
 */
typedef struct {
    // Connection state
    DoIPConnectionState_t state;
    bool is_connected;
    
    // Server info
    char server_host[64];
    uint16_t server_port;
    
    // Certificates (for mbedTLS)
    char cert_file[128];
    char key_file[128];
    char ca_file[128];
    
    // Reconnection tracking
    uint32_t reconnect_count;
    uint32_t backoff_ms;
    uint32_t last_attempt_time_ms;
    uint32_t last_keepalive_time_ms;
    
    // Statistics
    uint32_t total_reconnects;
    uint32_t total_keepalive_failures;
    
    // Internal (platform-specific)
    void* client_ctx;  // mbedtls_doip_client* or similar
    
} DoIPClientReconnect_t;

/**
 * @brief Connection event callback
 */
typedef void (*DoIPConnectionCallback_t)(
    DoIPClientReconnect_t* client,
    DoIPConnectionState_t old_state,
    DoIPConnectionState_t new_state
);

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Initialize reconnecting DoIP client
 * 
 * @param client Client context
 * @param server_host Server hostname or IP
 * @param server_port Server port (typically 13400)
 * @param cert_file Client certificate file path
 * @param key_file Client private key file path
 * @param ca_file CA certificate file path
 * @return 0 on success, -1 on error
 */
int doip_client_reconnect_init(
    DoIPClientReconnect_t* client,
    const char* server_host,
    uint16_t server_port,
    const char* cert_file,
    const char* key_file,
    const char* ca_file
);

/**
 * @brief Start connection (non-blocking)
 * 
 * Initiates connection attempt. Call doip_client_reconnect_process()
 * periodically to complete the connection.
 * 
 * @param client Client context
 * @return 0 on success, -1 on error
 */
int doip_client_reconnect_start(DoIPClientReconnect_t* client);

/**
 * @brief Process reconnection logic (call periodically)
 * 
 * This function should be called periodically (e.g., every 100ms)
 * from the main loop or FreeRTOS task.
 * 
 * @param client Client context
 * @return 0 if connected, -1 if disconnected, -2 if connecting
 */
int doip_client_reconnect_process(DoIPClientReconnect_t* client);

/**
 * @brief Check if client is connected
 * 
 * @param client Client context
 * @return true if connected, false otherwise
 */
bool doip_client_reconnect_is_connected(const DoIPClientReconnect_t* client);

/**
 * @brief Send data (only if connected)
 * 
 * @param client Client context
 * @param data Data to send
 * @param len Data length
 * @return Number of bytes sent, or -1 on error
 */
int doip_client_reconnect_send(
    DoIPClientReconnect_t* client,
    const uint8_t* data,
    size_t len
);

/**
 * @brief Receive data (non-blocking)
 * 
 * @param client Client context
 * @param buf Buffer to receive data
 * @param cap Buffer capacity
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes received, 0 on timeout, -1 on error
 */
int doip_client_reconnect_recv(
    DoIPClientReconnect_t* client,
    uint8_t* buf,
    size_t cap,
    uint32_t timeout_ms
);

/**
 * @brief Force disconnect and reconnect
 * 
 * @param client Client context
 * @return 0 on success, -1 on error
 */
int doip_client_reconnect_reset(DoIPClientReconnect_t* client);

/**
 * @brief Get connection statistics
 * 
 * @param client Client context
 * @param total_reconnects Output: total reconnection count
 * @param current_backoff_ms Output: current backoff time
 */
void doip_client_reconnect_get_stats(
    const DoIPClientReconnect_t* client,
    uint32_t* total_reconnects,
    uint32_t* current_backoff_ms
);

/**
 * @brief Cleanup and free resources
 * 
 * @param client Client context
 */
void doip_client_reconnect_cleanup(DoIPClientReconnect_t* client);

// ============================================================================
// FreeRTOS Task Helper
// ============================================================================

/**
 * @brief FreeRTOS task for DoIP client with auto-reconnect
 * 
 * Example usage:
 * ```c
 * DoIPClientReconnect_t client;
 * doip_client_reconnect_init(&client, "192.168.1.1", 13400, ...);
 * xTaskCreate(doip_client_reconnect_task, "DoIP", 4096, &client, 5, NULL);
 * ```
 * 
 * @param params Pointer to DoIPClientReconnect_t
 */
void doip_client_reconnect_task(void* params);

#ifdef __cplusplus
}
#endif

#endif /* DOIP_CLIENT_RECONNECT_H */

