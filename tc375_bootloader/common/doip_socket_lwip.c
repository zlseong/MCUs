/**
 * @file doip_socket_lwip.c
 * @brief DoIP Socket Implementation using lwIP stack
 * 
 * Platform-specific implementation for TC375 using lwIP TCP/IP stack.
 * Adapt this file for your specific RTOS/network stack.
 */

#include "doip_client.h"
#include <string.h>

/* 
 * NOTE: This is a template implementation.
 * Replace lwIP includes with your actual TCP/IP stack headers.
 */

/* Uncomment when porting to actual TC375 with lwIP */
#if 0
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#endif

/* For demonstration, use POSIX sockets (remove when porting) */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

DoIPSocket_t doip_socket_tcp_create(void) {
    DoIPSocket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return DOIP_INVALID_SOCKET;
    }

    /* Set socket options */
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    return sock;
}

DoIPSocket_t doip_socket_udp_create(void) {
    DoIPSocket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return DOIP_INVALID_SOCKET;
    }

    /* Enable broadcast */
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

    return sock;
}

int doip_socket_tcp_connect(DoIPSocket_t sock, uint32_t ip, uint16_t port) {
    if (sock == DOIP_INVALID_SOCKET) {
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = ip;  /* Already in network byte order */
    server_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return -1;
    }

    return 0;
}

int doip_socket_tcp_send(DoIPSocket_t sock, const uint8_t* data, size_t len) {
    if (sock == DOIP_INVALID_SOCKET || !data) {
        return -1;
    }

    int sent = send(sock, (const char*)data, (int)len, 0);
    return sent;
}

int doip_socket_tcp_recv(DoIPSocket_t sock, uint8_t* buf, size_t cap, uint32_t timeout_ms) {
    if (sock == DOIP_INVALID_SOCKET || !buf) {
        return -1;
    }

    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    int received = recv(sock, (char*)buf, (int)cap, 0);
    
    if (received < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAETIMEDOUT) {
            return 0;  /* Timeout */
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  /* Timeout */
        }
#endif
        return -1;
    }

    return received;
}

int doip_socket_udp_broadcast(DoIPSocket_t sock, const uint8_t* data, size_t len, uint16_t port) {
    if (sock == DOIP_INVALID_SOCKET || !data) {
        return -1;
    }

    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadcast_addr.sin_port = htons(port);

    int sent = sendto(sock, (const char*)data, (int)len, 0,
                      (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    return sent;
}

int doip_socket_udp_recv(DoIPSocket_t sock, uint8_t* buf, size_t cap, uint32_t timeout_ms, uint32_t* src_ip) {
    if (sock == DOIP_INVALID_SOCKET || !buf) {
        return -1;
    }

    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    int received = recvfrom(sock, (char*)buf, (int)cap, 0,
                            (struct sockaddr*)&src_addr, &addr_len);

    if (received < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAETIMEDOUT) {
            return 0;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
#endif
        return -1;
    }

    if (src_ip) {
        *src_ip = src_addr.sin_addr.s_addr;
    }

    return received;
}

void doip_socket_close(DoIPSocket_t sock) {
    if (sock != DOIP_INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}

int doip_ip_str_to_addr(const char* ip_str, uint32_t* ip_out) {
    if (!ip_str || !ip_out) {
        return -1;
    }

    struct in_addr addr;
    
#ifdef _WIN32
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        return -1;
    }
#else
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        return -1;
    }
#endif

    *ip_out = addr.s_addr;  /* Already in network byte order */
    return 0;
}

