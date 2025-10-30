/**
 * @file ecu_node.c
 * @brief End Node ECU Implementation for TC375
 */

#include "ecu_node.h"
#include "doip_client.h"
#include "uds_handler.h"
#include <stdio.h>
#include <string.h>

int ecu_init(ECUNode_t* ecu, const char* ecu_id, uint16_t logical_addr,
             const char* zg_ip, uint16_t zg_port) {
    if (!ecu || !ecu_id || !zg_ip) return -1;
    
    memset(ecu, 0, sizeof(ECUNode_t));
    
    /* Set identity */
    strncpy(ecu->ecu_id, ecu_id, sizeof(ecu->ecu_id) - 1);
    ecu->logical_address = logical_addr;
    strncpy(ecu->firmware_version, "1.0.0", sizeof(ecu->firmware_version) - 1);
    strncpy(ecu->hardware_version, "TC375TP-LiteKit-v2.0", sizeof(ecu->hardware_version) - 1);
    
    /* Set Zone Gateway connection info */
    strncpy(ecu->zg_ip, zg_ip, sizeof(ecu->zg_ip) - 1);
    ecu->zg_port = zg_port;
    
    /* Initialize DoIP client */
    doip_client_init(&ecu->zg_client, zg_ip, zg_port, logical_addr, 0x0200);
    
    /* Initialize UDS handler */
    uds_handler_init(&ecu->uds_handler);
    
    /* Set capabilities */
    ecu->ota_capable = true;
    ecu->delta_update_supported = true;
    ecu->max_package_size = 10485760; // 10MB
    
    ecu->state = ECU_STATE_INIT;
    
    return 0;
}

int ecu_start(ECUNode_t* ecu) {
    if (!ecu) return -1;
    
    /* Connect to Zone Gateway */
    if (ecu_connect_to_zg(ecu) != 0) {
        fprintf(stderr, "[ECU] Failed to connect to Zone Gateway\n");
        return -1;
    }
    
    ecu->state = ECU_STATE_READY;
    printf("[ECU] ECU Node started: %s\n", ecu->ecu_id);
    
    return 0;
}

void ecu_stop(ECUNode_t* ecu) {
    if (!ecu) return;
    
    /* Disconnect from Zone Gateway */
    if (ecu->zg_connected) {
        doip_client_disconnect(&ecu->zg_client);
        ecu->zg_connected = false;
    }
    
    ecu->state = ECU_STATE_INIT;
    printf("[ECU] ECU Node stopped: %s\n", ecu->ecu_id);
}

void ecu_run(ECUNode_t* ecu) {
    if (!ecu) return;
    
    uint32_t current_time = ecu_get_tick_ms();
    
    /* Send heartbeat */
    if (current_time - ecu->last_heartbeat_time >= ECU_HEARTBEAT_INTERVAL_MS) {
        ecu_send_heartbeat(ecu);
        ecu->last_heartbeat_time = current_time;
    }
    
    /* Send VCI update */
    if (current_time - ecu->last_vci_update_time >= ECU_VCI_UPDATE_INTERVAL_MS) {
        ecu_send_vci_info(ecu);
        ecu->last_vci_update_time = current_time;
    }
    
    /* Handle incoming messages */
    /* TODO: Implement message handling */
}

int ecu_discover_zone_gateway(ECUNode_t* ecu) {
    if (!ecu) return -1;
    
    /* Send UDP broadcast for vehicle identification */
    char vin[18];
    if (doip_client_vehicle_identification(&ecu->zg_client, vin) == 0) {
        printf("[ECU] Discovered Zone Gateway, VIN: %s\n", vin);
        return 0;
    }
    
    return -1;
}

int ecu_connect_to_zg(ECUNode_t* ecu) {
    if (!ecu) return -1;
    
    printf("[ECU] Connecting to Zone Gateway: %s:%d\n", ecu->zg_ip, ecu->zg_port);
    
    /* Connect via TCP */
    if (doip_client_connect(&ecu->zg_client) != 0) {
        fprintf(stderr, "[ECU] Failed to connect to Zone Gateway\n");
        return -1;
    }
    
    /* Routing activation */
    if (doip_client_routing_activation(&ecu->zg_client, 0x00) != 0) {
        fprintf(stderr, "[ECU] Routing activation failed\n");
        doip_client_disconnect(&ecu->zg_client);
        return -1;
    }
    
    ecu->zg_connected = true;
    printf("[ECU] Connected to Zone Gateway\n");
    
    return 0;
}

