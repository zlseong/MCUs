/**
 * C++ Wrapper for PQC TLS
 */

#ifndef PQC_TLS_WRAPPER_HPP
#define PQC_TLS_WRAPPER_HPP

#include <string>
#include <memory>

extern "C" {
#include "pqc_config.h"
typedef struct PQC_Server PQC_Server;
typedef struct PQC_Client PQC_Client;
}

class PQCServer {
private:
    PQC_Server* server;
    
public:
    PQCServer(uint16_t port, const PQC_Config* config,
              const std::string& cert, const std::string& key, const std::string& ca);
    ~PQCServer();
    
    bool accept(SSL** out_ssl, int* out_fd);
};

class PQCClient {
private:
    PQC_Client* client;
    
public:
    PQCClient(const std::string& hostname, uint16_t port,
              const PQC_Config* config,
              const std::string& cert, const std::string& key, const std::string& ca);
    ~PQCClient();
    
    int write(const void* data, size_t len);
    int read(void* buf, size_t len);
};

#endif

