/**
 * @file ecu_node.h
 * @brief End Node ECU Implementation for TC375
 * 
 * MCU #2, #4, #6 등 End Node ECU 역할
 * - Zone Gateway에 연결되는 최종 노드
 * - DoIP Client로 동작
 * - UDS 서비스 제공
 */

#ifndef ECU_NODE_H
#define ECU_NODE_H

#include "doip_client.h"
#include "doip_message.h"
#include "uds_handler.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define ECU_MAX_DIAG_BUFFER_SIZE    4096
#define ECU_HEARTBEAT_INTERVAL_MS   10000
#define ECU_VCI_UPDATE_INTERVAL_MS  60000

/**
 * @brief ECU Node 상태
 */
typedef enum {
    ECU_STATE_INIT = 0,
    ECU_STATE_DISCOVERING,      /* ZG 찾는 중 */
    ECU_STATE_CONNECTING,       /* ZG 연결 중 */
    ECU_STATE_READY,            /* 정상 동작 */
    ECU_STATE_OTA_IN_PROGRESS,  /* OTA 진행 중 */
    ECU_STATE_ERROR
} ECUState_t;

/**
 * @brief ECU Node Context
 */
typedef struct {
    /* Identity */
    char ecu_id[32];                    /* ECU ID (e.g., "TC375-ECU-002") */
    uint16_t logical_address;           /* DoIP 논리 주소 (e.g., 0x0201) */
    char firmware_version[16];          /* 현재 펌웨어 버전 */
    char hardware_version[32];          /* 하드웨어 버전 */
    
    /* Zone Gateway Connection */
    char zg_ip[16];                     /* ZG IP address */
    uint16_t zg_port;                   /* ZG DoIP port (13400) */
    DoIPClient_t zg_client;             /* ZG 클라이언트 */
    bool zg_connected;
    
    /* State */
    ECUState_t state;
    
    /* UDS Handler */
    UDSHandler_t uds_handler;
    
    /* Capabilities */
    bool ota_capable;
    bool delta_update_supported;
    uint32_t max_package_size;
    
    /* Timing */
    uint32_t last_heartbeat_time;
    uint32_t last_vci_update_time;
    
    /* Buffers */
    uint8_t rx_buffer[ECU_MAX_DIAG_BUFFER_SIZE];
    uint8_t tx_buffer[ECU_MAX_DIAG_BUFFER_SIZE];
    
} ECUNode_t;

/**
 * @brief Initialize ECU Node
 * 
 * @param ecu ECU Node context
 * @param ecu_id ECU ID string
 * @param logical_addr DoIP logical address
 * @param zg_ip Zone Gateway IP
 * @param zg_port Zone Gateway port
 * @return 0 on success, -1 on error
 */
int ecu_init(ECUNode_t* ecu, const char* ecu_id, uint16_t logical_addr,
             const char* zg_ip, uint16_t zg_port);

/**
 * @brief Start ECU Node
 * 
 * @param ecu ECU Node context
 * @return 0 on success, -1 on error
 */
int ecu_start(ECUNode_t* ecu);

/**
 * @brief Stop ECU Node
 * 
 * @param ecu ECU Node context
 */
void ecu_stop(ECUNode_t* ecu);

/**
 * @brief Main loop (non-blocking)
 * 
 * @param ecu ECU Node context
 */
void ecu_run(ECUNode_t* ecu);

/* ========== Zone Gateway Connection ========== */

/**
 * @brief Discover Zone Gateway (UDP broadcast)
 * 
 * @param ecu ECU Node context
 * @return 0 on success, -1 on error
 */
int ecu_discover_zone_gateway(ECUNode_t* ecu);

/**
 * @brief Connect to Zone Gateway
 * 
 * @param ecu ECU Node context
 * @return 0 on success, -1 on error
 */
int ecu_connect_to_zg(ECUNode_t* ecu);

/**
 * @brief Send heartbeat to Zone Gateway
 * 
 * @param ecu ECU Node context
 * @return 0 on success, -1 on error
 */
int ecu_send_heartbeat(ECUNode_t* ecu);

/**
 * @brief Send VCI info to Zone Gateway
 * 
 * @param ecu ECU Node context
 * @return 0 on success, -1 on error
 */
int ecu_send_vci_info(ECUNode_t* ecu);

/* ========== UDS Services ========== */

/**
 * @brief Handle UDS request from Zone Gateway
 * 
 * @param ecu ECU Node context
 * @param request UDS request data
 * @param req_len Request length
 * @param response Response buffer
 * @param resp_cap Response buffer capacity
 * @param resp_len Response length (output)
 * @return 0 on success, -1 on error
 */
int ecu_handle_uds_request(ECUNode_t* ecu,
                           const uint8_t* request, size_t req_len,
                           uint8_t* response, size_t resp_cap, size_t* resp_len);

/* ========== OTA Functions ========== */

/**
 * @brief Check if ECU is ready for OTA
 * 
 * @param ecu ECU Node context
 * @return true if ready, false otherwise
 */
bool ecu_check_ota_readiness(ECUNode_t* ecu);

/**
 * @brief Receive and write OTA firmware
 * 
 * @param ecu ECU Node context
 * @param firmware_data Firmware data
 * @param firmware_size Firmware size
 * @return 0 on success, -1 on error
 */
int ecu_receive_ota_firmware(ECUNode_t* ecu, const uint8_t* firmware_data, size_t firmware_size);

/**
 * @brief Install OTA firmware (activate new bank)
 * 
 * @param ecu ECU Node context
 * @return 0 on success, -1 on error
 */
int ecu_install_ota_firmware(ECUNode_t* ecu);

/**
 * @brief Report OTA result
 * 
 * @param ecu ECU Node context
 * @param success OTA success status
 * @return 0 on success, -1 on error
 */
int ecu_report_ota_result(ECUNode_t* ecu, bool success);

/* ========== Utility ========== */

/**
 * @brief Print ECU info
 * 
 * @param ecu ECU Node context
 */
void ecu_print_info(const ECUNode_t* ecu);

/**
 * @brief Get current tick (milliseconds)
 * 
 * @return Current tick in ms
 */
uint32_t ecu_get_tick_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* ECU_NODE_H */

