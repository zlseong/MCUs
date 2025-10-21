#include "tls_client.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace tc375 {

TlsClient::TlsClient(const std::string& host, int port)
    : host_(host)
    , port_(port)
    , socket_fd_(-1)
    , connected_(false)
    , ssl_ctx_(nullptr)
    , ssl_(nullptr)
    , verify_peer_(false)
{
}

TlsClient::~TlsClient() {
    disconnect();
}

bool TlsClient::initSSL() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    const SSL_METHOD* method = TLS_client_method();
    ssl_ctx_ = SSL_CTX_new(method);
    
    if (!ssl_ctx_) {
        handleError("Failed to create SSL context");
        return false;
    }

    // Set TLS 1.3
    SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_3_VERSION);

    if (!verify_peer_) {
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
    } else {
        if (!ca_cert_path_.empty()) {
            if (SSL_CTX_load_verify_locations(ssl_ctx_, ca_cert_path_.c_str(), nullptr) != 1) {
                handleError("Failed to load CA certificate");
                return false;
            }
        }
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
    }

    // Load client certificate if provided
    if (!client_cert_path_.empty() && !client_key_path_.empty()) {
        if (SSL_CTX_use_certificate_file(ssl_ctx_, client_cert_path_.c_str(), SSL_FILETYPE_PEM) != 1) {
            handleError("Failed to load client certificate");
            return false;
        }
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, client_key_path_.c_str(), SSL_FILETYPE_PEM) != 1) {
            handleError("Failed to load client private key");
            return false;
        }
    }

    return true;
}

void TlsClient::cleanupSSL() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
}

bool TlsClient::createSocket() {
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port_);
    if (getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &result) != 0) {
        handleError("Failed to resolve hostname");
        return false;
    }

    socket_fd_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_fd_ < 0) {
        freeaddrinfo(result);
        handleError("Failed to create socket");
        return false;
    }

    if (::connect(socket_fd_, result->ai_addr, result->ai_addrlen) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        freeaddrinfo(result);
        handleError("Failed to connect to server");
        return false;
    }

    freeaddrinfo(result);
    return true;
}

bool TlsClient::connect() {
    if (connected_) {
        return true;
    }

    std::cout << "[TLS Client] Connecting to " << host_ << ":" << port_ << std::endl;

    // Initialize SSL
    if (!initSSL()) {
        return false;
    }

    // Create socket
    if (!createSocket()) {
        cleanupSSL();
        return false;
    }

    // Create SSL object
    ssl_ = SSL_new(ssl_ctx_);
    if (!ssl_) {
        handleError("Failed to create SSL object");
        close(socket_fd_);
        socket_fd_ = -1;
        cleanupSSL();
        return false;
    }

    SSL_set_fd(ssl_, socket_fd_);

    // Perform TLS handshake
    int ret = SSL_connect(ssl_);
    if (ret != 1) {
        int err = SSL_get_error(ssl_, ret);
        std::string error_msg = "TLS handshake failed: " + std::to_string(err);
        handleError(error_msg);
        cleanupSSL();
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "[TLS Client] Connected successfully" << std::endl;
    std::cout << "[TLS Client] Using cipher: " << SSL_get_cipher(ssl_) << std::endl;
    return true;
}

void TlsClient::disconnect() {
    if (!connected_) {
        return;
    }

    std::cout << "[TLS Client] Disconnecting..." << std::endl;
    
    cleanupSSL();
    
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }

    connected_ = false;
}

bool TlsClient::send(const std::string& data) {
    if (!connected_ || !ssl_) {
        handleError("Not connected");
        return false;
    }

    int ret = SSL_write(ssl_, data.c_str(), data.size());
    if (ret <= 0) {
        int err = SSL_get_error(ssl_, ret);
        handleError("Failed to send data: " + std::to_string(err));
        return false;
    }

    return true;
}

std::string TlsClient::receive(size_t max_len) {
    if (!connected_ || !ssl_) {
        handleError("Not connected");
        return "";
    }

    char buffer[max_len];
    int ret = SSL_read(ssl_, buffer, max_len - 1);
    
    if (ret <= 0) {
        int err = SSL_get_error(ssl_, ret);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            handleError("Failed to receive data: " + std::to_string(err));
        }
        return "";
    }

    buffer[ret] = '\0';
    return std::string(buffer, ret);
}

void TlsClient::setClientCertPath(const std::string& cert, const std::string& key) {
    client_cert_path_ = cert;
    client_key_path_ = key;
}

void TlsClient::handleError(const std::string& error) {
    std::cerr << "[TLS Client] Error: " << error << std::endl;
    if (error_callback_) {
        error_callback_(error);
    }
}

} // namespace tc375