int ecu_send_heartbeat(ECUNode_t* ecu) {
    if (!ecu || !ecu->zg_connected) return -1;
    
    /* Send Tester Present (0x3E 0x00) */
    uint8_t request[] = {0x3E, 0x00};
    uint8_t response[256];
    size_t resp_len;
    
    return doip_client_send_diagnostic(&ecu->zg_client,
                                       request, sizeof(request),
                                       response, sizeof(response), &resp_len);
}

int ecu_send_vci_info(ECUNode_t* ecu) {
    if (!ecu || !ecu->zg_connected) return -1;
    
    /* Send VCI information to Zone Gateway */
    /* TODO: Implement VCI data serialization and send */
    
    printf("[ECU] Sent VCI info to Zone Gateway\n");
    return 0;
}

int ecu_handle_uds_request(ECUNode_t* ecu,
                           const uint8_t* request, size_t req_len,
                           uint8_t* response, size_t resp_cap, size_t* resp_len) {
    if (!ecu || !request || !response || !resp_len) return -1;
    
    /* Handle UDS request using UDS handler */
    return uds_handle_request(&ecu->uds_handler, request, req_len,
                              response, resp_cap, resp_len);
}

bool ecu_check_ota_readiness(ECUNode_t* ecu) {
    if (!ecu) return false;
    
    /* Check battery level */
    /* Check storage space */
    /* Check vehicle state */
    
    return true;
}

int ecu_receive_ota_firmware(ECUNode_t* ecu, const uint8_t* firmware_data, size_t firmware_size) {
    if (!ecu || !firmware_data || firmware_size == 0) return -1;
    
    printf("[ECU] Receiving OTA firmware: %zu bytes\n", firmware_size);
    
    /* Write firmware to inactive bank */
    /* This would use flash_write() on actual hardware */
    /* TODO: Implement flash writing */
    
    ecu->state = ECU_STATE_OTA_IN_PROGRESS;
    
    return 0;
}

int ecu_install_ota_firmware(ECUNode_t* ecu) {
    if (!ecu) return -1;
    
    printf("[ECU] Installing OTA firmware...\n");
    
    /* Set boot flag to inactive bank */
    /* This would trigger bank switch on next boot */
    /* TODO: Implement bank switching */
    
    /* Reboot ECU */
    /* system_reset(); */
    
    return 0;
}

int ecu_report_ota_result(ECUNode_t* ecu, bool success) {
    if (!ecu || !ecu->zg_connected) return -1;
    
    printf("[ECU] Reporting OTA result: %s\n", success ? "SUCCESS" : "FAILED");
    
    /* Send OTA result to Zone Gateway */
    /* TODO: Implement result reporting */
    
    return 0;
}

void ecu_print_info(const ECUNode_t* ecu) {
    if (!ecu) return;
    
    printf("\n┌─────────────────────────────────────────┐\n");
    printf("│ ECU Node Information                    │\n");
    printf("├─────────────────────────────────────────┤\n");
    printf("│ ECU ID:       %s\n", ecu->ecu_id);
    printf("│ Address:      0x%04X\n", ecu->logical_address);
    printf("│ FW Version:   %s\n", ecu->firmware_version);
    printf("│ HW Version:   %s\n", ecu->hardware_version);
    printf("│ Zone Gateway: %s:%d\n", ecu->zg_ip, ecu->zg_port);
    printf("│ Connected:    %s\n", ecu->zg_connected ? "YES" : "NO");
    printf("│ OTA Capable:  %s\n", ecu->ota_capable ? "YES" : "NO");
    printf("│ Delta Update: %s\n", ecu->delta_update_supported ? "YES" : "NO");
    printf("└─────────────────────────────────────────┘\n");
}

uint32_t ecu_get_tick_ms(void) {
    /* Platform-specific implementation */
    /* For TC375, this would use STM (System Timer Module) */
    /* TODO: Implement actual timer */
    return 0;
}

