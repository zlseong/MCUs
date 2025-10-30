#  OTA  (ISO 22842 )

## [WARNING] ** :   Flash  **

### **:**

```
:    OTA

Engine ECU ( ):
  +- Flash    
  +- OTA: Bank B  
  |   +- Flash  erase (500ms)
  |       +-    !
  +- Critical ISR Flash 
  |   +- ISR  !
  +-    [ERROR]

Result:     -> !
```

### **Flash  :**

```
TC375 Flash:
  Erase Sector: 500 ms ( )
  Write Page:   5-10 ms ( )
  
Critical Functions:
  - ISR (Interrupt Service Routine)
  - Fuel injection
  - Brake control
  - Steering assist
  
->  Flash    !
```

---

## [OK] ** : Staged OTA**

### **3 :**

```
+--------------------------------------------------+
|  Phase 1: Download (IGN ON,   )       |
|  ---------------------------------------------   |
|                              |
|  • Internal Flash  !                |
|  •                                   |
+--------------------------------------------------+

+--------------------------------------------------+
|  Phase 2: Verify (IGN ON,   )         |
|  ---------------------------------------------   |
|                                |
|  • CRC, Signature                           |
|  • Internal Flash                    |
+--------------------------------------------------+

+--------------------------------------------------+
|  Phase 3: Install (IGN OFF,   !)       |
|  ---------------------------------------------   |
|   Flash                            |
|  •                                   |
|  •                                 |
+--------------------------------------------------+
```

---

## [BUILD] **Zonal/Domain Controller  ( !)**

### ** :**

```
                 +--------------+
                 |   Server     |
                 +------+-------+
                        | OTA Package
                        v
              +---------------------+
              |  Gateway            |
              |  (Zonal Controller) |
              |                     |
              |  +---------------+  |
              |  | Flash 4 Click |  | <- 64 MB  !
              |  | (OTA Buffer)  |  |
              |  +---------------+  |
              +----------+----------+
                         | CAN/Ethernet
                         |
           +-------------+-------------+
           v             v             v
      +--------+    +--------+    +--------+
      | ECU #1 |    | ECU #2 |    | ECU #3 |
      | (TC375)|    | (TC375)|    | (TC375)|
      +--------+    +--------+    +--------+
```

### **Flash 4 Click :**

```
Flash 4 Click (64 MB):
+---------------------------------+
|  OTA Download Buffer            |
|  ----------------------------   |
|  ECU #1 Firmware: 3 MB          |
|  ECU #2 Firmware: 3 MB          |
|  ECU #3 Firmware: 3 MB          |
|  ...                            |
|  Gateway Firmware: 5 MB         |
|                                 |
|  Total:  20 ECU     |
+---------------------------------+

:
[OK]    ->  Flash (!)
[OK] IGN OFF  Internal Flash 
[OK]  ECU   
```

---

## [CODE] ** **

### **1. OTA State Machine (  )**

```cpp
// ota_manager.hpp

enum class VehicleState {
    IGN_OFF,           //   ()
    IGN_ON_PARKED,     //   +  ( )
    IGN_ON_DRIVING,    //   (!)
    CHARGING           //    ()
};

class SafeOtaManager {
public:
    // Download to external storage (IGN ON OK)
    bool downloadToBuffer(const std::string& firmware_url);
    
    // Verify without touching internal Flash (IGN ON OK)
    bool verifyInBuffer();
    
    // Install to internal Flash (IGN OFF ONLY!)
    bool installFromBuffer();
    
    // Safety checks
    bool isInstallSafe() const;
    VehicleState getVehicleState() const;
    
private:
    std::string external_buffer_;  // Flash 4 Click path
    VehicleState vehicle_state_;
};

bool SafeOtaManager::isInstallSafe() const {
    VehicleState state = getVehicleState();
    
    //  
    if (state == VehicleState::IGN_OFF) {
        return true;  // [OK]  
    }
    
    if (state == VehicleState::CHARGING) {
        return true;  // [OK]   
    }
    
    if (state == VehicleState::IGN_ON_PARKED) {
        //   
        if (isParked() && getSpeed() == 0 && getBrakePressed()) {
            return true;  // [WARNING]  
        }
    }
    
    return false;  // [X] !
}

bool SafeOtaManager::downloadToBuffer(const std::string& url) {
    // IGN   -   
    std::cout << "[OTA] Downloading to external buffer (safe in any state)" << std::endl;
    
    external_buffer_ = "/external_flash/ota_buffer.bin";
    
    // Download from server
    auto firmware = http_client_->get(url);
    
    // Save to Flash 4 Click ( )
    writeToExternalFlash(external_buffer_, firmware.body);
    
    std::cout << "[OTA] Download complete, ready for install" << std::endl;
    std::cout << "[OTA] Waiting for safe install condition (IGN OFF)" << std::endl;
    
    return true;
}

bool SafeOtaManager::installFromBuffer() {
    //   !
    if (!isInstallSafe()) {
        std::cerr << "[OTA] Install NOT SAFE in current vehicle state!" << std::endl;
        std::cerr << "[OTA] Please turn off ignition" << std::endl;
        return false;
    }
    
    std::cout << "[OTA] Vehicle state: SAFE for installation" << std::endl;
    std::cout << "[OTA] Installing firmware to internal Flash..." << std::endl;
    
    // Read from external buffer
    auto firmware = readFromExternalFlash(external_buffer_);
    
    // Write to internal Flash Bank B
    // (IGN OFF !)
    writeToInternalFlash(BANK_B_START, firmware);
    
    return true;
}
```

