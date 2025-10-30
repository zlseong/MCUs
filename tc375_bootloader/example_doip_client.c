/**
 * @file example_doip_client.c
 * @brief DoIP Client Usage Example
 * 
 * Demonstrates how to use the DoIP client for TC375 MCU.
 * This example mirrors the Python DoIPClient usage.
 */

#include "common/doip_client.h"
#include "common/uds_handler.h"
#include <stdio.h>
#include <string.h>

/* Example configuration */
#define SERVER_IP           "192.168.1.100"
#define SERVER_PORT         13400
#define TESTER_ADDRESS      0x0E00
#define ECU_ADDRESS         0x0100

/* Example UDS operations */
static int example_session_control(DoIPClient_t* client);
static int example_read_vin(DoIPClient_t* client);
static int example_read_dtc(DoIPClient_t* client);
static void print_hex(const char* label, const uint8_t* data, size_t len);

int main(void) {
    DoIPClient_t client;
    int result;

    printf("=== DoIP Client Example ===\n\n");

    /* Initialize client */
    result = doip_client_init(&client, SERVER_IP, SERVER_PORT, TESTER_ADDRESS, ECU_ADDRESS);
    if (result != 0) {
        printf("ERROR: Failed to initialize client\n");
        return -1;
    }

    /* Vehicle identification (optional, UDP broadcast) */
    printf("1. Performing vehicle identification...\n");
    char vin[18];
    result = doip_client_vehicle_identification(&client, vin);
    if (result == 0) {
        printf("   Vehicle VIN: %s\n\n", vin);
    } else {
        printf("   WARNING: Vehicle identification failed (continuing anyway)\n\n");
    }

    /* Connect to DoIP server (TCP) */
    printf("2. Connecting to DoIP server...\n");
    result = doip_client_connect(&client);
    if (result != 0) {
        printf("ERROR: Failed to connect to server\n");
        return -1;
    }
    printf("   Connected successfully\n\n");

    /* Routing activation */
    printf("3. Activating routing...\n");
    result = doip_client_routing_activation(&client, 0x00);
    if (result != 0) {
        printf("ERROR: Routing activation failed\n");
        doip_client_disconnect(&client);
        return -1;
    }
    printf("   Routing activated successfully\n\n");

    /* Perform UDS operations */
    printf("4. Performing UDS diagnostics...\n\n");

    /* Session Control */
    if (example_session_control(&client) != 0) {
        printf("ERROR: Session control failed\n");
        doip_client_disconnect(&client);
        return -1;
    }

    /* Read VIN */
    if (example_read_vin(&client) != 0) {
        printf("ERROR: VIN read failed\n");
    }

    /* Read DTC */
    if (example_read_dtc(&client) != 0) {
        printf("ERROR: DTC read failed\n");
    }

    /* Disconnect */
    printf("\n5. Disconnecting...\n");
    doip_client_disconnect(&client);
    printf("   Disconnected\n");

    printf("\n=== Example Complete ===\n");
    return 0;
}

static int example_session_control(DoIPClient_t* client) {
    printf("   [UDS] Session Control (0x10 01)...\n");

    uint8_t request[] = {0x10, 0x01};  /* Default session */
    uint8_t response[256];
    size_t resp_len;

    int result = doip_client_send_diagnostic(
        client,
        request,
        sizeof(request),
        response,
        sizeof(response),
        &resp_len
    );

    if (result != 0) {
        printf("   ERROR: Failed to send diagnostic message\n");
        return -1;
    }

    print_hex("   Response", response, resp_len);

    /* Verify positive response */
    if (resp_len >= 2 && response[0] == 0x50 && response[1] == 0x01) {
        printf("   SUCCESS: Session control successful\n\n");
        return 0;
    }

    printf("   ERROR: Unexpected response\n\n");
    return -1;
}

static int example_read_vin(DoIPClient_t* client) {
    printf("   [UDS] Read Data By Identifier - VIN (0x22 F190)...\n");

    uint8_t request[] = {0x22, 0xF1, 0x90};
    uint8_t response[256];
    size_t resp_len;

    int result = doip_client_send_diagnostic(
        client,
        request,
        sizeof(request),
        response,
        sizeof(response),
        &resp_len
    );

    if (result != 0) {
        printf("   ERROR: Failed to send diagnostic message\n");
        return -1;
    }

    print_hex("   Response", response, resp_len);

    /* Verify positive response */
    if (resp_len >= 20 && response[0] == 0x62 && response[1] == 0xF1 && response[2] == 0x90) {
        char vin[18];
        memcpy(vin, &response[3], 17);
        vin[17] = '\0';
        printf("   VIN: %s\n\n", vin);
        return 0;
    }

    printf("   ERROR: Unexpected response\n\n");
    return -1;
}

static int example_read_dtc(DoIPClient_t* client) {
    printf("   [UDS] Read DTC Information (0x19 02 FF)...\n");

    uint8_t request[] = {0x19, 0x02, 0xFF};
    uint8_t response[1024];
    size_t resp_len;

    int result = doip_client_send_diagnostic(
        client,
        request,
        sizeof(request),
        response,
        sizeof(response),
        &resp_len
    );

    if (result != 0) {
        printf("   ERROR: Failed to send diagnostic message\n");
        return -1;
    }

    print_hex("   Response", response, resp_len);

    /* Verify positive response */
    if (resp_len >= 2 && response[0] == 0x59) {
        printf("   DTC read successful\n\n");
        return 0;
    }

    printf("   ERROR: Unexpected response\n\n");
    return -1;
}

static void print_hex(const char* label, const uint8_t* data, size_t len) {
    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 32) {
        printf("...");
    }
    printf("\n");
}

