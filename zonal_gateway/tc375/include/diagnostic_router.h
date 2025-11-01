/**
 * @file diagnostic_router.h
 * @brief Diagnostic Message Router for Zonal Gateway
 * 
 * Routes diagnostic messages between VMG and ECUs
 */

#ifndef DIAGNOSTIC_ROUTER_H
#define DIAGNOSTIC_ROUTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

#define DIAG_ROUTER_MAX_ECUS        32
#define DIAG_ROUTER_TIMEOUT_MS      5000
#define DIAG_ROUTER_MAX_PENDING     16

// ============================================================================
// Types
// ============================================================================

/**
 * @brief ECU routing entry
 */
typedef struct {
    char ecu_id[32];
    uint16_t logical_address;
    bool is_connected;
    uint32_t last_activity_time_ms;
} ECURoutingEntry_t;

/**
 * @brief Pending diagnostic request
 */
typedef struct {
    uint16_t source_address;
    uint16_t target_address;
    uint8_t* uds_data;
    size_t uds_len;
    uint32_t timestamp_ms;
    bool is_active;
} PendingDiagRequest_t;

/**
 * @brief Diagnostic Router
 */
typedef struct {
    // ECU routing table
    ECURoutingEntry_t ecus[DIAG_ROUTER_MAX_ECUS];
    uint32_t ecu_count;
    
    // Pending requests
    PendingDiagRequest_t pending[DIAG_ROUTER_MAX_PENDING];
    
    // Statistics
    uint32_t total_requests;
    uint32_t routed_to_ecu;
    uint32_t routed_to_vmg;
    uint32_t routing_errors;
    
} DiagnosticRouter_t;

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Initialize diagnostic router
 * 
 * @param router Router context
 * @return 0 on success, -1 on error
 */
int diagnostic_router_init(DiagnosticRouter_t* router);

/**
 * @brief Register ECU
 * 
 * @param router Router context
 * @param ecu_id ECU identifier
 * @param logical_address DoIP logical address
 * @return 0 on success, -1 on error
 */
int diagnostic_router_register_ecu(
    DiagnosticRouter_t* router,
    const char* ecu_id,
    uint16_t logical_address
);

/**
 * @brief Route diagnostic message from VMG to ECU
 * 
 * @param router Router context
 * @param source_address Source logical address (VMG/Tester)
 * @param target_address Target logical address (ECU)
 * @param uds_data UDS service + data
 * @param uds_len Length of UDS data
 * @return 0 on success, -1 on error
 */
int diagnostic_router_route_to_ecu(
    DiagnosticRouter_t* router,
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t* uds_data,
    size_t uds_len
);

/**
 * @brief Route diagnostic response from ECU to VMG
 * 
 * @param router Router context
 * @param source_address Source logical address (ECU)
 * @param target_address Target logical address (VMG/Tester)
 * @param uds_data UDS response data
 * @param uds_len Length of UDS data
 * @return 0 on success, -1 on error
 */
int diagnostic_router_route_to_vmg(
    DiagnosticRouter_t* router,
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t* uds_data,
    size_t uds_len
);

/**
 * @brief Broadcast diagnostic message to all ECUs in zone
 * 
 * @param router Router context
 * @param source_address Source logical address
 * @param uds_data UDS service + data
 * @param uds_len Length of UDS data
 * @return Number of ECUs message was sent to
 */
int diagnostic_router_broadcast(
    DiagnosticRouter_t* router,
    uint16_t source_address,
    const uint8_t* uds_data,
    size_t uds_len
);

/**
 * @brief Find ECU by logical address
 * 
 * @param router Router context
 * @param logical_address DoIP logical address
 * @return ECU routing entry or NULL
 */
const ECURoutingEntry_t* diagnostic_router_find_ecu(
    const DiagnosticRouter_t* router,
    uint16_t logical_address
);

/**
 * @brief Update ECU activity timestamp
 * 
 * @param router Router context
 * @param logical_address DoIP logical address
 */
void diagnostic_router_update_activity(
    DiagnosticRouter_t* router,
    uint16_t logical_address
);

/**
 * @brief Check for timed out requests
 * 
 * @param router Router context
 */
void diagnostic_router_check_timeouts(DiagnosticRouter_t* router);

/**
 * @brief Get statistics
 * 
 * @param router Router context
 * @param total_requests Output: total requests
 * @param routed_to_ecu Output: routed to ECU
 * @param routed_to_vmg Output: routed to VMG
 * @param routing_errors Output: routing errors
 */
void diagnostic_router_get_stats(
    const DiagnosticRouter_t* router,
    uint32_t* total_requests,
    uint32_t* routed_to_ecu,
    uint32_t* routed_to_vmg,
    uint32_t* routing_errors
);

#ifdef __cplusplus
}
#endif

#endif /* DIAGNOSTIC_ROUTER_H */

