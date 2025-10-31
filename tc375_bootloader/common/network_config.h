/**
 * @file network_config.h
 * @brief TC375 Network Configuration with lwIP
 * 
 * NOTE: TC375 Lite Kit does NOT have built-in Ethernet!
 *       You need external Ethernet shield or module.
 */

#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Network Configuration
// ============================================================================

/* Static IP Configuration */
#define TC375_IP_ADDR_0         192
#define TC375_IP_ADDR_1         168
#define TC375_IP_ADDR_2         1
#define TC375_IP_ADDR_3         10

#define TC375_NETMASK_0         255
#define TC375_NETMASK_1         255
#define TC375_NETMASK_2         255
#define TC375_NETMASK_3         0

#define TC375_GATEWAY_0         192
#define TC375_GATEWAY_1         168
#define TC375_GATEWAY_2         1
#define TC375_GATEWAY_3         1

/* DHCP Configuration */
#define TC375_USE_DHCP          0  /* 0 = Static, 1 = DHCP */

/* MAC Address (customize for each device) */
#define TC375_MAC_ADDR_0        0x00
#define TC375_MAC_ADDR_1        0x03
#define TC375_MAC_ADDR_2        0x19
#define TC375_MAC_ADDR_3        0x45
#define TC375_MAC_ADDR_4        0x00
#define TC375_MAC_ADDR_5        0x01

// ============================================================================
// lwIP Configuration
// ============================================================================

#if defined(USE_LWIP)

#include "lwip/ip_addr.h"
#include "lwip/netif.h"

/**
 * @brief Initialize network interface with static IP
 * 
 * Example usage:
 * ```c
 * struct netif tc375_netif;
 * network_init_static(&tc375_netif);
 * ```
 */
int network_init_static(struct netif* netif);

/**
 * @brief Initialize network interface with DHCP
 */
int network_init_dhcp(struct netif* netif);

/**
 * @brief Get current IP address
 */
void network_get_ip_address(uint8_t ip[4]);

#endif /* USE_LWIP */

// ============================================================================
// Hardware Setup Requirements
// ============================================================================

/*
 * TC375 Lite Kit Ethernet Hardware:
 * 
 * ✅ Built-in Ethernet PHY (CONFIRMED)
 * -------------------------------------
 * - TC375 Lite Kit (KIT_A2G_TC375_LITE) includes Ethernet PHY
 * - RGMII/RMII interface
 * - 10/100 Mbps support
 * - RJ45 connector on board
 * - No additional hardware needed!
 * 
 * Pin Configuration (RGMII):
 *   Port P11.2-7   : TX pins (TXD0-3, TX_EN, TX_CLK)
 *   Port P11.8-13  : RX pins (RXD0-3, RX_DV, RX_CLK)
 * 
 * Software Stack:
 *   - iLLD (Infineon Low-Level Driver) for MAC
 *   - lwIP 2.1.3 for TCP/IP stack
 *   - FreeRTOS (optional, recommended for multitasking)
 * 
 * Setup Guide:
 *   See: AURIX_SETUP_GUIDE.md
 */

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_CONFIG_H */

