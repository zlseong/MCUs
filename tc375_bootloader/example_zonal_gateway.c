/**
 * @file example_zonal_gateway.c
 * @brief Zonal Gateway Usage Example
 * 
 * MCU #1 역할: Zonal Gateway
 * - Zone 내 ECU들의 서버 (DoIP Server + JSON Server)
 * - VMG의 클라이언트 (DoIP Client)
 */

#include "common/zonal_gateway.h"
#include "common/doip_client.h"
#include "common/uds_handler.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Zone Configuration */
#define ZONE_ID         1
#define VMG_IP          "192.168.1.1"
#define VMG_PORT        13400

/* Thread for server operations */
void* zg_server_thread(void* arg);

/* Thread for client operations */
void* zg_client_thread(void* arg);

int main(void) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Zonal Gateway Example (MCU #1)        ║\n");
    printf("║  Zone ID: %d                            ║\n", ZONE_ID);
    printf("╚════════════════════════════════════════╝\n\n");
    
    ZonalGateway_t zg;
    
    /* Initialize Zonal Gateway */
    printf("[INIT] Initializing Zonal Gateway...\n");
    if (zg_init(&zg, ZONE_ID, VMG_IP, VMG_PORT) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize Zonal Gateway\n");
        return -1;
    }
    printf("[INIT] Zone ID: %d\n", ZONE_ID);
    printf("[INIT] ZG ID: %s\n", zg.zg_id);
    printf("[INIT] VMG: %s:%d\n\n", VMG_IP, VMG_PORT);
    
    /* Start Zonal Gateway */
    printf("[START] Starting Zonal Gateway services...\n");
    if (zg_start(&zg) != 0) {
        fprintf(stderr, "ERROR: Failed to start Zonal Gateway\n");
        return -1;
    }
    printf("[START] DoIP Server: 0.0.0.0:%d (TCP/UDP)\n", ZG_DOIP_SERVER_PORT);
    printf("[START] JSON Server: 0.0.0.0:%d (TCP)\n\n", ZG_JSON_SERVER_PORT);
    
    /* Phase 1: Discovery - Zone 내 ECU 찾기 */
    printf("═════════════════════════════════════════\n");
    printf("Phase 1: ECU Discovery (Zone %d)\n", ZONE_ID);
    printf("═════════════════════════════════════════\n");
    printf("[DISCOVERY] Waiting for ECUs to connect...\n");
    printf("[DISCOVERY] ECUs should:\n");
    printf("  1. Send UDP broadcast to 255.255.255.255:13400\n");
    printf("  2. Receive VIN and logical address\n");
    printf("  3. Connect via TCP to ZG\n\n");
    
    /* Wait for ECUs (simulated) */
    sleep(5);
    
    /* Simulate ECU registration */
    printf("[DISCOVERY] ECU #2 connected!\n");
    ZoneECUInfo_t ecu2 = {
        .ecu_id = "TC375-SIM-002-Zone1-ECU1",
        .logical_address = 0x0201,
        .firmware_version = "1.0.0",
        .hardware_version = "TC375TP-LiteKit-v2.0",
        .is_online = true,
        .ota_capable = true,
        .delta_update_supported = true,
        .max_package_size = 10485760
    };
    zg_update_ecu_info(&zg, ecu2.ecu_id, &ecu2);
    
    printf("[DISCOVERY] ECU #3 connected!\n");
    ZoneECUInfo_t ecu3 = {
        .ecu_id = "TC375-SIM-003-Zone1-ECU2",
        .logical_address = 0x0202,
        .firmware_version = "1.0.0",
        .hardware_version = "TC375TP-LiteKit-v2.0",
        .is_online = true,
        .ota_capable = true,
        .delta_update_supported = false,
        .max_package_size = 5242880
    };
    zg_update_ecu_info(&zg, ecu3.ecu_id, &ecu3);
    
    printf("[DISCOVERY] Zone %d: %d ECUs discovered\n\n", ZONE_ID, zg.zone_vci.ecu_count);
    zg_print_zone_vci(&zg);
    
    /* Phase 2: Connect to VMG */
    printf("\n═════════════════════════════════════════\n");
    printf("Phase 2: Connect to VMG (CCU)\n");
    printf("═════════════════════════════════════════\n");
    if (zg_connect_to_vmg(&zg) != 0) {
        fprintf(stderr, "ERROR: Failed to connect to VMG\n");
        return -1;
    }
    printf("[VMG] Connected to VMG at %s:%d\n", VMG_IP, VMG_PORT);
    printf("[VMG] Routing activation successful\n\n");
    
    /* Phase 3: Send Zone VCI to VMG */
    printf("═════════════════════════════════════════\n");
    printf("Phase 3: Send Zone VCI to VMG\n");
    printf("═════════════════════════════════════════\n");
    if (zg_send_zone_vci_to_vmg(&zg) != 0) {
        fprintf(stderr, "ERROR: Failed to send Zone VCI\n");
        return -1;
    }
    printf("[VCI] Zone VCI sent to VMG\n");
    printf("[VCI] Zone %d: %d ECUs\n", ZONE_ID, zg.zone_vci.ecu_count);
    printf("[VCI] Total storage: %u MB\n", zg.zone_vci.total_storage_mb);
    printf("[VCI] Available: %u MB\n\n", zg.zone_vci.available_storage_mb);
    
    /* Phase 4: Periodic operations */
    printf("═════════════════════════════════════════\n");
    printf("Phase 4: Normal Operation\n");
    printf("═════════════════════════════════════════\n");
    printf("[OPERATION] Entering main loop...\n");
    printf("  - Heartbeat to VMG: Every 10 seconds\n");
    printf("  - Zone status: Every 60 seconds\n");
    printf("  - ECU monitoring: Continuous\n\n");
    
    /* Main loop (10 iterations for demo) */
    for (int i = 0; i < 10; i++) {
        printf("[%d] Heartbeat to VMG...\n", i + 1);
        zg_send_heartbeat_to_vmg(&zg);
        
        if ((i + 1) % 6 == 0) {
            printf("[%d] Zone status report...\n", i + 1);
            zg_send_zone_status_to_vmg(&zg);
        }
        
        sleep(10);
    }
    
    /* Phase 5: Simulate OTA Update */
    printf("\n═════════════════════════════════════════\n");
    printf("Phase 5: OTA Update Simulation\n");
    printf("═════════════════════════════════════════\n");
    
    const char* campaign_id = "OTA-2025-001";
    printf("[OTA] Checking readiness for campaign: %s\n", campaign_id);
    
    if (zg_check_ota_readiness(&zg, campaign_id)) {
        printf("[OTA] Zone %d is ready for OTA\n", ZONE_ID);
        printf("[OTA] Battery: %u%%\n", zg.zone_vci.average_battery_level);
        printf("[OTA] Storage: %u MB available\n", zg.zone_vci.available_storage_mb);
        
        /* Simulate OTA progress */
        printf("[OTA] Starting OTA update...\n");
        for (int progress = 0; progress <= 100; progress += 10) {
            printf("[OTA] Progress: %d%%\n", progress);
            zg_report_ota_progress(&zg, progress);
            sleep(1);
        }
        printf("[OTA] OTA update completed successfully!\n");
    } else {
        printf("[OTA] Zone %d is NOT ready for OTA\n", ZONE_ID);
    }
    
    /* Cleanup */
    printf("\n═════════════════════════════════════════\n");
    printf("Shutting down Zonal Gateway...\n");
    printf("═════════════════════════════════════════\n");
    zg_stop(&zg);
    printf("Zonal Gateway stopped.\n");
    
    return 0;
}

