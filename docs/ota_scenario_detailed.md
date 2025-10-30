# OTA 시나리오 상세 (사용자 제공)

4단계 계층적 OTA 업데이트 시나리오: Server → VMG → ZG → ECU

## 📊 전체 아키텍처

```
┌─────────┐   ┌────────┐   ┌─────┐   ┌─────┐   ┌──────┐
│ Server  │◄─►│ Driver │◄─►│ VMG │◄─►│ ZG  │◄─►│ ECU  │
└─────────┘   └────────┘   └─────┘   └─────┘   └──────┘
   Cloud       사용자       CCU      Zone    End Node
```

---

## 📦 Phase 1: OTA 패키지 전송

### 다이어그램
```
Server          VMG             ZG              ECU
  │              │               │               │
  │─Transfer────►│               │               │
  │ global OTA   │               │               │
  │ packages     │               │               │
  │              │               │               │
  │              │─Transfer─────►│               │
  │              │ zonal OTA     │               │
  │              │ packages      │               │
  │              │               │               │
  │              │               │─Transfer─────►│
  │              │               │ binary        │
  │              │               │ images        │
  │              │               │               │
  │              │               │               │──┐
  │              │               │               │  │ Write new
  │              │               │               │  │ binary in
  │              │               │               │  │ inactive
  │              │               │               │◄─┘ bank memory
```

### 상세 흐름

#### 1.1 Server → VMG: Global OTA Package

**프로토콜:** HTTPS  
**메시지:**
```json
{
  "message_type": "OTA_PACKAGE_AVAILABLE",
  "campaign_id": "OTA-2025-001",
  "package_url": "https://cdn.ota.com/packages/global_v2.0.0.tar.gz",
  "package_size": 104857600,
  "checksum": "sha256:1a2b3c4d...",
  "zones": [
    {
      "zone_id": 1,
      "zone_package_url": "https://cdn.ota.com/packages/zone1_v2.0.0.bin",
      "target_ecus": ["ECU-002", "ECU-003"]
    },
    {
      "zone_id": 2,
      "zone_package_url": "https://cdn.ota.com/packages/zone2_v2.0.0.bin",
      "target_ecus": ["ECU-006", "ECU-007"]
    }
  ]
}
```

**VMG 동작:**
```cpp
// VMG: Download global package
https_download("https://cdn.ota.com/packages/global_v2.0.0.tar.gz", 
               "/tmp/ota/global_v2.0.0.tar.gz");

// Extract zone packages
tar_extract("/tmp/ota/global_v2.0.0.tar.gz", "/tmp/ota/zones/");
// → /tmp/ota/zones/zone1_v2.0.0.bin
// → /tmp/ota/zones/zone2_v2.0.0.bin
```

#### 1.2 VMG → ZG: Zonal OTA Package

**프로토콜:** TCP (DoIP 또는 HTTP)  
**메시지:**
```json
{
  "message_type": "ZONE_OTA_PACKAGE",
  "campaign_id": "OTA-2025-001",
  "zone_id": 1,
  "package_size": 20971520,
  "target_ecus": ["ECU-002", "ECU-003"],
  "checksum": "sha256:5e6f7g8h..."
}
```

**전송:**
```cpp
// VMG → ZG: Send zone package via TCP
int zg_socket = connect_to_zg("192.168.1.10", 8765);
send_file(zg_socket, "/tmp/ota/zones/zone1_v2.0.0.bin");
```

**ZG 수신:**
```c
// ZG: Receive and verify
receive_file(vmg_socket, "/tmp/zone_ota.bin");
if (verify_checksum("/tmp/zone_ota.bin", "sha256:5e6f7g8h...") != 0) {
    send_error("Checksum mismatch");
    return -1;
}

// Extract ECU binaries
extract_ecu_binaries("/tmp/zone_ota.bin", "/tmp/ecu_images/");
// → /tmp/ecu_images/ECU-002.bin
// → /tmp/ecu_images/ECU-003.bin
```

#### 1.3 ZG → ECU: Binary Images

**프로토콜:** DoIP (UDS)  
**UDS 서비스:**
- 0x34: Request Download
- 0x36: Transfer Data
- 0x37: Request Transfer Exit

