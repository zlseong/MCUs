/**
 * @file tc375_ethernet_init.c
 * @brief TC375 Lite Kit Ethernet Initialization with lwIP
 * 
 * Hardware: TC375 Lite Kit (KIT_A2G_TC375_LITE)
 * Ethernet PHY: Built-in RGMII/RMII PHY
 * IDE: AURIX Development Studio
 */

#include "tc375_ethernet_init.h"
#include "Ifx_Types.h"
#include "IfxEth.h"
#include "IfxPort.h"

/* lwIP includes */
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"

/* Static IP Configuration - Customize as needed */
#define TC375_IP_ADDR           "192.168.1.10"
#define TC375_NETMASK           "255.255.255.0"
#define TC375_GATEWAY           "192.168.1.1"
#define TC375_USE_DHCP          0  /* 0 = Static IP, 1 = DHCP */

/* MAC Address - Customize for each ECU */
#define TC375_MAC_ADDR_0        0x00
#define TC375_MAC_ADDR_1        0x03
#define TC375_MAC_ADDR_2        0x19
#define TC375_MAC_ADDR_3        0x45
#define TC375_MAC_ADDR_4        0x00
#define TC375_MAC_ADDR_5        0x01

/* Network interface */
static struct netif tc375_netif;

/**
 * @brief Low-level Ethernet initialization (iLLD)
 */
static void tc375_eth_hw_init(void) {
    /* Configure Ethernet pins (RGMII/RMII) */
    /* Refer to TC375 User Manual Chapter 33: Ethernet MAC */
    
    /* Example: RGMII Configuration for TC375 Lite Kit */
    IfxPort_setPinMode(&MODULE_P11, 2, IfxPort_Mode_outputPushPullAlt1);  // ETH_TXD0
    IfxPort_setPinMode(&MODULE_P11, 3, IfxPort_Mode_outputPushPullAlt1);  // ETH_TXD1
    IfxPort_setPinMode(&MODULE_P11, 4, IfxPort_Mode_outputPushPullAlt1);  // ETH_TXD2
    IfxPort_setPinMode(&MODULE_P11, 5, IfxPort_Mode_outputPushPullAlt1);  // ETH_TXD3
    IfxPort_setPinMode(&MODULE_P11, 6, IfxPort_Mode_outputPushPullAlt1);  // ETH_TX_EN
    IfxPort_setPinMode(&MODULE_P11, 7, IfxPort_Mode_outputPushPullAlt1);  // ETH_TX_CLK
    
    IfxPort_setPinMode(&MODULE_P11, 10, IfxPort_Mode_inputPullUp);        // ETH_RXD0
    IfxPort_setPinMode(&MODULE_P11, 11, IfxPort_Mode_inputPullUp);        // ETH_RXD1
    IfxPort_setPinMode(&MODULE_P11, 12, IfxPort_Mode_inputPullUp);        // ETH_RXD2
    IfxPort_setPinMode(&MODULE_P11, 13, IfxPort_Mode_inputPullUp);        // ETH_RXD3
    IfxPort_setPinMode(&MODULE_P11, 8,  IfxPort_Mode_inputPullUp);        // ETH_RX_DV
    IfxPort_setPinMode(&MODULE_P11, 9,  IfxPort_Mode_inputPullUp);        // ETH_RX_CLK
    
    /* Initialize Ethernet MAC */
    IfxEth_Config ethConfig;
    IfxEth_initModuleConfig(&ethConfig, &MODULE_ETH);
    
    /* MAC Configuration */
    ethConfig.macAddress[0] = TC375_MAC_ADDR_0;
    ethConfig.macAddress[1] = TC375_MAC_ADDR_1;
    ethConfig.macAddress[2] = TC375_MAC_ADDR_2;
    ethConfig.macAddress[3] = TC375_MAC_ADDR_3;
    ethConfig.macAddress[4] = TC375_MAC_ADDR_4;
    ethConfig.macAddress[5] = TC375_MAC_ADDR_5;
    
    /* Initialize Ethernet module */
    IfxEth_Eth eth;
    IfxEth_init(&eth, &ethConfig);
}

