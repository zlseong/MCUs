/**
 * Pure PQC DoIP Client Implementation for TC375
 */

#include "pqc_doip_client.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

PQC_DoIP_Client::PQC_DoIP_Client(const std::string& vmg_host, uint16_t vmg_port,
                                 uint16_t source_address,
                                 PQC_KEM kem, PQC_SIG sig)
    : ctx_(nullptr), ssl_(nullptr), sock_fd_(-1),
      vmg_host_(vmg_host), vmg_port_(vmg_port),
      source_address_(source_address),
      connected_(false), routing_activated_(false) {
    
    // Configure PQC algorithm (Pure PQC only)
    switch (kem) {
        case PQC_KEM::MLKEM512:
            config_.kem = kem;
            config_.kem_name = "ML-KEM-512";
            config_.openssl_groups = "mlkem512";
            break;
        case PQC_KEM::MLKEM768:
            config_.kem = kem;
            config_.kem_name = "ML-KEM-768";
            config_.openssl_groups = "mlkem768";
            break;
        case PQC_KEM::MLKEM1024:
            config_.kem = kem;
            config_.kem_name = "ML-KEM-1024";
            config_.openssl_groups = "mlkem1024";
            break;
    }
    
    switch (sig) {
        case PQC_SIG::MLDSA44:
            config_.sig = sig;
            config_.sig_name = "ML-DSA-44";
            config_.openssl_sigalgs = "mldsa44";
            break;
        case PQC_SIG::MLDSA65:
            config_.sig = sig;
            config_.sig_name = "ML-DSA-65";
            config_.openssl_sigalgs = "mldsa65";
            break;
        case PQC_SIG::MLDSA87:
            config_.sig = sig;
            config_.sig_name = "ML-DSA-87";
            config_.openssl_sigalgs = "mldsa87";
            break;
    }
    
    std::cout << "[TC375] PQC Configuration: "
              << config_.kem_name << " + " << config_.sig_name << std::endl;
}

PQC_DoIP_Client::~PQC_DoIP_Client() {
    disconnect();
}

bool PQC_DoIP_Client::configure_pqc() {
    // TLS 1.3 only
    SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx_, TLS1_3_VERSION);
    
    // Set KEM groups
    if (SSL_CTX_set1_groups_list(ctx_, config_.openssl_groups.c_str()) != 1) {
        std::cerr << "[TC375] Failed to set groups: " << config_.openssl_groups << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    // Set signature algorithms
    if (SSL_CTX_set1_sigalgs_list(ctx_, config_.openssl_sigalgs.c_str()) != 1) {
        std::cerr << "[TC375] Failed to set sigalgs: " << config_.openssl_sigalgs << std::endl;
        ERR_print_errors_fp(stderr);
        return false;
    }
    
    // Cipher suites
    if (SSL_CTX_set_ciphersuites(ctx_, "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256") != 1) {
        std::cerr << "[TC375] Failed to set cipher suites" << std::endl;
        return false;
    }
    
    // Verify peer
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
    
    return true;
}