**흐름:**
```c
// ZG → ECU: Start download
uint8_t req_download[] = {
    0x34,           // SID
    0x00,           // dataFormatIdentifier
    0x44,           // addressAndLengthFormatIdentifier
    0x82, 0x00, 0x00, 0x00,  // Address: 0x82000000 (Region B)
    0x01, 0x40, 0x00, 0x00   // Length: 20MB
};
doip_send_diagnostic(ecu_socket, req_download, sizeof(req_download));

// Transfer blocks
FILE* fp = fopen("/tmp/ecu_images/ECU-002.bin", "rb");
uint8_t buffer[1024];
uint8_t seq = 1;

while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
    uint8_t transfer_data[1026];
    transfer_data[0] = 0x36;  // SID
    transfer_data[1] = seq++;  // Sequence
    memcpy(&transfer_data[2], buffer, bytes_read);
    
    doip_send_diagnostic(ecu_socket, transfer_data, 2 + bytes_read);
    
    // Receive ACK
    doip_receive_response(ecu_socket, response, &resp_len);
}

// Finish
uint8_t req_exit[] = {0x37};
doip_send_diagnostic(ecu_socket, req_exit, sizeof(req_exit));
```

**ECU 동작:**
```c
// ECU: Write to Inactive Bank (Region B)
void handle_transfer_data(uint8_t* data, size_t len) {
    uint8_t seq = data[1];
    uint8_t* payload = &data[2];
    size_t payload_len = len - 2;
    
    // Write to flash (Region B: 0x82000000)
    uint32_t flash_addr = 0x82000000 + (seq - 1) * 1024;
    flash_write(flash_addr, payload, payload_len);
    
    // Send positive response
    uint8_t response[] = {0x76, seq};  // 0x76 = 0x36 + 0x40
    send_response(response, sizeof(response));
}
```

---

## 🔍 Phase 2: VCI 수집 및 Ready 확인

### 다이어그램
```
Server          VMG             ZG              ECU
  │              │               │               │
  │─Request─────►│               │               │
  │ VCI          │               │               │
  │              │               │               │
  │              │─Request──────►│               │
  │              │ Attribute     │               │
  │              │ information   │               │
  │              │               │               │
  │              │               │─Request──────►│
  │              │               │ Attribute     │
  │              │               │ information   │
  │              │               │               │
  │              │               │               │◄─Send
  │              │               │◄─Send         │  Attribute
  │              │◄─Send         │  Attribute    │  of ECU
  │◄─Create VCI──┤  Attribute    │  of ECUs      │
  │ with attr    │  of ECUs      │  in zone      │
  │ info and send│  in zone      │               │
  │              │               │               │
  │─Request─────►│               │               │
  │ Ready        │               │               │
  │ response     │               │               │
  │              │               │               │
  │              │               │──┐            │
  │              │◄─────────────────┘            │
  │◄─Send Ready──┤  Check vehicle conditions     │
  │ response     │  for update readiness         │
```

### 상세 흐름

#### 2.1 Server → VMG: Request VCI

```json
{
  "message_type": "REQUEST_VCI",
  "message_id": "req-001",
  "vin": "KMHGH4JH1NU123456",
  "include_sections": ["hardware", "software", "configuration", "diagnostics"]
}
```

#### 2.2 VMG → ZG: Request Attribute

**DoIP 진단 메시지:**
```c
// VMG → ZG: Request zone attributes
uint8_t uds_req[] = {0x22, 0xF1, 0x90};  // Read VIN
doip_send_to_zg(zg_socket, uds_req, sizeof(uds_req));

uint8_t uds_req2[] = {0x22, 0xF1, 0x95};  // Read SW version
doip_send_to_zg(zg_socket, uds_req2, sizeof(uds_req2));
```

#### 2.3 ZG → ECU: Request Attribute

```c
// ZG → ECU: Forward requests
for (int i = 0; i < zone_vci.ecu_count; i++) {
    int ecu_socket = zone_vci.ecus[i].socket;
    
    // Read VIN
    uint8_t uds_req[] = {0x22, 0xF1, 0x90};
    doip_send_diagnostic(ecu_socket, uds_req, sizeof(uds_req));
    
    uint8_t response[256];
    size_t resp_len;
    doip_receive_response(ecu_socket, response, &resp_len);
    
    // Parse VIN
    if (response[0] == 0x62 && response[1] == 0xF1 && response[2] == 0x90) {
        memcpy(zone_vci.ecus[i].vin, &response[3], 17);
    }
    
    // Read SW version
    uint8_t uds_req2[] = {0x22, 0xF1, 0x95};
    doip_send_diagnostic(ecu_socket, uds_req2, sizeof(uds_req2));
    doip_receive_response(ecu_socket, response, &resp_len);
    
    // Parse SW version
    if (response[0] == 0x62) {
        strcpy(zone_vci.ecus[i].firmware_version, (char*)&response[3]);
    }
}
```

