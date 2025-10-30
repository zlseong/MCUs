/**
 * @file zonal_gateway.c
 * @brief Zonal Gateway Implementation for TC375
 */

#include "zonal_gateway.h"
#include "doip_client.h"
#include "doip_message.h"
#include "uds_handler.h"
#include <stdio.h>
#include <string.h>

/* Platform-specific includes would go here */
/* #include "lwip/tcp.h" */
/* #include "lwip/udp.h" */

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
    
    /* Initialize zone VCI */
    zg->zone_vci.zone_id = zone_id;
    zg->zone_vci.ecu_count = 0;
    
    return 0;
}

int zg_start(ZonalGateway_t* zg) {
    if (!zg) return -1;
    
    /* TODO: Create server sockets */
    /* For TC375, this would use lwIP or custom network stack */
    /* zg->doip_server_tcp_socket = lwip_tcp_listen(ZG_DOIP_SERVER_PORT); */
    /* zg->doip_server_udp_socket = lwip_udp_bind(ZG_DOIP_SERVER_PORT); */
    /* zg->json_server_socket = lwip_tcp_listen(ZG_JSON_SERVER_PORT); */
    
    zg->state = ZG_STATE_READY;
    return 0;
}

void zg_stop(ZonalGateway_t* zg) {
    if (!zg) return;
    
    /* Close all sockets */
    if (zg->vmg_client.tcp_socket >= 0) {
        doip_client_disconnect(&zg->vmg_client);
    }
    
    /* Close server sockets */
    /* lwip_close(zg->doip_server_tcp_socket); */
    /* lwip_close(zg->doip_server_udp_socket); */
    /* lwip_close(zg->json_server_socket); */
    
    zg->state = ZG_STATE_INIT;
}

void zg_run(ZonalGateway_t* zg) {
    if (!zg) return;
    
    /* Non-blocking main loop */
    /* Handle incoming connections */
    /* Check VMG connection status */
    /* Process queued messages */
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
    
    /* Build Zone VCI message */
    /* Send via DoIP diagnostic message */
    
    return 0;
}

int zg_send_heartbeat_to_vmg(ZonalGateway_t* zg) {
    if (!zg || !zg->vmg_connected) return -1;
    
    /* Send Tester Present (0x3E 0x00) */
    uint8_t heartbeat[] = {0x3E, 0x00};
    uint8_t response[256];
    size_t resp_len;
    
    return doip_client_send_diagnostic(&zg->vmg_client,
                                       heartbeat, sizeof(heartbeat),
                                       response, sizeof(response), &resp_len);
}

int zg_send_zone_status_to_vmg(ZonalGateway_t* zg) {
    if (!zg || !zg->vmg_connected) return -1;
    
    /* Send zone status */
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
    if (zg->zone_vci.average_battery_level < 50) return false;
    if (zg->zone_vci.available_storage_mb < 100) return false;
    
    /* Check all ECUs online */
    for (uint8_t i = 0; i < zg->zone_vci.ecu_count; i++) {
        if (!zg->zone_vci.ecus[i].is_online) return false;
    }
    
    return true;
}

int zg_report_ota_progress(ZonalGateway_t* zg, uint8_t progress_percentage) {
    if (!zg || !zg->vmg_connected) return -1;
    
    /* Send OTA progress to VMG */
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
        printf("│\n");
    }
    
    printf("└─────────────────────────────────────────┘\n");
}

const char* zg_get_zone_name(uint8_t zone_id) {
    static char name[32];
    snprintf(name, sizeof(name), "Zone_%d", zone_id);
    return name;
}

