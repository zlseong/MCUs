/**
 * Pure PQC DoIP Client for TC375
 * Connects to VMG Gateway with PQC TLS
 */

#ifndef PQC_DOIP_CLIENT_HPP
#define PQC_DOIP_CLIENT_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <openssl/ssl.h>
#include <openssl/err.h>

// DoIP Protocol Constants (ISO 13400)
#define DOIP_PROTOCOL_VERSION 0x02
#define DOIP_HEADER_SIZE 8

// DoIP Payload Types
#define DOIP_ROUTING_ACTIVATION_REQ   0x0005
#define DOIP_ROUTING_ACTIVATION_RES   0x0006
#define DOIP_DIAGNOSTIC_MESSAGE       0x8001
#define DOIP_DIAGNOSTIC_ACK           0x8002
#define DOIP_DIAGNOSTIC_NACK          0x8003

enum class PQC_KEM {
    MLKEM512,
    MLKEM768,
    MLKEM1024
};

enum class PQC_SIG {
    MLDSA44,
    MLDSA65,
    MLDSA87
};

struct PQC_Config_TC375 {
    PQC_KEM kem;
    PQC_SIG sig;
    std::string kem_name;
    std::string sig_name;
    std::string openssl_groups;
    std::string openssl_sigalgs;
};

class PQC_DoIP_Client {
private:
    SSL_CTX* ctx_;
    SSL* ssl_;
    int sock_fd_;
    std::string vmg_host_;
    uint16_t vmg_port_;
    uint16_t source_address_;
    bool connected_;
    bool routing_activated_;
    
    PQC_Config_TC375 config_;
    
public:
    PQC_DoIP_Client(const std::string& vmg_host, uint16_t vmg_port,
                    uint16_t source_address,
                    PQC_KEM kem = PQC_KEM::MLKEM768,
                    PQC_SIG sig = PQC_SIG::MLDSA65);
    
    ~PQC_DoIP_Client();
    
    bool connect(const std::string& cert_file,
                const std::string& key_file,
                const std::string& ca_file);
    
    void disconnect();
    
    bool send_routing_activation();
    
    bool send_diagnostic_message(uint16_t target_address,
                                 const std::vector<uint8_t>& data);
    
    std::vector<uint8_t> receive_diagnostic_message();
    
    bool is_connected() const { return connected_ && routing_activated_; }
    
private:
    bool configure_pqc();
    bool perform_tls_handshake();
    int send_doip_packet(uint16_t payload_type, const uint8_t* payload, uint32_t payload_len);
    int recv_doip_packet(uint16_t* payload_type, uint8_t* payload, uint32_t max_len);
};

#endif

