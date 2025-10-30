/**
 * HTTPS Client with PQC for VMG
 * External server communication (OTA, Fleet API)
 */

#include <iostream>
#include <string>
#include <cstring>

extern "C" {
#include "pqc_config.h"
typedef struct PQC_Client PQC_Client;
PQC_Client* pqc_client_create(const char* hostname, uint16_t port,
                              const PQC_Config* config,
                              const char* cert_file,
                              const char* key_file,
                              const char* ca_file);
int pqc_client_write(PQC_Client* client, const void* data, size_t len);
int pqc_client_read(PQC_Client* client, void* buf, size_t len);
void pqc_client_destroy(PQC_Client* client);
}

class HTTPSClient {
private:
    PQC_Client* client;
    std::string hostname;
    uint16_t port;
    
public:
    HTTPSClient(const std::string& host, uint16_t p, const PQC_Config* config,
                const std::string& cert, const std::string& key, const std::string& ca)
        : client(nullptr), hostname(host), port(p) {
        
        client = pqc_client_create(host.c_str(), port, config,
                                   cert.c_str(), key.c_str(), ca.c_str());
        
        if (!client) {
            throw std::runtime_error("Failed to create HTTPS client");
        }
    }
    
    ~HTTPSClient() {
        if (client) {
            pqc_client_destroy(client);
        }
    }
    
    std::string get(const std::string& path) {
        std::string request = 
            "GET " + path + " HTTP/1.1\r\n"
            "Host: " + hostname + "\r\n"
            "User-Agent: VMG/1.0\r\n"
            "Connection: close\r\n"
            "\r\n";
        
        if (pqc_client_write(client, request.c_str(), request.size()) < 0) {
            throw std::runtime_error("Failed to send request");
        }
        
        std::string response;
        char buf[4096];
        int n;
        
        while ((n = pqc_client_read(client, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            response += buf;
        }
        
        return response;
    }
    
    std::string post(const std::string& path, const std::string& body,
                     const std::string& content_type = "application/json") {
        std::string request =
            "POST " + path + " HTTP/1.1\r\n"
            "Host: " + hostname + "\r\n"
            "User-Agent: VMG/1.0\r\n"
            "Content-Type: " + content_type + "\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + body;
        
        if (pqc_client_write(client, request.c_str(), request.size()) < 0) {
            throw std::runtime_error("Failed to send request");
        }
        
        std::string response;
        char buf[4096];
        int n;
        
        while ((n = pqc_client_read(client, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            response += buf;
        }
        
        return response;
    }
};

// Example usage
int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] 
                  << " <url> <cert> <key> <ca>" << std::endl;
        return 1;
    }
    
    std::string url = argv[1];
    std::string cert = argv[2];
    std::string key = argv[3];
    std::string ca = argv[4];
    
    // Parse URL (simple)
    std::string hostname;
    uint16_t port = 443;
    std::string path = "/";
    
    if (url.substr(0, 8) == "https://") {
        url = url.substr(8);
    }
    
    size_t slash_pos = url.find('/');
    if (slash_pos != std::string::npos) {
        hostname = url.substr(0, slash_pos);
        path = url.substr(slash_pos);
    } else {
        hostname = url;
    }
    
    size_t colon_pos = hostname.find(':');
    if (colon_pos != std::string::npos) {
        port = std::stoi(hostname.substr(colon_pos + 1));
        hostname = hostname.substr(0, colon_pos);
    }
    
    // Use ML-KEM for external servers
    const PQC_Config* config = &PQC_CONFIGS[4]; // ML-KEM-768 + ML-DSA-65 (recommended)
    
    std::cout << "========================================" << std::endl;
    std::cout << "VMG HTTPS Client with PQC" << std::endl;
    std::cout << "========================================" << std::endl;
    pqc_print_config(config);
    std::cout << "Target: " << hostname << ":" << port << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        HTTPSClient client(hostname, port, config, cert, key, ca);
        
        std::cout << "\n[HTTPS] Sending GET " << path << std::endl;
        std::string response = client.get(path);
        
        std::cout << "\n[Response]" << std::endl;
        std::cout << response << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