#### 2.4 ECU → ZG → VMG: Send Attributes

**ECU 응답:**
```c
// ECU: UDS positive response
uint8_t response[] = {
    0x62,           // Positive response (0x22 + 0x40)
    0xF1, 0x90,     // DID
    'K','M','H','G','H','4','J','H','1','N','U','1','2','3','4','5','6'  // VIN
};
send_response(response, sizeof(response));
```

**ZG 집계:**
```c
// ZG: Aggregate zone VCI
ZoneVCIData_t zone_vci = {
    .zone_id = 1,
    .ecu_count = 2,
    .ecus = {
        {
            .ecu_id = "ECU-002",
            .vin = "KMHGH4JH1NU123456",
            .firmware_version = "1.0.0",
            .hardware_version = "HW_v2.0",
            .is_online = true
        },
        {
            .ecu_id = "ECU-003",
            .firmware_version = "1.0.0",
            .is_online = true
        }
    }
};

// Send to VMG
send_zone_vci_to_vmg(&zone_vci);
```

**VMG 통합:**
```cpp
// VMG: Create vehicle VCI
json vehicle_vci = {
    {"vin", "KMHGH4JH1NU123456"},
    {"zones", json::array({
        {
            {"zone_id", 1},
            {"ecus", json::array({
                {{"ecu_id", "ECU-002"}, {"firmware_version", "1.0.0"}},
                {{"ecu_id", "ECU-003"}, {"firmware_version", "1.0.0"}}
            })}
        },
        {
            {"zone_id", 2},
            {"ecus", [...]}
        }
    })}
};

// Send to Server (MQTT)
mqtt_publish(mqtt_client, "v2x/vmg/{vin}/vci", vehicle_vci.dump());
```

#### 2.5 Server → VMG: Request Ready Response

```json
{
  "message_type": "REQUEST_READINESS",
  "campaign_id": "OTA-2025-001",
  "prerequisites": {
    "minimum_battery_level": 50,
    "vehicle_state": "parked",
    "ignition_state": "off_or_acc"
  }
}
```

#### 2.6 VMG: Check Conditions

```cpp
// VMG: Check vehicle readiness
bool check_ota_readiness() {
    // Check battery
    uint8_t battery_level = get_battery_level();
    if (battery_level < 50) {
        return false;
    }
    
    // Check vehicle state
    VehicleState state = get_vehicle_state();
    if (state != PARKED) {
        return false;
    }
    
    // Check ignition
    IgnitionState ignition = get_ignition_state();
    if (ignition == ON) {
        return false;
    }
    
    // Check all zones
    for (auto& zg : zonal_gateways) {
        if (!zg.is_ready()) {
            return false;
        }
    }
    
    return true;
}
```

#### 2.7 VMG → Server: Send Ready Response

```json
{
  "message_type": "READINESS_RESPONSE",
  "campaign_id": "OTA-2025-001",
  "readiness_status": "ready",
  "checks": {
    "battery_level": 85,
    "vehicle_state": "parked",
    "ignition_state": "acc",
    "all_zones_ready": true
  }
}
```

---

## 👤 Phase 3: 활성화 (Driver 승인)

### 다이어그램
```
Driver          VMG             ZG              ECU
  │              │               │               │
  │─Allow───────►│               │               │
  │ activation   │               │               │
  │              │               │               │
  │              │─Forward──────►│               │
  │              │ activation    │               │
  │              │               │               │
  │              │               │─Install──────►│
  │              │               │ software      │
  │              │               │ update        │
  │              │               │               │
  │              │               │               │──┐
  │              │               │               │  │ Switch
  │              │               │               │  │ inactive bank
  │              │               │               │  │ to active on
  │              │               │               │◄─┘ reboot
```

### 상세 흐름

#### 3.1 Driver → VMG: Allow Activation

**HMI (Human Machine Interface):**
```
┌────────────────────────────────────┐
│   소프트웨어 업데이트 준비 완료     │
├────────────────────────────────────┤
│                                    │
│  업데이트 크기: 20 MB              │
│  예상 시간: 10분                   │
│                                    │
│  업데이트 내용:                     │
│  - Engine ECU v1.0.0 → v1.1.0     │
│  - Transmission ECU v1.0.0→v1.1.0 │
│                                    │
│  [ 나중에 ]    [ 지금 설치 ]       │
└────────────────────────────────────┘
```

