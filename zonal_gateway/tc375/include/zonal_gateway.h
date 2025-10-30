/**
 * @file zonal_gateway.h
 * @brief Zonal Gateway Implementation for TC375
 * 
 * MCU #1, #3, #5 등 Zonal Gateway 역할
 * - Downstream: Zone 내 ECU들의 서버 (DoIP Server)
 * - Upstream: VMG의 클라이언트 (DoIP Client)
 */

#ifndef ZONAL_GATEWAY_H
#define ZONAL_GATEWAY_H

#include "doip_client.h"
#include "doip_message.h"
#include "uds_handler.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define ZG_MAX_ECUS             8       /* Zone당 최대 ECU 수 */
#define ZG_MAX_VCI_SIZE         4096    /* VCI 데이터 최대 크기 */
#define ZG_DOIP_SERVER_PORT     13400   /* Zone 내부 DoIP 포트 */
#define ZG_JSON_SERVER_PORT     8765    /* Zone 내부 JSON 포트 */

/**
 * @brief Zone 내 ECU 정보
 */
typedef struct {
    char ecu_id[32];                /* ECU ID (e.g., "TC375-SIM-002") */
    uint16_t logical_address;       /* DoIP 논리 주소 */
    char firmware_version[16];
    char hardware_version[32];
    bool is_online;
    uint32_t last_heartbeat_time;
    
    /* Capabilities */
    bool ota_capable;
    bool delta_update_supported;
    uint32_t max_package_size;
} ZoneECUInfo_t;

/**
 * @brief Zone VCI 집계 데이터
 */
typedef struct {
    uint8_t zone_id;
    uint8_t ecu_count;
    ZoneECUInfo_t ecus[ZG_MAX_ECUS];
    
    /* Zone 통계 */
    uint32_t total_storage_mb;
    uint32_t available_storage_mb;
    uint8_t average_battery_level;
    
} ZoneVCIData_t;

/**
 * @brief Zonal Gateway 상태
 */
typedef enum {
    ZG_STATE_INIT = 0,
    ZG_STATE_DISCOVERING,       /* Zone 내 ECU Discovery */
    ZG_STATE_CONNECTING_VMG,    /* VMG 연결 중 */
    ZG_STATE_READY,             /* 정상 동작 */
    ZG_STATE_OTA_IN_PROGRESS,   /* OTA 진행 중 */
    ZG_STATE_ERROR
} ZGState_t;

/**
 * @brief Zonal Gateway Context
 */
typedef struct {
    /* Identity */
    uint8_t zone_id;            /* Zone ID (1, 2, 3...) */
    char zg_id[32];             /* "ZG-001" */
    uint16_t logical_address;   /* DoIP 논리 주소 (e.g., 0x0200) */
    
    /* State */
    ZGState_t state;
    
    /* ========== Server 역할 (Zone 내부) ========== */
    int doip_server_tcp_socket;     /* ECU들 연결 대기 */
    int doip_server_udp_socket;     /* Vehicle Discovery */
    int json_server_socket;         /* JSON 메시지 */
    
    /* Zone 내 ECU 관리 */
    ZoneVCIData_t zone_vci;
    
    /* UDS Handler (ECU 요청 처리용) */
    UDSHandler_t uds_handler;
    
    /* ========== Client 역할 (VMG 연결) ========== */
    DoIPClient_t vmg_client;        /* VMG 클라이언트 */
    bool vmg_connected;
    
    /* Buffers */
    uint8_t server_rx_buffer[4096];
    uint8_t server_tx_buffer[4096];
    
} ZonalGateway_t;

/**
 * @brief Initialize Zonal Gateway
 * 
 * @param zg Zonal Gateway context
 * @param zone_id Zone ID (1, 2, 3...)
 * @param vmg_ip VMG IP address (e.g., "192.168.1.1")
 * @param vmg_port VMG DoIP port (default: 13400)
 * @return 0 on success, -1 on error
 */
int zg_init(ZonalGateway_t* zg, uint8_t zone_id, const char* vmg_ip, uint16_t vmg_port);

/**
 * @brief Start Zonal Gateway
 * 
 * @param zg Zonal Gateway context
 * @return 0 on success, -1 on error
 */
int zg_start(ZonalGateway_t* zg);

/**
 * @brief Stop Zonal Gateway
 * 
 * @param zg Zonal Gateway context
 */