bool PQC_DoIP_Client::connect(const std::string& cert_file,
                              const std::string& key_file,
                              const std::string& ca_file) {
    
    // Create SSL context
    ctx_ = SSL_CTX_new(TLS_client_method());
    if (!ctx_) {
        std::cerr << "[TC375] Failed to create SSL context" << std::endl;
        return false;
    }
    
    // Configure PQC
    if (!configure_pqc()) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    // Load certificates (mTLS)
    if (SSL_CTX_use_certificate_file(ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        std::cerr << "[TC375] Failed to load certificate: " << cert_file << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    if (SSL_CTX_use_PrivateKey_file(ctx_, key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        std::cerr << "[TC375] Failed to load private key: " << key_file << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    if (!SSL_CTX_check_private_key(ctx_)) {
        std::cerr << "[TC375] Private key does not match certificate" << std::endl;
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    if (SSL_CTX_load_verify_locations(ctx_, ca_file.c_str(), nullptr) != 1) {
        std::cerr << "[TC375] Failed to load CA certificate: " << ca_file << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    // Resolve hostname
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", vmg_port_);
    
    struct addrinfo* result;
    if (getaddrinfo(vmg_host_.c_str(), port_str, &hints, &result) != 0) {
        std::cerr << "[TC375] Failed to resolve: " << vmg_host_ << std::endl;
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    // Connect to VMG
    for (struct addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        sock_fd_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd_ < 0) continue;
        
        if (::connect(sock_fd_, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        
        close(sock_fd_);
        sock_fd_ = -1;
    }
    
    freeaddrinfo(result);
    
    if (sock_fd_ < 0) {
        std::cerr << "[TC375] Failed to connect to VMG: " 
                  << vmg_host_ << ":" << vmg_port_ << std::endl;
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    std::cout << "[TC375] TCP connected to VMG: " 
              << vmg_host_ << ":" << vmg_port_ << std::endl;
    
    // Create SSL
    ssl_ = SSL_new(ctx_);
    if (!ssl_) {
        close(sock_fd_);
        sock_fd_ = -1;
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    SSL_set_fd(ssl_, sock_fd_);
    SSL_set_tlsext_host_name(ssl_, vmg_host_.c_str());
    
    // TLS handshake
    if (SSL_connect(ssl_) <= 0) {
        std::cerr << "[TC375] TLS handshake failed" << std::endl;
        ERR_print_errors_fp(stderr);
        SSL_free(ssl_);
        ssl_ = nullptr;
        close(sock_fd_);
        sock_fd_ = -1;
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
        return false;
    }
    
    std::cout << "[TC375] TLS handshake successful" << std::endl;
    std::cout << "[TC375] Cipher: " << SSL_get_cipher(ssl_) << std::endl;
    std::cout << "[TC375] Protocol: " << SSL_get_version(ssl_) << std::endl;
    
    connected_ = true;
    return true;
}

void PQC_DoIP_Client::disconnect() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
    
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
    
    connected_ = false;
    routing_activated_ = false;
}

int PQC_DoIP_Client::send_doip_packet(uint16_t payload_type, 
                                       const uint8_t* payload, 
                                       uint32_t payload_len) {
    uint8_t header[DOIP_HEADER_SIZE];
    
    header[0] = DOIP_PROTOCOL_VERSION;
    header[1] = ~DOIP_PROTOCOL_VERSION;
    header[2] = (payload_type >> 8) & 0xFF;
    header[3] = payload_type & 0xFF;
    header[4] = (payload_len >> 24) & 0xFF;
    header[5] = (payload_len >> 16) & 0xFF;
    header[6] = (payload_len >> 8) & 0xFF;
    header[7] = payload_len & 0xFF;
    
    if (SSL_write(ssl_, header, DOIP_HEADER_SIZE) != DOIP_HEADER_SIZE) {
        return -1;
    }
    
    if (payload_len > 0) {
        if (SSL_write(ssl_, payload, payload_len) != (int)payload_len) {
            return -1;
        }
    }
    
    return payload_len + DOIP_HEADER_SIZE;
}

int PQC_DoIP_Client::recv_doip_packet(uint16_t* payload_type,
                                       uint8_t* payload,
                                       uint32_t max_len) {
    uint8_t header[DOIP_HEADER_SIZE];
    
    if (SSL_read(ssl_, header, DOIP_HEADER_SIZE) != DOIP_HEADER_SIZE) {
        return -1;
    }
    
    *payload_type = (header[2] << 8) | header[3];
    uint32_t payload_len = (header[4] << 24) | (header[5] << 16) |
                          (header[6] << 8) | header[7];
    
    if (payload_len > max_len) {
        return -1;
    }
    
    if (payload_len > 0) {
        if (SSL_read(ssl_, payload, payload_len) != (int)payload_len) {
            return -1;
        }
    }
    
    return payload_len;
}

bool PQC_DoIP_Client::send_routing_activation() {
    if (!connected_) return false;
    
    uint8_t payload[11];
    payload[0] = (source_address_ >> 8) & 0xFF;
    payload[1] = source_address_ & 0xFF;
    payload[2] = 0x00; // Activation type
    payload[3] = 0x00; payload[4] = 0x00;
    payload[5] = 0x00; payload[6] = 0x00;
    
    std::cout << "[TC375] Sending routing activation request..." << std::endl;
    
    if (send_doip_packet(DOIP_ROUTING_ACTIVATION_REQ, payload, 7) < 0) {
        std::cerr << "[TC375] Failed to send routing activation" << std::endl;
        return false;
    }
    
    // Wait for response
    uint16_t resp_type;
    uint8_t resp_payload[256];
    int resp_len = recv_doip_packet(&resp_type, resp_payload, sizeof(resp_payload));
    
    if (resp_len < 0 || resp_type != DOIP_ROUTING_ACTIVATION_RES) {
        std::cerr << "[TC375] Failed to receive routing activation response" << std::endl;
        return false;
    }
    
    uint8_t result_code = resp_payload[4];
    if (result_code == 0x10) {
        std::cout << "[TC375] Routing activated successfully" << std::endl;
        routing_activated_ = true;
        return true;
    } else {
        std::cerr << "[TC375] Routing activation failed: 0x" 
                  << std::hex << (int)result_code << std::dec << std::endl;
        return false;
    }
}

bool PQC_DoIP_Client::send_diagnostic_message(uint16_t target_address,
                                              const std::vector<uint8_t>& data) {
    if (!routing_activated_) return false;
    
    std::vector<uint8_t> payload(4 + data.size());
    payload[0] = (source_address_ >> 8) & 0xFF;
    payload[1] = source_address_ & 0xFF;
    payload[2] = (target_address >> 8) & 0xFF;
    payload[3] = target_address & 0xFF;
    memcpy(&payload[4], data.data(), data.size());
    
    return send_doip_packet(DOIP_DIAGNOSTIC_MESSAGE, payload.data(), payload.size()) > 0;
}

std::vector<uint8_t> PQC_DoIP_Client::receive_diagnostic_message() {
    if (!routing_activated_) return {};
    
    uint16_t payload_type;
    uint8_t payload[4096];
    int len = recv_doip_packet(&payload_type, payload, sizeof(payload));
    
    if (len < 0 || payload_type != DOIP_DIAGNOSTIC_MESSAGE) {
        return {};
    }
    
    if (len < 4) return {};
    
    // Skip source/target address (4 bytes)
    return std::vector<uint8_t>(payload + 4, payload + len);
}