**Driver 선택:**
```cpp
// Driver UI Event
if (driver_click_install_now()) {
    json activation_msg = {
        {"message_type", "DRIVER_ACTIVATION"},
        {"campaign_id", "OTA-2025-001"},
        {"user_consent", true},
        {"timestamp", get_current_timestamp()}
    };
    
    send_to_vmg(activation_msg);
}
```

#### 3.2 VMG → ZG: Forward Activation

```cpp
// VMG: Forward activation to zones
for (auto& zg : zonal_gateways) {
    json zone_activation = {
        {"message_type", "ZONE_ACTIVATION"},
        {"campaign_id", "OTA-2025-001"},
        {"zone_id", zg.zone_id},
        {"approved_by_driver", true}
    };
    
    send_to_zg(zg.socket, zone_activation);
}
```

#### 3.3 ZG → ECU: Install Software Update

**UDS 명령:**
```c
// ZG → ECU: Install command
uint8_t uds_install[] = {
    0x31,       // Routine Control
    0x01,       // Start Routine
    0xFF, 0x00  // Routine ID: Install Update
};

doip_send_diagnostic(ecu_socket, uds_install, sizeof(uds_install));

// ECU response
uint8_t response[256];
size_t resp_len;
doip_receive_response(ecu_socket, response, &resp_len);

if (response[0] == 0x71 && response[1] == 0x01) {
    printf("[ZG] ECU installation started\n");
}
```

#### 3.4 ECU: Switch Bank on Reboot

**ECU 내부:**
```c
// ECU: Install routine handler
void uds_routine_control_install(void) {
    // 1. Verify inactive bank
    if (verify_inactive_bank_firmware() != 0) {
        send_negative_response(0x31, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }
    
    // 2. Set boot flag to Region B
    set_boot_region(REGION_B);
    
    // 3. Send positive response
    uint8_t response[] = {0x71, 0x01, 0xFF, 0x00};
    send_response(response, sizeof(response));
    
    // 4. Trigger reboot
    delay_ms(100);
    system_reset();
}

// Bootloader (SSW)
void bootloader_main(void) {
    BootRegion_t boot_region = get_boot_region_flag();
    
    if (boot_region == REGION_B) {
        // Boot from Region B (new firmware)
        jump_to_application(0x82000000);
    } else {
        // Boot from Region A (current firmware)
        jump_to_application(0x80000000);
    }
}
```

---

## 📊 Phase 4: 결과 보고

### 다이어그램
```
Server  Driver      VMG             ZG              ECU
  │       │          │               │               │
  │       │          │               │               │──┐
  │       │          │               │               │  │ Execute new
  │       │          │               │               │  │ software and
  │       │          │               │◄──────────────┤  │ send result
  │       │          │               │  Forward      │◄─┘
  │       │          │◄──────────────┤  results      │
  │       │◄─────────┤  Create       │               │
  │       │  report  │  report and   │               │
  │       │  and     │  notify it    │               │
  │       │  notify  │               │               │
  │◄──────┴──────────┤               │               │
  │  Send report of  │               │               │
  │  software update │               │               │
  │                  │               │               │──┐
  │                  │               │               │  │ Update ECU's
  │                  │               │               │  │ inactive bank
  │                  │               │               │  │ with new
  │                  │               │               │◄─┘ software
```

### 상세 흐름

#### 4.1 ECU: Execute New Software

**새 펌웨어 시작:**
```c
// New firmware (Region B: 0x82000000)
void application_main(void) {
    // 1. Initialize
    init_system();
    
    // 2. Verify successful boot
    if (get_boot_count() == 1) {
        // First boot after OTA
        set_firmware_status(FW_STATUS_TESTING);
        
        // Run self-tests
        if (self_test() == 0) {
            set_firmware_status(FW_STATUS_VALID);
            set_boot_region_permanent(REGION_B);
        } else {
            set_firmware_status(FW_STATUS_INVALID);
            // Will rollback on next reboot
        }
    }
    
    // 3. Send result
    send_ota_result_to_zg();
    
    // 4. Update inactive bank (Region A)
    update_inactive_bank_background();
    
    // 5. Normal operation
    main_loop();
}
```

#### 4.2 ECU → ZG: Send Result

```c
// ECU → ZG: OTA result
json ecu_result = {
    {"ecu_id", "ECU-002"},
    {"status", "success"},
    {"previous_version", "1.0.0"},
    {"current_version", "1.1.0"},
    {"boot_count", 1},
    {"self_test_result", "passed"},
    {"active_bank", "B"},
    {"timestamp", get_current_timestamp()}
};

send_json_to_zg(zg_socket, ecu_result);
```

