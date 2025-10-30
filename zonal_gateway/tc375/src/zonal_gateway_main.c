/**
 * @file zonal_gateway_main.c
 * @brief Zonal Gateway Main Application for TC375
 * 
 * MCU #1 역할: Zonal Gateway
 * - Zone 내 ECU들의 서버 (DoIP Server + JSON Server)
 * - VMG의 클라이언트 (DoIP Client)
 */

#include "zonal_gateway.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Zone Configuration */
#define ZONE_ID         1
#define VMG_IP          "192.168.1.1"
#define VMG_PORT        13400

int main(void) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Zonal Gateway (TC375)                 ║\n");
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
    
    /* Main loop */
    printf("[OPERATION] Entering main loop...\n");
    while (1) {
        zg_run(&zg);
        usleep(10000); // 10ms
    }
    
    /* Cleanup */
    zg_stop(&zg);
    printf("Zonal Gateway stopped.\n");
    
    return 0;
}

