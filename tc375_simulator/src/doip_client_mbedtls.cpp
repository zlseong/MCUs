/**
 * TC375 DoIP Client with mbedTLS
 * Standard TLS 1.3 (no PQC)
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>

extern "C" {
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/certs.h>
}

// DoIP Protocol Constants (ISO 13400)
#define DOIP_PROTOCOL_VERSION 0x02
#define DOIP_HEADER_SIZE 8

// DoIP Payload Types
#define DOIP_ROUTING_ACTIVATION_REQ   0x0005
#define DOIP_ROUTING_ACTIVATION_RES   0x0006
#define DOIP_DIAGNOSTIC_MESSAGE       0x8001

class TC375_DoIP_Client {
private:
    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
    
    uint16_t source_address;
    bool connected;
    bool routing_activated;
    
public:
    TC375_DoIP_Client(uint16_t src_addr) 
        : source_address(src_addr), connected(false), routing_activated(false) {
        
        mbedtls_net_init(&server_fd);
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt_init(&cacert);
        mbedtls_x509_crt_init(&clicert);
        mbedtls_pk_init(&pkey);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);
    }
    
    ~TC375_DoIP_Client() {
        disconnect();
    }
    
    bool connect(const std::string& host, uint16_t port,
                const std::string& cert_file,
                const std::string& key_file,
                const std::string& ca_file) {
        
        int ret;
        const char *pers = "tc375_client";
        
        // Seed RNG
        if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
                                         &entropy,
                                         (const unsigned char *)pers,
                                         strlen(pers))) != 0) {
            std::cerr << "[TC375] Failed to seed RNG: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        // Load CA certificate
        if ((ret = mbedtls_x509_crt_parse_file(&cacert, ca_file.c_str())) != 0) {
            std::cerr << "[TC375] Failed to load CA: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        // Load client certificate
        if ((ret = mbedtls_x509_crt_parse_file(&clicert, cert_file.c_str())) != 0) {
            std::cerr << "[TC375] Failed to load cert: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        // Load private key
        if ((ret = mbedtls_pk_parse_keyfile(&pkey, key_file.c_str(), NULL,
                                            mbedtls_ctr_drbg_random,
                                            &ctr_drbg)) != 0) {
            std::cerr << "[TC375] Failed to load key: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        // Connect to VMG
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%u", port);
        
        if ((ret = mbedtls_net_connect(&server_fd, host.c_str(), port_str,
                                       MBEDTLS_NET_PROTO_TCP)) != 0) {
            std::cerr << "[TC375] Failed to connect: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        std::cout << "[TC375] Connected to " << host << ":" << port << std::endl;
        
        // Setup SSL
        if ((ret = mbedtls_ssl_config_defaults(&conf,
                                              MBEDTLS_SSL_IS_CLIENT,
                                              MBEDTLS_SSL_TRANSPORT_STREAM,
                                              MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
            std::cerr << "[TC375] Failed to set defaults: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        // TLS 1.3 only
        mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
        mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
        
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
        
        // Set client certificate (mutual TLS)
        if ((ret = mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey)) != 0) {
            std::cerr << "[TC375] Failed to set client cert: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
            std::cerr << "[TC375] Failed to setup SSL: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        if ((ret = mbedtls_ssl_set_hostname(&ssl, host.c_str())) != 0) {
            std::cerr << "[TC375] Failed to set hostname: -0x" << std::hex << -ret << std::dec << std::endl;
            return false;
        }
        
        mbedtls_ssl_set_bio(&ssl, &server_fd,
                           mbedtls_net_send, mbedtls_net_recv, NULL);
        
        // Perform handshake
        std::cout << "[TC375] Performing TLS handshake..." << std::endl;
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
                ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                std::cerr << "[TC375] Handshake failed: -0x" << std::hex << -ret << std::dec << std::endl;
                return false;
            }
        }
        
        std::cout << "[TC375] TLS handshake complete" << std::endl;
        std::cout << "[TC375] Cipher: " << mbedtls_ssl_get_ciphersuite(&ssl) << std::endl;
        
        // Verify peer certificate
        uint32_t flags;
        if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0) {
            char vrfy_buf[512];
            mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
            std::cerr << "[TC375] Certificate verification failed:\n" << vrfy_buf << std::endl;
            return false;
        }
        
        connected = true;
        return true;
    }
    
    void disconnect() {
        if (connected) {
            mbedtls_ssl_close_notify(&ssl);
        }
        
        mbedtls_net_free(&server_fd);
        mbedtls_x509_crt_free(&clicert);
        mbedtls_x509_crt_free(&cacert);
        mbedtls_pk_free(&pkey);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        
        connected = false;
        routing_activated = false;
    }
    
    bool send_routing_activation() {
        if (!connected) return false;
        
        unsigned char packet[256];
        int pos = 0;
        
        // DoIP header
        packet[pos++] = DOIP_PROTOCOL_VERSION;
        packet[pos++] = ~DOIP_PROTOCOL_VERSION;
        packet[pos++] = (DOIP_ROUTING_ACTIVATION_REQ >> 8) & 0xFF;
        packet[pos++] = DOIP_ROUTING_ACTIVATION_REQ & 0xFF;
        
        uint32_t payload_len = 7;
        packet[pos++] = (payload_len >> 24) & 0xFF;
        packet[pos++] = (payload_len >> 16) & 0xFF;
        packet[pos++] = (payload_len >> 8) & 0xFF;
        packet[pos++] = payload_len & 0xFF;
        
        // Payload
        packet[pos++] = (source_address >> 8) & 0xFF;
        packet[pos++] = source_address & 0xFF;
        packet[pos++] = 0x00; // Activation type
        packet[pos++] = 0x00; packet[pos++] = 0x00;
        packet[pos++] = 0x00; packet[pos++] = 0x00;
        
        std::cout << "[TC375] Sending routing activation..." << std::endl;
        
        if (mbedtls_ssl_write(&ssl, packet, pos) < 0) {
            std::cerr << "[TC375] Failed to send routing activation" << std::endl;
            return false;
        }
        
        // Wait for response
        unsigned char response[256];
        int n = mbedtls_ssl_read(&ssl, response, sizeof(response));
        if (n < DOIP_HEADER_SIZE) {
            std::cerr << "[TC375] Failed to receive response" << std::endl;
            return false;
        }
        
        uint16_t resp_type = (response[2] << 8) | response[3];
        if (resp_type == DOIP_ROUTING_ACTIVATION_RES) {
            uint8_t result_code = response[12];
            if (result_code == 0x10) {
                std::cout << "[TC375] Routing activated" << std::endl;
                routing_activated = true;
                return true;
            }
        }
        
        std::cerr << "[TC375] Routing activation failed" << std::endl;
        return false;
    }
    
    bool send_diagnostic_message(const std::vector<uint8_t>& data) {
        if (!routing_activated) return false;
        
        unsigned char packet[4096];
        int pos = 0;
        
        // DoIP header
        packet[pos++] = DOIP_PROTOCOL_VERSION;
        packet[pos++] = ~DOIP_PROTOCOL_VERSION;
        packet[pos++] = (DOIP_DIAGNOSTIC_MESSAGE >> 8) & 0xFF;
        packet[pos++] = DOIP_DIAGNOSTIC_MESSAGE & 0xFF;
        
        uint32_t payload_len = 4 + data.size();
        packet[pos++] = (payload_len >> 24) & 0xFF;
        packet[pos++] = (payload_len >> 16) & 0xFF;
        packet[pos++] = (payload_len >> 8) & 0xFF;
        packet[pos++] = payload_len & 0xFF;
        
        // Payload
        packet[pos++] = (source_address >> 8) & 0xFF;
        packet[pos++] = source_address & 0xFF;
        packet[pos++] = 0x00; packet[pos++] = 0x01; // Target address
        memcpy(packet + pos, data.data(), data.size());
        pos += data.size();
        
        return mbedtls_ssl_write(&ssl, packet, pos) > 0;
    }
};

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <vmg_host> <vmg_port> <cert> <key> <ca>" << std::endl;
        std::cerr << "Example: " << argv[0]
                  << " 192.168.1.1 13400 "
                  << "certs/tc375_client.crt "
                  << "certs/tc375_client.key "
                  << "certs/ca.crt" << std::endl;
        return 1;
    }
    
    std::string vmg_host = argv[1];
    uint16_t vmg_port = atoi(argv[2]);
    std::string cert = argv[3];
    std::string key = argv[4];
    std::string ca = argv[5];
    
    std::cout << "========================================" << std::endl;
    std::cout << "TC375 DoIP Client with mbedTLS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "VMG: " << vmg_host << ":" << vmg_port << std::endl;
    std::cout << "Protocol: TLS 1.3 (Standard, no PQC)" << std::endl;
    std::cout << "Auth: Mutual TLS" << std::endl;
    std::cout << "========================================" << std::endl;
    
    TC375_DoIP_Client client(0x0E80);
    
    if (!client.connect(vmg_host, vmg_port, cert, key, ca)) {
        std::cerr << "Failed to connect to VMG" << std::endl;
        return 1;
    }
    
    if (!client.send_routing_activation()) {
        std::cerr << "Failed to activate routing" << std::endl;
        return 1;
    }
    
    std::cout << "\n[TC375] Sending diagnostic messages..." << std::endl;
    
    for (int i = 0; i < 5; i++) {
        std::vector<uint8_t> data = {0x10, 0x01}; // DiagnosticSessionControl
        
        if (client.send_diagnostic_message(data)) {
            std::cout << "[" << i + 1 << "/5] Sent diagnostic message" << std::endl;
        }
        
        sleep(1);
    }
    
    std::cout << "\n[TC375] Disconnecting..." << std::endl;
    client.disconnect();
    
    return 0;
}