#### 4.3 ZG → VMG: Forward Results

```c
// ZG: Aggregate zone results
json zone_result = {
    {"zone_id", 1},
    {"overall_status", "success"},
    {"ecus", json::array({
        {
            {"ecu_id", "ECU-002"},
            {"status", "success"},
            {"version", "1.1.0"}
        },
        {
            {"ecu_id", "ECU-003"},
            {"status", "success"},
            {"version", "1.1.0"}
        }
    })}
};

send_to_vmg(vmg_socket, zone_result);
```

#### 4.4 VMG → Driver: Notify

**HMI 알림:**
```
┌────────────────────────────────────┐
│   ✅ 소프트웨어 업데이트 완료       │
├────────────────────────────────────┤
│                                    │
│  성공적으로 업데이트되었습니다.     │
│                                    │
│  업데이트된 ECU:                    │
│  ✓ Engine ECU v1.1.0              │
│  ✓ Transmission ECU v1.1.0        │
│                                    │
│  [ 확인 ]                          │
└────────────────────────────────────┘
```

#### 4.5 VMG → Server: Send Report

```json
{
  "message_type": "OTA_UPDATE_RESULT",
  "campaign_id": "OTA-2025-001",
  "vin": "KMHGH4JH1NU123456",
  "overall_status": "success",
  "completion_time": "2025-10-30T16:00:00Z",
  "zones": [
    {
      "zone_id": 1,
      "status": "success",
      "ecus": [
        {
          "ecu_id": "ECU-002",
          "status": "success",
          "previous_version": "1.0.0",
          "current_version": "1.1.0"
        }
      ]
    }
  ]
}
```

#### 4.6 ECU: Update Inactive Bank (Background)

```c
// ECU: Background task
void update_inactive_bank_background(void) {
    // Copy current firmware (Region B) to inactive bank (Region A)
    flash_erase_region(REGION_A);
    
    uint32_t src_addr = 0x82000000;
    uint32_t dst_addr = 0x80000000;
    uint32_t size = get_firmware_size();
    
    for (uint32_t offset = 0; offset < size; offset += 1024) {
        uint8_t buffer[1024];
        memcpy(buffer, (void*)(src_addr + offset), 1024);
        flash_write(dst_addr + offset, buffer, 1024);
    }
    
    // Now both banks have v1.1.0
    set_inactive_bank_version("1.1.0");
}
```

---

## 🔑 핵심 포인트

### 1. **계층적 분배**
```
Global Package (Server)
  ├─ Zone 1 Package (VMG → ZG1)
  │   ├─ ECU-002 Binary
  │   └─ ECU-003 Binary
  └─ Zone 2 Package (VMG → ZG2)
      ├─ ECU-006 Binary
      └─ ECU-007 Binary
```

### 2. **Driver 승인 필수**
- Phase 3에서 사용자 동의 필요
- HMI를 통한 명시적 승인
- 승인 없이는 설치 불가

### 3. **Dual Bank 전략**
```
Before OTA:
  Region A (Active):    v1.0.0 ← 현재 실행
  Region B (Inactive):  v1.0.0

Download Phase:
  Region A (Active):    v1.0.0 ← 계속 실행
  Region B (Inactive):  v1.1.0 ← 쓰기 중

Activation Phase:
  Region A (Inactive):  v1.0.0
  Region B (Active):    v1.1.0 ← 새 펌웨어 실행

Background Update:
  Region A (Inactive):  v1.1.0 ← 백그라운드 복사
  Region B (Active):    v1.1.0
```

### 4. **Rollback 지원**
```c
if (self_test_failed || boot_count > MAX_RETRIES) {
    // Rollback to Region A
    set_boot_region(REGION_A);
    system_reset();
}
```

---

## 📚 구현 체크리스트

- [ ] Phase 1: HTTPS download, TCP file transfer, UDS 0x34/0x36/0x37
- [ ] Phase 2: VCI collection, UDS 0x22, JSON aggregation, Readiness check
- [ ] Phase 3: Driver UI, HMI integration, UDS 0x31, Bank switching
- [ ] Phase 4: Self-test, Result reporting, Inactive bank update

---

**참고:**
- OTA 프로젝트: https://github.com/zlseong/OTA-Project-with-PQC-hybrid-TLS.git
- 통합 메시지: `docs/unified_message_format.md`
- Zonal Gateway: `docs/zonal_gateway_architecture.md`

