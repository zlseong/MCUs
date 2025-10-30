/**
 * @file zonal_gateway_linux.hpp
 * @brief Zonal Gateway Implementation for Linux (x86)
 * 
 * 노트북/PC 환경에서 Zonal Gateway 역할 시뮬레이션
 * - Downstream: Zone 내 ECU들의 서버 (DoIP Server)
 * - Upstream: VMG의 클라이언트 (DoIP Client)
 */

#ifndef ZONAL_GATEWAY_LINUX_HPP
#define ZONAL_GATEWAY_LINUX_HPP

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <cstdint>
#include <atomic>

namespace vmg {

/* Configuration */
constexpr uint8_t ZG_MAX_ECUS = 8;
constexpr uint16_t ZG_DOIP_SERVER_PORT = 13400;
constexpr uint16_t ZG_JSON_SERVER_PORT = 8765;

/**
 * @brief Zone 내 ECU 정보
 */
struct ZoneECUInfo {
    std::string ecu_id;                 /* ECU ID */
    uint16_t logical_address;           /* DoIP 논리 주소 */
    std::string firmware_version;
    std::string hardware_version;
    bool is_online;
    uint64_t last_heartbeat_time;
    
    /* Capabilities */
    bool ota_capable;
    bool delta_update_supported;
    uint32_t max_package_size;
};

/**
 * @brief Zone VCI 집계 데이터
 */
struct ZoneVCIData {
    uint8_t zone_id;
    std::vector<ZoneECUInfo> ecus;
    
    /* Zone 통계 */
    uint32_t total_storage_mb;
    uint32_t available_storage_mb;
    uint8_t average_battery_level;
};

/**
 * @brief Zonal Gateway 상태
 */
enum class ZGState {
    INIT = 0,
    DISCOVERING,        /* Zone 내 ECU Discovery */
    CONNECTING_VMG,     /* VMG 연결 중 */
    READY,              /* 정상 동작 */
    OTA_IN_PROGRESS,    /* OTA 진행 중 */
    ERROR
};

/**
 * @brief Zonal Gateway Linux Implementation
 */
class ZonalGatewayLinux {
public:
    ZonalGatewayLinux(uint8_t zone_id, const std::string& vmg_ip, uint16_t vmg_port);
    ~ZonalGatewayLinux();
    
    /* Lifecycle */
    bool start();
    void stop();
    void run();
    
    /* Server Functions (Zone 내부) */
    void handleECUConnections();
    void handleVehicleDiscovery();
    
    /* Client Functions (VMG 연결) */
    bool connectToVMG();
    bool sendZoneVCIToVMG();
    bool sendHeartbeatToVMG();
    bool sendZoneStatusToVMG();
    
    /* VCI Collection */
    bool collectZoneVCI();
    bool requestECUVCI(size_t ecu_index);
    bool updateECUInfo(const std::string& ecu_id, const ZoneECUInfo& info);
    
    /* OTA Coordination */
    bool checkOTAReadiness(const std::string& campaign_id);
    bool distributeOTAToZone(const std::vector<uint8_t>& package_data);
    bool reportOTAProgress(uint8_t progress_percentage);
    
    /* Utility */
    std::string getZoneName() const;
    void printZoneVCI() const;
    
    /* Getters */
    uint8_t getZoneID() const { return zone_id_; }
    ZGState getState() const { return state_; }
    const ZoneVCIData& getZoneVCI() const { return zone_vci_; }
    
private:
    /* Identity */
    uint8_t zone_id_;
    std::string zg_id_;
    uint16_t logical_address_;
    
    /* VMG Connection */
    std::string vmg_ip_;
    uint16_t vmg_port_;
    
    /* State */
    std::atomic<ZGState> state_;
    std::atomic<bool> running_;
    std::atomic<bool> vmg_connected_;
    
    /* Data */
    ZoneVCIData zone_vci_;
    mutable std::mutex zone_vci_mutex_;
    
    /* Network */
    int doip_server_tcp_socket_;
    int doip_server_udp_socket_;
    int json_server_socket_;
    int vmg_client_socket_;
    
    /* Threads */
    std::unique_ptr<std::thread> server_thread_;
    std::unique_ptr<std::thread> client_thread_;
    std::unique_ptr<std::thread> discovery_thread_;
    
    /* Private methods */
    void serverThreadFunc();
    void clientThreadFunc();
    void discoveryThreadFunc();
    
    bool createServerSockets();
    void closeServerSockets();
    
    bool sendDoIPMessage(int socket, uint16_t payload_type, 
                         const std::vector<uint8_t>& payload);
    bool receiveDoIPMessage(int socket, uint16_t& payload_type,
                            std::vector<uint8_t>& payload);
};

} // namespace vmg

#endif /* ZONAL_GATEWAY_LINUX_HPP */

