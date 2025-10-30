#   : ECU ID, MAC, IP, VIN 

## [TABLE]     

### **3-Tier Storage Strategy**

```
+-------------------------------------------------+
|  Flash (Permanent - Immutable)                  |
|  - ECU Serial Number                           |
|  - MAC Address                                 |
|  - VIN (Vehicle ID)                            |
|  - Hardware Version                            |
|  - Calibration Data                            |
|                                                 |
|  :                                           |
|  •  1                        |
|  • Write-Protected                              |
|  •                                   |
+-------------------------------------------------+

+-------------------------------------------------+
|  EEPROM (Configuration - Rewritable)            |
|  - IP Address                                 |
|  - Gateway Host/Port                          |
|  - Feature Flags                              |
|  - CAN Settings                               |
|  - Log Level                                  |
|                                                 |
|  :                                           |
|  • UDS WriteDataByID                  |
|  •                               |
|  • 10                                |
+-------------------------------------------------+

+-------------------------------------------------+
|  RAM (Runtime - Volatile)                       |
|  -   (Temperature, Pressure) [TABLE]           |
|  - CPU/Memory  [TABLE]                         |
|  -   [TABLE]                                  |
|  - Uptime [TABLE]                                    |
|                                                 |
|  :                                           |
|  •                                 |
|  •                                 |
|  • JSON Gateway                    |
+-------------------------------------------------+
```

---

## [SIGNAL] **JSON  **

### **1.   ( 1)**

```json
// TC375 -> Gateway (    )

{
  "type": "DEVICE_REGISTRATION",
  "device": {
    // === Flash  () ===
    "ecu_serial": "TC375-SIM-001-20251021",
    "mac_address": "02:00:00:AA:BB:CC",
    "hardware_version": "TC375TP-LiteKit-v2.0",
    "vin": "KMHGH4JH1NU123456",
    "vehicle_model": "Genesis G80 EV",
    "vehicle_year": 2025,
    
    // === EEPROM  () ===
    "ip_address": "192.168.1.100",
    "gateway_host": "gateway.example.com",
    "gateway_port": 8765,
    "tls_enabled": true,
    "ota_enabled": true,
    
    // === Flash  () ===
    "firmware_version": "1.0.0",
    "bootloader_version": "1.0.0"
  },
  "timestamp": "2025-10-21 15:30:00"
}

// Gateway -> Server DB 
//  ecu_serial Primary Key
```

### **2. Heartbeat (10)**

```json
// TC375 -> Gateway ()

{
  "type": "HEARTBEAT",
  "device_id": "TC375-SIM-001-20251021",  // !
  "payload": {
    "status": "alive"
  },
  "timestamp": "2025-10-21 15:30:10"
}

// [INFO] MAC, VIN    !
// Server DB  !
```

### **3.   (5)**

```json
// TC375 -> Gateway ()

{
  "type": "SENSOR_DATA",
  "device_id": "TC375-SIM-001-20251021",
  "payload": {
    // RAM  ()
    "temperature": 25.5,
    "pressure": 101.3,
    "voltage": 12.0,
    "rpm": 2000,
    "speed": 80
  },
  "timestamp": "2025-10-21 15:30:15"
}
```

### **4.   (1)**

```json
// TC375 -> Gateway

{
  "type": "STATUS_REPORT",
  "device_id": "TC375-SIM-001-20251021",
  "payload": {
    // RAM ()
    "uptime": 3600,
    "cpu_usage": 45.2,
    "memory_free": 2048,
    "gateway_connected": true,
    
    // EEPROM ( )
    "current_ip": "192.168.1.100",
    "ota_enabled": true,
    
    // Flash ( )
    "active_bank": "A",
    "firmware_version": "1.0.0"
  },
  "timestamp": "2025-10-21 15:31:00"
}
```

---

## [CONFIG] **UDS  **

### **Gateway -> TC375: IP  **

```json
// Gateway 

{
  "command": "WRITE_CONFIG",
  "device_id": "TC375-SIM-001-20251021",
  "parameters": {
    "data_id": "0xF190",  // IP Address (UDS DID)
    "value": "192.168.1.200"
  }
}

// | UDS  
// UDS: WriteDataByID(0xF190, [192, 168, 1, 200])

// TC375 :
handleWriteDataById(request) {
    if (data_id == 0xF190) {
        // EEPROM 
        device_info.updateIPAddress(new_ip);
        saveToEEPROM();
        
        return POSITIVE_RESPONSE;
    }
}

// TC375 -> Gateway 

{
  "type": "CONFIG_UPDATED",
  "device_id": "TC375-SIM-001-20251021",
  "data_id": "0xF190",
  "old_value": "192.168.1.100",
  "new_value": "192.168.1.200"
}
```

---

## [STORAGE] ** TC375  **

### **Flash Configuration Sector:**

```c
// 0x80572000 - Configuration Sector (256 KB)

struct FlashConfigSector {
    // Offset 0x0000
    PermanentConfig permanent;     // 1 KB
    
    // Offset 0x0400 (1 KB)
    uint8_t production_cert[1024]; //  
    
    // Offset 0x0800 (2 KB)
    uint8_t calibration_table[2048];
    
    // : Reserved for future
} __attribute__((aligned(4096)));

// Write Protection
void lockPermanentConfig(void) {
    IfxFlash_setProtection(
        0x80572000, 
        0x80573000,  //  4KB 
        IfxFlash_Protection_write
    );
}
```

### **EEPROM Layout:**

```c
// TC375 EEPROM (64 KB total)

0xAF000000  +------------------------+
            | Boot Config (512 B)    |  <- Stage 1/2, App  
0xAF000200  +------------------------+
            | Device Config (1 KB)   |  <- IP, Port, Flags
0xAF000600  +------------------------+
            | Network Config (1 KB)  |  <- DNS, Subnet, etc.
0xAF000A00  +------------------------+
            | CAN Config (2 KB)      |  <- CAN IDs, Filters
0xAF001200  +------------------------+
            | User Settings (4 KB)   |  <-  
0xAF002200  +------------------------+
            | Reserved (56 KB)       |
0xAF010000  +------------------------+
```

---

##  ** **

### **   :**

```cpp
// BAD:     ([X])
void sendHeartbeat() {
    json msg = {
        {"ecu_serial", "..."},      //   (!)
        {"mac", "..."},             //   (!)
        {"vin", "..."},             //   (!)
        {"status", "alive"}
    };
    send(msg);
}

// GOOD:   ([OK])
void sendHeartbeat() {
    json msg = {
        {"device_id", device_info.getEcuSerial()},  // 
        {"type", "HEARTBEAT"},
        {"payload", {"status", "alive"}}
    };
    send(msg);
}

// Server device_id DB 
// MAC, VIN    !
```

---

## [TARGET] ** **

### **Q: ECU ID, MAC, IP, VIN   JSON ?**

### **A:   !**

**   (1):**
```json
[OK]   JSON 
   - ECU Serial, MAC, VIN, IP 
```

**  ():**
```json
[OK] device_id 
   - ECU Serial 
   - Server DB  
   
[WARNING]    
   - IP  
   - Config  
```

** :**
```
ECU Serial, MAC, VIN -> Flash ( , )
IP, Port, Flags      -> EEPROM (UDS  )
 ,         -> RAM (JSON )
```

** :**
```
:         JSON <- 1!
 :   device_id <- 
 :   UDS WriteDataByID <-  
```

? 
