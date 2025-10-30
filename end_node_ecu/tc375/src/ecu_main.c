/**
 * @file ecu_main.c
 * @brief End Node ECU Main Application for TC375
 */

#include "ecu_node.h"
#include <stdio.h>
#include <unistd.h>

/* ECU Configuration */
#define ECU_ID              "TC375-ECU-002-Zone1-ECU1"
#define ECU_LOGICAL_ADDR    0x0201
#define ZG_IP               "192.168.1.10"
#define ZG_PORT             13400

int main(void) {
    printf("╔════════════════════════════════════════╗\n");
    printf("║  End Node ECU (TC375)                  ║\n");
    printf("║  ECU ID: %s          ║\n", ECU_ID);
    printf("╚════════════════════════════════════════╝\n\n");
    
    ECUNode_t ecu;
    
    /* Initialize ECU */
    printf("[INIT] Initializing ECU Node...\n");
    if (ecu_init(&ecu, ECU_ID, ECU_LOGICAL_ADDR, ZG_IP, ZG_PORT) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize ECU\n");
        return -1;
    }
    
    ecu_print_info(&ecu);
    
    /* Start ECU */
    printf("\n[START] Starting ECU Node...\n");
    if (ecu_start(&ecu) != 0) {
        fprintf(stderr, "ERROR: Failed to start ECU\n");
        return -1;
    }
    
    /* Main loop */
    printf("[OPERATION] Entering main loop...\n");
    printf("  - Heartbeat to ZG: Every 10 seconds\n");
    printf("  - VCI update: Every 60 seconds\n\n");
    
    while (1) {
        ecu_run(&ecu);
        usleep(10000); // 10ms
    }
    
    /* Cleanup */
    ecu_stop(&ecu);
    printf("ECU Node stopped.\n");
    
    return 0;
}

