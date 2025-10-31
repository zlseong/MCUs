/**
 * @file tc375_ethernet_init.h
 * @brief TC375 Lite Kit Ethernet Initialization Header
 */

#ifndef TC375_ETHERNET_INIT_H
#define TC375_ETHERNET_INIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Ethernet with Static IP
 * 
 * Configuration in tc375_ethernet_init.c:
 *   - IP: 192.168.1.10 (customize as needed)
 *   - Netmask: 255.255.255.0
 *   - Gateway: 192.168.1.1
 * 
 * @return 0 on success, -1 on error
 */
int tc375_ethernet_init_static(void);

/**
 * @brief Initialize Ethernet with DHCP
 * 
 * DHCP will automatically assign IP address
 * 
 * @return 0 on success, -1 on error
 */
int tc375_ethernet_init_dhcp(void);

/**
 * @brief Get current IP address
 * 
 * @param ip Output buffer for IP address (4 bytes)
 */
void tc375_ethernet_get_ip(uint8_t ip[4]);

/**
 * @brief Print network configuration to console
 */
void tc375_ethernet_print_config(void);

/**
 * @brief Process lwIP timers (call periodically)
 * 
 * Call this in main loop or RTOS task every 10-100ms
 */
void tc375_ethernet_process(void);

#ifdef __cplusplus
}
#endif

#endif /* TC375_ETHERNET_INIT_H */