/* Pseudo-implementation of ZG functions (실제 구현은 zonal_gateway.c에) */

int zg_init(ZonalGateway_t* zg, uint8_t zone_id, const char* vmg_ip, uint16_t vmg_port) {
    if (!zg) return -1;
    
    memset(zg, 0, sizeof(ZonalGateway_t));
    zg->zone_id = zone_id;
    snprintf(zg->zg_id, sizeof(zg->zg_id), "ZG-%03d", zone_id);
    zg->logical_address = 0x0200 + zone_id; // 0x0201, 0x0202...
    zg->state = ZG_STATE_INIT;
    
    /* Initialize VMG client */
    doip_client_init(&zg->vmg_client, vmg_ip, vmg_port, 
                     zg->logical_address, 0x0100);
    
    /* Initialize UDS handler */
    uds_handler_init(&zg->uds_handler);
    
    return 0;
}

int zg_start(ZonalGateway_t* zg) {
    if (!zg) return -1;
    
    /* TODO: Create server sockets */
    /* zg->doip_server_tcp_socket = ... */
    /* zg->doip_server_udp_socket = ... */
    /* zg->json_server_socket = ... */
    
    zg->state = ZG_STATE_READY;
    return 0;
}

void zg_stop(ZonalGateway_t* zg) {
    if (!zg) return;
    
    /* Close all sockets */
    if (zg->vmg_client.tcp_socket >= 0) {
        doip_client_disconnect(&zg->vmg_client);
    }
    
    zg->state = ZG_STATE_INIT;
}

