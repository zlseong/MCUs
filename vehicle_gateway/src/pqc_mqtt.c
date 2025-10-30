/**
 * MQTT Client with PQC for VMG
 * Lightweight MQTT 3.1.1 client over PQC TLS
 */

#include "pqc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Forward declarations
typedef struct PQC_Client PQC_Client;
extern PQC_Client* pqc_client_create(const char* hostname, uint16_t port,
                                     const PQC_Config* config,
                                     const char* cert_file,
                                     const char* key_file,
                                     const char* ca_file);
extern int pqc_client_write(PQC_Client* client, const void* data, size_t len);
extern int pqc_client_read(PQC_Client* client, void* buf, size_t len);
extern void pqc_client_destroy(PQC_Client* client);

// MQTT Control Packet Types
#define MQTT_CONNECT     1
#define MQTT_CONNACK     2
#define MQTT_PUBLISH     3
#define MQTT_PUBACK      4
#define MQTT_SUBSCRIBE   8
#define MQTT_SUBACK      9
#define MQTT_PINGREQ     12
#define MQTT_PINGRESP    13
#define MQTT_DISCONNECT  14

typedef struct {
    PQC_Client* tls_client;
    char client_id[64];
    uint16_t packet_id;
} MQTT_Client;

static int mqtt_encode_length(uint8_t* buf, size_t len) {
    int i = 0;
    do {
        uint8_t d = len % 128;
        len /= 128;
        if (len > 0) d |= 0x80;
        buf[i++] = d;
    } while (len > 0);
    return i;
}

static int mqtt_send_connect(MQTT_Client* mqtt) {
    uint8_t packet[256];
    int pos = 0;
    
    // Fixed header
    packet[pos++] = (MQTT_CONNECT << 4);
    
    // Variable header
    const char* protocol = "MQTT";
    size_t protocol_len = strlen(protocol);
    
    // Calculate remaining length
    size_t remaining_len = 
        2 + protocol_len +  // Protocol Name
        1 +                 // Protocol Level
        1 +                 // Connect Flags
        2 +                 // Keep Alive
        2 + strlen(mqtt->client_id);  // Client ID
    
    pos += mqtt_encode_length(&packet[pos], remaining_len);
    
    // Protocol Name
    packet[pos++] = (protocol_len >> 8) & 0xFF;
    packet[pos++] = protocol_len & 0xFF;
    memcpy(&packet[pos], protocol, protocol_len);
    pos += protocol_len;
    
    // Protocol Level (3.1.1 = 4)
    packet[pos++] = 4;
    
    // Connect Flags (Clean Session)
    packet[pos++] = 0x02;
    
    // Keep Alive (60 seconds)
    packet[pos++] = 0x00;
    packet[pos++] = 0x3C;
    
    // Client ID
    size_t client_id_len = strlen(mqtt->client_id);
    packet[pos++] = (client_id_len >> 8) & 0xFF;
    packet[pos++] = client_id_len & 0xFF;
    memcpy(&packet[pos], mqtt->client_id, client_id_len);
    pos += client_id_len;
    
    return pqc_client_write(mqtt->tls_client, packet, pos);
}

static int mqtt_recv_connack(MQTT_Client* mqtt) {
    uint8_t buf[4];
    if (pqc_client_read(mqtt->tls_client, buf, 4) != 4) {
        return 0;
    }
    
    if ((buf[0] >> 4) != MQTT_CONNACK) {
        fprintf(stderr, "[MQTT] Expected CONNACK, got 0x%02X\n", buf[0]);
        return 0;
    }
    
    if (buf[3] != 0) {
        fprintf(stderr, "[MQTT] Connection refused: %d\n", buf[3]);
        return 0;
    }
    
    printf("[MQTT] Connected\n");
    return 1;
}

MQTT_Client* mqtt_client_create(const char* broker_url,
                                const PQC_Config* config,
                                const char* cert_file,
                                const char* key_file,
                                const char* ca_file) {
    // Parse broker URL (mqtts://hostname:port)
    char hostname[256] = {0};
    uint16_t port = 8883;
    
    if (strncmp(broker_url, "mqtts://", 8) == 0) {
        const char* host_start = broker_url + 8;
        const char* colon = strchr(host_start, ':');
        
        if (colon) {
            size_t len = colon - host_start;
            if (len >= sizeof(hostname)) len = sizeof(hostname) - 1;
            memcpy(hostname, host_start, len);
            port = atoi(colon + 1);
        } else {
            strncpy(hostname, host_start, sizeof(hostname) - 1);
        }
    } else {
        strncpy(hostname, broker_url, sizeof(hostname) - 1);
    }
    
    // Create TLS client
    PQC_Client* tls_client = pqc_client_create(hostname, port, config,
                                               cert_file, key_file, ca_file);
    if (!tls_client) {
        return NULL;
    }
    
    MQTT_Client* mqtt = calloc(1, sizeof(MQTT_Client));
    if (!mqtt) {
        pqc_client_destroy(tls_client);
        return NULL;
    }
    
    mqtt->tls_client = tls_client;
    snprintf(mqtt->client_id, sizeof(mqtt->client_id), "vmg_%d", getpid());
    mqtt->packet_id = 1;
    
    // Send CONNECT
    if (mqtt_send_connect(mqtt) < 0) {
        fprintf(stderr, "[MQTT] Failed to send CONNECT\n");
        free(mqtt);
        pqc_client_destroy(tls_client);
        return NULL;
    }
    
    // Wait for CONNACK
    if (!mqtt_recv_connack(mqtt)) {
        free(mqtt);
        pqc_client_destroy(tls_client);
        return NULL;
    }
    
    return mqtt;
}

int mqtt_publish(MQTT_Client* mqtt, const char* topic, 
                const void* payload, size_t payload_len, uint8_t qos) {
    if (!mqtt || !topic) return 0;
    
    uint8_t packet[2048];
    int pos = 0;
    
    // Fixed header
    packet[pos++] = (MQTT_PUBLISH << 4) | (qos << 1);
    
    size_t topic_len = strlen(topic);
    size_t remaining_len = 2 + topic_len + payload_len;
    if (qos > 0) remaining_len += 2;
    
    pos += mqtt_encode_length(&packet[pos], remaining_len);
    
    // Topic
    packet[pos++] = (topic_len >> 8) & 0xFF;
    packet[pos++] = topic_len & 0xFF;
    memcpy(&packet[pos], topic, topic_len);
    pos += topic_len;
    
    // Packet ID (for QoS > 0)
    if (qos > 0) {
        packet[pos++] = (mqtt->packet_id >> 8) & 0xFF;
        packet[pos++] = mqtt->packet_id & 0xFF;
        mqtt->packet_id++;
    }
    
    // Payload
    if (payload_len > 0) {
        memcpy(&packet[pos], payload, payload_len);
        pos += payload_len;
    }
    
    return pqc_client_write(mqtt->tls_client, packet, pos) > 0;
}

void mqtt_client_destroy(MQTT_Client* mqtt) {
    if (!mqtt) return;
    
    // Send DISCONNECT
    uint8_t disconnect[] = {(MQTT_DISCONNECT << 4), 0};
    pqc_client_write(mqtt->tls_client, disconnect, 2);
    
    pqc_client_destroy(mqtt->tls_client);
    free(mqtt);
}

