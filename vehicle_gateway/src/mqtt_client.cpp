/**
 * MQTT Client Main for VMG
 */

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>

extern "C" {
#include "pqc_config.h"

typedef struct MQTT_Client MQTT_Client;
MQTT_Client* mqtt_client_create(const char* broker_url,
                                const PQC_Config* config,
                                const char* cert_file,
                                const char* key_file,
                                const char* ca_file);
int mqtt_publish(MQTT_Client* mqtt, const char* topic,
                const void* payload, size_t payload_len, uint8_t qos);
void mqtt_client_destroy(MQTT_Client* mqtt);
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] 
                  << " <broker_url> <cert> <key> <ca>" << std::endl;
        std::cerr << "Example: " << argv[0]
                  << " mqtts://broker.example.com:8883 "
                  << "certs/mlkem768_mldsa65_client.crt "
                  << "certs/mlkem768_mldsa65_client.key "
                  << "certs/ca.crt" << std::endl;
        return 1;
    }
    
    std::string broker_url = argv[1];
    std::string cert = argv[2];
    std::string key = argv[3];
    std::string ca = argv[4];
    
    // PQC Configuration for External Server (MQTT Broker)
    // Change this to test different parameters (0-5)
    const int PQC_CONFIG_ID = 1; // Default: ML-KEM-768 + ECDSA-P256
    const PQC_Config* config = &PQC_CONFIGS[PQC_CONFIG_ID];
    
    std::cout << "========================================" << std::endl;
    std::cout << "VMG MQTT Client with PQC" << std::endl;
    std::cout << "========================================" << std::endl;
    pqc_print_config(config);
    std::cout << "Broker: " << broker_url << std::endl;
    std::cout << "========================================" << std::endl;
    
    MQTT_Client* mqtt = mqtt_client_create(broker_url.c_str(), config,
                                           cert.c_str(), key.c_str(), ca.c_str());
    
    if (!mqtt) {
        std::cerr << "Failed to create MQTT client" << std::endl;
        return 1;
    }
    
    std::cout << "\n[MQTT] Publishing telemetry..." << std::endl;
    
    // Example: Publish telemetry every 5 seconds
    for (int i = 0; i < 10; i++) {
        char payload[256];
        snprintf(payload, sizeof(payload),
                 "{\"timestamp\":%ld,\"vehicle_id\":\"VMG-001\","
                 "\"speed\":%.1f,\"battery\":%.1f}",
                 time(NULL), 60.0 + i * 5.0, 80.0 - i * 2.0);
        
        if (mqtt_publish(mqtt, "vmg/telemetry", payload, strlen(payload), 1)) {
            std::cout << "[" << i + 1 << "/10] Published: " << payload << std::endl;
        } else {
            std::cerr << "Failed to publish" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "\n[MQTT] Disconnecting..." << std::endl;
    mqtt_client_destroy(mqtt);
    
    return 0;
}