---

## [UPDATE] ** OTA  ( )**

### **: ECU #1 **

```
Day 1 ( ):
  10:00 Server: "ECU #1 v1.1 available"
  10:01 Gateway: Download to Flash 4 Click (3 MB)
        +-   [OK] !
        +- path: /flash4/ecu1_v1.1.bin
        
  10:05 Gateway: Verify
        +- CRC32: OK
        +- Signature: OK
        +- Version: OK
        
  10:06 Gateway -> Driver:
        "ECU #1 update ready. Will install when parked."
        
  15:00 Still driving... (  )
        +- Flash 4 Click  
        +- ECU #1   

Day 1 ( off):
  18:00 Driver:  
        |
  18:01 Gateway: IGN OFF !
        |
  18:02 Gateway -> ECU #1 (CAN):
        "Start OTA install"
        |
  18:03 ECU #1: 
        +- Gateway   (Flash 4 Click)
        +- Internal Bank B  [OK] !
        +-  (30)
        
  18:04 ECU #1 -> Gateway: "Install complete"
        
Day 2 ( ):
  08:00 Driver:  
        |
  08:01 ECU #1 Bootloader:
        +- Bank B 
        +- Bank B 
        +- v1.1 ! [OK]
```

---

## [BUILD] **Zonal Controller  ( )**

### **Tesla/Mercedes/BMW :**

```
                    +----------------+
                    |  Cloud Server  |
                    +--------+-------+
                             | Full OTA Package
                             v
              +--------------------------+
              |  Central Gateway         |
              |  (Zonal Controller)      |
              |                          |
              |  +--------------------+  |
              |  | Large Storage      |  |
              |  | - eMMC/SSD (32GB)  |  |
              |  | or                 |  |
              |  | - Flash 4 Click    |  |
              |  |   (64 MB)          |  |
              |  |                    |  |
              |  | Stores:            |  |
              |  | • ECU#1 FW (3MB)   |  |
              |  | • ECU#2 FW (3MB)   |  |
              |  | • ECU#N FW         |  |
              |  | • Gateway FW       |  |
              |  +--------------------+  |
              +--------+-----------------+
                       |
                       | CAN/Ethernet
                       |
        +--------------+--------------+
        |              |              |
        v              v              v
    +--------+    +--------+    +--------+
    | ECU #1 |    | ECU #2 |    | ECU #N |
    |        |    |        |    |        |
    | 6 MB   |    | 6 MB   |    | 6 MB   |
    | Flash  |    | Flash  |    | Flash  |
    +--------+    +--------+    +--------+
```

### ** :**

```
Gateway (With Flash 4 Click 64 MB):
+-------------------------------------+
|  Internal Storage (eMMC/SD)         |
|  or Flash 4 Click (64 MB)           |
|                                     |
|  OTA Package Repository:            |
|  +- gateway_v1.1.bin      (5 MB)   |
|  +- ecu_engine_v2.0.bin   (3 MB)   |
|  +- ecu_brake_v1.5.bin    (3 MB)   |
|  +- ecu_adas_v3.0.bin     (4 MB)   |
|  +- ... ( 10+ ECUs)             |
|                                     |
|   ECU  Flash (6 MB)   |
|  Gateway   !        |
+-------------------------------------+
```

---

## [CONFIG] **:  OTA Manager**

### ** :**