/**
 * @brief lwIP network interface initialization
 */
static err_t tc375_netif_init(struct netif *netif) {
    /* Set MAC hardware address */
    netif->hwaddr_len = 6;
    netif->hwaddr[0] = TC375_MAC_ADDR_0;
    netif->hwaddr[1] = TC375_MAC_ADDR_1;
    netif->hwaddr[2] = TC375_MAC_ADDR_2;
    netif->hwaddr[3] = TC375_MAC_ADDR_3;
    netif->hwaddr[4] = TC375_MAC_ADDR_4;
    netif->hwaddr[5] = TC375_MAC_ADDR_5;
    
    /* Set MTU */
    netif->mtu = 1500;
    
    /* Device capabilities */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    
    /* Set network interface name */
    netif->name[0] = 't';
    netif->name[1] = 'c';
    
    /* Link output function */
    netif->output = etharp_output;
    netif->linkoutput = /* Your low-level output function */;
    
    return ERR_OK;
}

/**
 * @brief Initialize Ethernet with Static IP
 */
int tc375_ethernet_init_static(void) {
    ip4_addr_t ipaddr, netmask, gateway;
    
    /* Parse IP configuration */
    ip4addr_aton(TC375_IP_ADDR, &ipaddr);
    ip4addr_aton(TC375_NETMASK, &netmask);
    ip4addr_aton(TC375_GATEWAY, &gateway);
    
    /* Initialize lwIP */
    lwip_init();
    
    /* Initialize hardware */
    tc375_eth_hw_init();
    
    /* Add network interface */
    netif_add(&tc375_netif, &ipaddr, &netmask, &gateway, NULL, tc375_netif_init, ethernet_input);
    
    /* Set as default interface */
    netif_set_default(&tc375_netif);
    
    /* Bring interface up */
    netif_set_up(&tc375_netif);
    
    /* Link is up */
    netif_set_link_up(&tc375_netif);
    
    return 0;
}

/**
 * @brief Initialize Ethernet with DHCP
 */
int tc375_ethernet_init_dhcp(void) {
    /* Initialize lwIP */
    lwip_init();
    
    /* Initialize hardware */
    tc375_eth_hw_init();
    
    /* Add network interface with DHCP */
    netif_add(&tc375_netif, NULL, NULL, NULL, NULL, tc375_netif_init, ethernet_input);
    
    /* Set as default interface */
    netif_set_default(&tc375_netif);
    
    /* Bring interface up */
    netif_set_up(&tc375_netif);
    netif_set_link_up(&tc375_netif);
    
    /* Start DHCP */
    dhcp_start(&tc375_netif);
    
    return 0;
}

/**
 * @brief Get current IP address
 */
void tc375_ethernet_get_ip(uint8_t ip[4]) {
    const ip4_addr_t* addr = netif_ip4_addr(&tc375_netif);
    ip[0] = ip4_addr1(addr);
    ip[1] = ip4_addr2(addr);
    ip[2] = ip4_addr3(addr);
    ip[3] = ip4_addr4(addr);
}

/**
 * @brief Print network configuration
 */
void tc375_ethernet_print_config(void) {
    uint8_t ip[4];
    tc375_ethernet_get_ip(ip);
    
    printf("[Ethernet] MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           TC375_MAC_ADDR_0, TC375_MAC_ADDR_1, TC375_MAC_ADDR_2,
           TC375_MAC_ADDR_3, TC375_MAC_ADDR_4, TC375_MAC_ADDR_5);
    
    printf("[Ethernet] IP:  %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    
    if (netif_is_link_up(&tc375_netif)) {
        printf("[Ethernet] Link: UP\n");
    } else {
        printf("[Ethernet] Link: DOWN\n");
    }
}

/**
 * @brief Periodic lwIP timers (call in main loop or RTOS task)
 */
void tc375_ethernet_process(void) {
    sys_check_timeouts();
}

