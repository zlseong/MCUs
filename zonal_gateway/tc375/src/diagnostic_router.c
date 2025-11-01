/**
 * @file diagnostic_router.c
 * @brief Diagnostic Message Router Implementation
 */

#include "diagnostic_router.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Helper Functions
// ============================================================================

static uint32_t get_current_time_ms(void) {
    // TODO: Implement with FreeRTOS xTaskGetTickCount()
    return 0;
}

static void debug_print(const char* format, ...) {
    // TODO: Implement with UART or debug output
    (void)format;
}

// ============================================================================
// API Implementation
// ============================================================================

int diagnostic_router_init(DiagnosticRouter_t* router) {
    if (!router) {
        return -1;
    }
    
    memset(router, 0, sizeof(DiagnosticRouter_t));
    
    debug_print("[DiagRouter] Initialized\n");
    
    return 0;
}

int diagnostic_router_register_ecu(
    DiagnosticRouter_t* router,
    const char* ecu_id,
    uint16_t logical_address
) {
    if (!router || !ecu_id || router->ecu_count >= DIAG_ROUTER_MAX_ECUS) {
        return -1;
    }
    
    ECURoutingEntry_t* entry = &router->ecus[router->ecu_count];
    
    strncpy(entry->ecu_id, ecu_id, sizeof(entry->ecu_id) - 1);
    entry->ecu_id[sizeof(entry->ecu_id) - 1] = '\0';
    
    entry->logical_address = logical_address;
    entry->is_connected = false;
    entry->last_activity_time_ms = 0;
    
    router->ecu_count++;
    
    debug_print("[DiagRouter] Registered ECU: %s (0x%04X)\n", ecu_id, logical_address);
    
    return 0;
}

int diagnostic_router_route_to_ecu(
    DiagnosticRouter_t* router,
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t* uds_data,
    size_t uds_len
) {
    if (!router || !uds_data || uds_len == 0) {
        return -1;
    }
    
    router->total_requests++;
    
    // Find target ECU
    const ECURoutingEntry_t* ecu = diagnostic_router_find_ecu(router, target_address);
    if (!ecu) {
        debug_print("[DiagRouter] ECU not found: 0x%04X\n", target_address);
        router->routing_errors++;
        return -1;
    }
    
    debug_print("[DiagRouter] Routing to ECU: %s (0x%04X -> 0x%04X)\n",
                ecu->ecu_id, source_address, target_address);
    
    // TODO: Send via DoIP client to ECU
    // doip_client_send_diagnostic_message(source_address, target_address, uds_data, uds_len);
    
    router->routed_to_ecu++;
    
    return 0;
}

int diagnostic_router_route_to_vmg(
    DiagnosticRouter_t* router,
    uint16_t source_address,
    uint16_t target_address,
    const uint8_t* uds_data,
    size_t uds_len
) {
    if (!router || !uds_data || uds_len == 0) {
        return -1;
    }
    
    debug_print("[DiagRouter] Routing to VMG: 0x%04X -> 0x%04X\n",
                source_address, target_address);
    
    // Update ECU activity
    diagnostic_router_update_activity(router, source_address);
    
    // TODO: Send via DoIP client to VMG
    // doip_client_send_diagnostic_message(source_address, target_address, uds_data, uds_len);
    
    router->routed_to_vmg++;
    
    return 0;
}

int diagnostic_router_broadcast(
    DiagnosticRouter_t* router,
    uint16_t source_address,
    const uint8_t* uds_data,
    size_t uds_len
) {
    if (!router || !uds_data || uds_len == 0) {
        return -1;
    }
    
    debug_print("[DiagRouter] Broadcasting to %u ECUs\n", router->ecu_count);
    
    int sent_count = 0;
    
    for (uint32_t i = 0; i < router->ecu_count; i++) {
        const ECURoutingEntry_t* ecu = &router->ecus[i];
        
        if (ecu->is_connected) {
            debug_print("[DiagRouter]   -> %s (0x%04X)\n", 
                        ecu->ecu_id, ecu->logical_address);
            
            // TODO: Send to ECU
            // doip_client_send_diagnostic_message(source_address, ecu->logical_address, uds_data, uds_len);
            
            sent_count++;
        }
    }
    
    router->routed_to_ecu += sent_count;
    
    return sent_count;
}

const ECURoutingEntry_t* diagnostic_router_find_ecu(
    const DiagnosticRouter_t* router,
    uint16_t logical_address
) {
    if (!router) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < router->ecu_count; i++) {
        if (router->ecus[i].logical_address == logical_address) {
            return &router->ecus[i];
        }
    }
    
    return NULL;
}

void diagnostic_router_update_activity(
    DiagnosticRouter_t* router,
    uint16_t logical_address
) {
    if (!router) {
        return;
    }
    
    for (uint32_t i = 0; i < router->ecu_count; i++) {
        if (router->ecus[i].logical_address == logical_address) {
            router->ecus[i].last_activity_time_ms = get_current_time_ms();
            router->ecus[i].is_connected = true;
            break;
        }
    }
}

void diagnostic_router_check_timeouts(DiagnosticRouter_t* router) {
    if (!router) {
        return;
    }
    
    uint32_t current_time = get_current_time_ms();
    
    for (uint32_t i = 0; i < DIAG_ROUTER_MAX_PENDING; i++) {
        PendingDiagRequest_t* req = &router->pending[i];
        
        if (req->is_active) {
            uint32_t elapsed = current_time - req->timestamp_ms;
            
            if (elapsed > DIAG_ROUTER_TIMEOUT_MS) {
                debug_print("[DiagRouter] Request timed out: 0x%04X -> 0x%04X\n",
                            req->source_address, req->target_address);
                
                // TODO: Send negative response (timeout)
                
                req->is_active = false;
                router->routing_errors++;
            }
        }
    }
}

void diagnostic_router_get_stats(
    const DiagnosticRouter_t* router,
    uint32_t* total_requests,
    uint32_t* routed_to_ecu,
    uint32_t* routed_to_vmg,
    uint32_t* routing_errors
) {
    if (!router) {
        return;
    }
    
    if (total_requests) *total_requests = router->total_requests;
    if (routed_to_ecu) *routed_to_ecu = router->routed_to_ecu;
    if (routed_to_vmg) *routed_to_vmg = router->routed_to_vmg;
    if (routing_errors) *routing_errors = router->routing_errors;
}