```cpp
// ota_orchestrator.hpp

namespace vmg {

// Vehicle Safety State
enum class VehicleState {
    IGN_OFF,           // [OK]  
    IGN_ACC,           // [OK]  ()
    IGN_ON_PARKED,     // [WARNING]  ( + 0km/h)
    IGN_ON_DRIVING,    // [X] !
    CHARGING           // [OK]  (EV)
};

// OTA Target ECU
struct EcuOtaTarget {
    std::string ecu_id;
    std::string firmware_version;
    uint32_t firmware_size;
    std::string buffer_path;  // Flash 4 Click path
    bool download_complete;
    bool verified;
    bool install_pending;
};

// OTA Orchestrator (Gateway)
class OtaOrchestrator {
public:
    OtaOrchestrator();
    
    // === Phase 1: Download (IGN ON OK) ===
    bool downloadFirmware(const std::string& ecu_id, 
                         const std::string& firmware_url);
    
    // === Phase 2: Verify (IGN ON OK) ===
    bool verifyFirmware(const std::string& ecu_id);
    
    // === Phase 3: Install (IGN OFF ONLY!) ===
    bool installFirmware(const std::string& ecu_id);
    
    // Batch operations
    bool downloadAllPendingUpdates();
    bool installAllWhenSafe();
    
    // Safety checks
    VehicleState getVehicleState() const;
    bool isInstallSafe() const;
    void waitForSafeState();
    
    // Storage management (Flash 4 Click)
    std::string allocateBufferSpace(const std::string& ecu_id, uint32_t size);
    bool freeBufferSpace(const std::string& ecu_id);
    uint32_t getAvailableBufferSpace() const;
    
private:
    std::map<std::string, EcuOtaTarget> pending_updates_;
    std::string external_storage_path_;  // "/mnt/flash4click"
    
    // Vehicle state monitoring
    bool readIgnitionState() const;
    bool readParkingBrake() const;
    float readVehicleSpeed() const;
    bool readChargingState() const;
};

} // namespace vmg
```

---

## [CONTROL] **  **

### ** :**

```cpp
VehicleState OtaOrchestrator::getVehicleState() const {
    // CAN    
    bool ign_on = readIgnitionState();
    bool charging = readChargingState();  // EV only
    float speed = readVehicleSpeed();
    bool parked = readParkingBrake();
    
    if (!ign_on) {
        return VehicleState::IGN_OFF;  // [OK]  
    }
    
    if (charging) {
        return VehicleState::CHARGING;  // [OK] EV  
    }
    
    if (speed > 0.1f) {
        return VehicleState::IGN_ON_DRIVING;  // [X] !
    }
    
    if (parked && speed == 0.0f) {
        return VehicleState::IGN_ON_PARKED;  // [WARNING] 
    }
    
    return VehicleState::IGN_ON_DRIVING;  // Default: 
}

bool OtaOrchestrator::isInstallSafe() const {
    VehicleState state = getVehicleState();
    
    switch (state) {
        case VehicleState::IGN_OFF:
        case VehicleState::CHARGING:
            return true;  // [OK]  
            
        case VehicleState::IGN_ACC:
            return true;  // [OK]  
            
        case VehicleState::IGN_ON_PARKED:
            // : Non-critical ECU
            // (: Infotainment OK, Engine NO)
            return false;  //  
            
        case VehicleState::IGN_ON_DRIVING:
            return false;  // [X]   !
            
        default:
            return false;
    }
}

void OtaOrchestrator::waitForSafeState() {
    std::cout << "[OTA] Waiting for safe install condition..." << std::endl;
    std::cout << "[OTA] Current state: ";
    
    while (!isInstallSafe()) {
        VehicleState state = getVehicleState();
        
        switch (state) {
            case VehicleState::IGN_ON_DRIVING:
                std::cout << "DRIVING (unsafe)" << std::endl;
                std::cout << "[OTA] Please park and turn off ignition" << std::endl;
                break;
                
            case VehicleState::IGN_ON_PARKED:
                std::cout << "PARKED (turn off ignition)" << std::endl;
                break;
                
            default:
                break;
        }
        
        // Wait and recheck
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "[OTA] Safe state detected: IGN OFF" << std::endl;
    std::cout << "[OTA] Starting installation..." << std::endl;
}
```

---

## [LIST] **OTA  (Flash 4 Click )**

### **Complete Flow:**