int zg_connect_to_vmg(ZonalGateway_t* zg) {
    if (!zg) return -1;
    
    /* Connect to VMG */
    if (doip_client_connect(&zg->vmg_client) != 0) {
        return -1;
    }
    
    /* Routing activation */
    if (doip_client_routing_activation(&zg->vmg_client, 0x00) != 0) {
        return -1;
    }
    
    zg->vmg_connected = true;
    return 0;
}

int zg_send_zone_vci_to_vmg(ZonalGateway_t* zg) {
    if (!zg || !zg->vmg_connected) return -1;
    
    /* Build Zone VCI JSON message */
    char json_msg[4096];
    snprintf(json_msg, sizeof(json_msg),
             "{"
             "\"message_type\":\"ZONE_VCI_REPORT\","
             "\"zone_id\":%d,"
             "\"ecu_count\":%d,"
             "\"ecus\":["
             // ... ECU 정보 ...
             "]"
             "}",
             zg->zone_id, zg->zone_vci.ecu_count);
    
    /* Send via DoIP diagnostic message or JSON socket */
    /* doip_client_send_diagnostic(...) */
    
    return 0;
}

int zg_send_heartbeat_to_vmg(ZonalGateway_t* zg) {
    if (!zg || !zg->vmg_connected) return -1;
    
    /* Send heartbeat */
    uint8_t heartbeat[] = {0x3E, 0x00}; // Tester Present
    uint8_t response[256];
    size_t resp_len;
    
    return doip_client_send_diagnostic(&zg->vmg_client,
                                       heartbeat, sizeof(heartbeat),
                                       response, sizeof(response), &resp_len);
}

int zg_send_zone_status_to_vmg(ZonalGateway_t* zg) {
    if (!zg || !zg->vmg_connected) return -1;
    
    /* Build zone status */
    /* Send to VMG */
    
    return 0;
}

int zg_update_ecu_info(ZonalGateway_t* zg, const char* ecu_id, const ZoneECUInfo_t* info) {
    if (!zg || !ecu_id || !info) return -1;
    
    /* Find or add ECU */
    int idx = -1;
    for (uint8_t i = 0; i < zg->zone_vci.ecu_count; i++) {
        if (strcmp(zg->zone_vci.ecus[i].ecu_id, ecu_id) == 0) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        /* Add new ECU */
        if (zg->zone_vci.ecu_count >= ZG_MAX_ECUS) return -1;
        idx = zg->zone_vci.ecu_count++;
    }
    
    /* Update info */
    memcpy(&zg->zone_vci.ecus[idx], info, sizeof(ZoneECUInfo_t));
    
    return 0;
}

bool zg_check_ota_readiness(ZonalGateway_t* zg, const char* campaign_id) {
    if (!zg) return false;
    
    /* Check zone conditions */
    zg->zone_vci.average_battery_level = 85;
    zg->zone_vci.total_storage_mb = 512;
    zg->zone_vci.available_storage_mb = 256;
    
    if (zg->zone_vci.average_battery_level < 50) return false;
    if (zg->zone_vci.available_storage_mb < 100) return false;
    
    return true;
}

int zg_report_ota_progress(ZonalGateway_t* zg, uint8_t progress_percentage) {
    if (!zg || !zg->vmg_connected) return -1;
    
    /* Send OTA progress to VMG */
    /* JSON message or UDS message */
    
    return 0;
}

void zg_print_zone_vci(const ZonalGateway_t* zg) {
    if (!zg) return;
    
    printf("\n┌─────────────────────────────────────────┐\n");
    printf("│ Zone %d VCI Summary                      │\n", zg->zone_id);
    printf("├─────────────────────────────────────────┤\n");
    printf("│ ECU Count: %d                            │\n", zg->zone_vci.ecu_count);
    printf("├─────────────────────────────────────────┤\n");
    
    for (uint8_t i = 0; i < zg->zone_vci.ecu_count; i++) {
        const ZoneECUInfo_t* ecu = &zg->zone_vci.ecus[i];
        printf("│ ECU #%d: %s\n", i + 1, ecu->ecu_id);
        printf("│   Address: 0x%04X\n", ecu->logical_address);
        printf("│   FW Ver:  %s\n", ecu->firmware_version);
        printf("│   HW Ver:  %s\n", ecu->hardware_version);
        printf("│   OTA:     %s\n", ecu->ota_capable ? "YES" : "NO");
        printf("│   Delta:   %s\n", ecu->delta_update_supported ? "YES" : "NO");
        printf("│\n");
    }
    
    printf("└─────────────────────────────────────────┘\n");
}

const char* zg_get_zone_name(uint8_t zone_id) {
    static char name[32];
    snprintf(name, sizeof(name), "Zone_%d", zone_id);
    return name;
}

