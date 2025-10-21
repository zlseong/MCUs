#pragma once

#include <string>
#include <memory>
#include <functional>
#include <openssl/ssl.h>
#include <openssl/err.h>

namespace tc375 {

class TlsClient {
public:
    TlsClient(const std::string& host, int port);
    ~TlsClient();

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }

    // Data transfer
    bool send(const std::string& data);
    std::string receive(size_t max_len = 4096);

    // TLS configuration
    void setVerifyPeer(bool verify) { verify_peer_ = verify; }
    void setCACertPath(const std::string& path) { ca_cert_path_ = path; }
    void setClientCertPath(const std::string& cert, const std::string& key);

    // Callbacks
    using ErrorCallback = std::function<void(const std::string&)>;
    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }

private:
    std::string host_;
    int port_;
    int socket_fd_;
    bool connected_;

    SSL_CTX* ssl_ctx_;
    SSL* ssl_;

    bool verify_peer_;
    std::string ca_cert_path_;
    std::string client_cert_path_;
    std::string client_key_path_;

    ErrorCallback error_callback_;

    bool initSSL();
    void cleanupSSL();
    bool createSocket();
    void handleError(const std::string& error);
};

} // namespace tc375