void zg_stop(ZonalGateway_t* zg);

/**
 * @brief Main loop (non-blocking)
 * 
 * @param zg Zonal Gateway context
 */
void zg_run(ZonalGateway_t* zg);

/* ========== Server Functions (Zone 내부) ========== */

/**
 * @brief Handle DoIP message from Zone ECU
 * 
 * @param zg Zonal Gateway context
 * @param client_socket ECU socket
 * @return 0 on success, -1 on error
 */
int zg_handle_ecu_doip_message(ZonalGateway_t* zg, int client_socket);

/**
 * @brief Handle JSON message from Zone ECU
 * 
 * @param zg Zonal Gateway context
 * @param client_socket ECU socket
 * @return 0 on success, -1 on error
 */
int zg_handle_ecu_json_message(ZonalGateway_t* zg, int client_socket);

/**
 * @brief Handle Vehicle Discovery (UDP)
 * 
 * @param zg Zonal Gateway context
 * @return 0 on success, -1 on error
 */
int zg_handle_vehicle_discovery(ZonalGateway_t* zg);

/* ========== Client Functions (VMG 연결) ========== */

/**
 * @brief Connect to VMG
 * 
 * @param zg Zonal Gateway context
 * @return 0 on success, -1 on error
 */
int zg_connect_to_vmg(ZonalGateway_t* zg);

/**
 * @brief Send Zone VCI to VMG
 * 
 * @param zg Zonal Gateway context
 * @return 0 on success, -1 on error
 */
int zg_send_zone_vci_to_vmg(ZonalGateway_t* zg);

/**
 * @brief Send heartbeat to VMG
 * 
 * @param zg Zonal Gateway context
 * @return 0 on success, -1 on error
 */
int zg_send_heartbeat_to_vmg(ZonalGateway_t* zg);

/**
 * @brief Send zone status to VMG
 * 
 * @param zg Zonal Gateway context
 * @return 0 on success, -1 on error
 */
int zg_send_zone_status_to_vmg(ZonalGateway_t* zg);

/* ========== VCI Collection ========== */

/**
 * @brief Collect VCI from all Zone ECUs
 * 
 * @param zg Zonal Gateway context
 * @return 0 on success, -1 on error
 */
int zg_collect_zone_vci(ZonalGateway_t* zg);

/**
 * @brief Request VCI from specific ECU
 * 
 * @param zg Zonal Gateway context
 * @param ecu_index ECU index in zone_vci.ecus[]
 * @return 0 on success, -1 on error
 */
int zg_request_ecu_vci(ZonalGateway_t* zg, uint8_t ecu_index);

/**
 * @brief Update ECU info in zone
 * 
 * @param zg Zonal Gateway context
 * @param ecu_id ECU ID
 * @param info ECU info
 * @return 0 on success, -1 on error
 */
int zg_update_ecu_info(ZonalGateway_t* zg, const char* ecu_id, const ZoneECUInfo_t* info);

/* ========== OTA Coordination ========== */

/**
 * @brief Check zone readiness for OTA
 * 
 * @param zg Zonal Gateway context
 * @param campaign_id OTA campaign ID
 * @return true if ready, false otherwise
 */
bool zg_check_ota_readiness(ZonalGateway_t* zg, const char* campaign_id);

/**
 * @brief Distribute OTA update to Zone ECUs
 * 
 * @param zg Zonal Gateway context
 * @param package_data Firmware package
 * @param package_size Package size
 * @return 0 on success, -1 on error
 */
int zg_distribute_ota_to_zone(ZonalGateway_t* zg, const uint8_t* package_data, size_t package_size);

/**
 * @brief Report OTA progress to VMG
 * 
 * @param zg Zonal Gateway context
 * @param progress_percentage Progress (0-100)
 * @return 0 on success, -1 on error
 */
int zg_report_ota_progress(ZonalGateway_t* zg, uint8_t progress_percentage);

/* ========== Utility ========== */

/**
 * @brief Get Zone ID string
 * 
 * @param zone_id Zone ID
 * @return Zone ID string (e.g., "Zone_1")
 */
const char* zg_get_zone_name(uint8_t zone_id);

/**
 * @brief Print Zone VCI summary
 * 
 * @param zg Zonal Gateway context
 */
void zg_print_zone_vci(const ZonalGateway_t* zg);

#ifdef __cplusplus
}
#endif

#endif /* ZONAL_GATEWAY_H */