```cpp
// Gateway OTA Controller

void performSafeOta(const std::string& ecu_id, const std::string& firmware_url) {
    
    // ============================================================
    // PHASE 1: Download (  !)
    // ============================================================
    
    std::cout << "=== Phase 1: Download (IGN ON OK) ===" << std::endl;
    
    // 1.1 Flash 4 Click  
    std::string buffer = ota_->allocateBufferSpace(ecu_id, 3 * 1024 * 1024);
    // -> "/mnt/flash4/ecu_engine_v2.0.bin"
    
    // 1.2 Server 
    auto response = http_client_->get(firmware_url);
    
    // 1.3  Flash 
    writeToFlash4Click(buffer, response.body);
    
    std::cout << "[OTA] Downloaded to external storage (SAFE)" << std::endl;
    
    // ============================================================
    // PHASE 2: Verify (  !)
    // ============================================================
    
    std::cout << "=== Phase 2: Verify (IGN ON OK) ===" << std::endl;
    
    // 2.1 CRC 
    uint32_t crc = calculateCRC32(buffer);
    if (crc != expected_crc) {
        std::cerr << "[OTA] CRC failed, aborting" << std::endl;
        return;
    }
    
    // 2.2 PQC  
    if (!verifyDilithiumSignature(buffer, signature)) {
        std::cerr << "[OTA] Signature failed, aborting" << std::endl;
        return;
    }
    
    std::cout << "[OTA] Verification: PASSED" << std::endl;
    std::cout << "[OTA] Ready to install" << std::endl;
    
    // ============================================================
    // PHASE 3: Wait for Safe State
    // ============================================================
    
    std::cout << "=== Phase 3: Waiting for IGN OFF ===" << std::endl;
    
    // 3.1 Driver 
    notifyDriver("ECU update ready. Please park and turn off ignition.");
    
    // 3.2   
    ota_->waitForSafeState();  // Blocking!
    
    // ============================================================
    // PHASE 4: Install (IGN OFF - !)
    // ============================================================
    
    std::cout << "=== Phase 4: Install (IGN OFF - SAFE) ===" << std::endl;
    
    // 4.1 ECU   (UDS)
    sendUdsCommand(ecu_id, UDS_REQUEST_DOWNLOAD, BANK_B_ADDRESS, size);
    
    // 4.2 Flash 4 Click -> ECU 
    transferFromBufferToEcu(buffer, ecu_id);
    
    // 4.3 ECU: Internal Flash 
    // (IGN OFF !)
    
    // 4.4 
    std::cout << "[OTA] Installation complete" << std::endl;
    std::cout << "[OTA] ECU will use new firmware on next IGN ON" << std::endl;
}
```

---

## [TARGET] **  **

### **  :**

```cpp
vehicle_gateway/
+-- include/
|   +-- ota_orchestrator.hpp  [PENDING] NEW!
|   +-- flash4_driver.hpp     [PENDING] NEW! (Flash 4 Click)
+-- src/
|   +-- ota_orchestrator.cpp  [PENDING] NEW!
|   +-- flash4_driver.cpp     [PENDING] NEW!
+-- config/
    +-- ota_policy.json       [PENDING] NEW! ( )
```

### **OTA Policy :**

```json
{
  "ota_safety_policy": {
    "download_allowed": {
      "ign_off": true,
      "ign_on_parked": true,
      "ign_on_driving": true,
      "charging": true
    },
    "install_allowed": {
      "ign_off": true,
      "ign_on_parked": false,
      "ign_on_driving": false,
      "charging": true
    },
    "critical_ecus": [
      "engine_ecu",
      "brake_ecu",
      "steering_ecu"
    ],
    "non_critical_ecus": [
      "infotainment",
      "hvac",
      "lights"
    ]
  },
  "storage": {
    "external_flash": "/mnt/flash4click",
    "max_concurrent_downloads": 5,
    "retention_days": 7
  }
}
```

---

## [START] **  OTA **

### **Requirements:**

```
Hardware:
[OK] Gateway: Raspberry Pi 4 + Flash 4 Click (64 MB)
[OK] TC375: Lite Kit (6 MB internal Flash)

Software:
[OK] Gateway: OTA Orchestrator
[OK] Gateway: Flash 4 Click driver
[OK] Gateway: Vehicle state monitor (CAN reading)
[OK] TC375: A/B Bootloader
[OK] TC375: UDS Handler

Safety:
[OK] IGN state monitoring
[OK] Vehicle speed checking
[OK] Parking brake status
[OK] Download/Install separation
[OK] External buffer (Flash 4 Click)
```

---

## [INFO] ****

### **  !**

```
[X]  :
     Internal Flash  
   
[OK]   ( !):
   1.  : Gateway Flash 4 Click 
   2.  
   3. IGN OFF 
   4.   : ECU Internal Flash 
```

### **Flash 4 Click  :**

```
   ,
OTA   !

•     
•  ECU  
•   
```

---

## [TARGET] **  :**

1. [OK] Gateway ↔ Server  (!)
2. [PENDING] OTA Orchestrator 
3. [PENDING] Flash 4 Click 
4. [PENDING] Vehicle State Monitor
5. [PENDING] Safe Install Logic

** Server   !** 

    ?
