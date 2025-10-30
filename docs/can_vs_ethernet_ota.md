# CAN vs Ethernet ECU OTA 

## [TARGET] ** **

**"CAN  , CAN ECU   OTA "** [OK]

---

## [TABLE] ** **

### **Ethernet ECU:**

```
Bandwidth: 100 Mbps~1 Gbps
  = 12.5 MB/s ~ 125 MB/s

3 MB Firmware Download:
  = 0.24 ~ 2 ()
  = 2~5 ()

[OK]     ( )
[OK]     
[OK]  OTA 
```

### **CAN ECU:**

```
CAN 2.0: 500 Kbps = 62.5 KB/s
  3 MB Download = 48 ()
                = 1.5~2 ()

CAN-FD: 1 Mbps = 125 KB/s  
  3 MB Download = 24 ()
                = 1~1.5 ()

[X]    
  - CAN   
  -    
  - /  
  
[OK] IGN OFF  
  -   
  -  
```

---

## [BUILD] **Zonal Controller  ( !)**

### **:**

```
+--------------------------------------------------+
|  CCU/Gateway (Central Controller)                |
|                                                  |
|  :                                            |
|  1.  ECU   (Flash 4 Click)       |
|  2. Ethernet ECU:  OTA                   |
|  3. CAN ECU:    IGN OFF        |
|                                                  |
|  +----------------------------+                  |
|  |  Flash 4 Click (64 MB)     |                  |
|  |  -------------------------|                  |
|  |  Ethernet Zone:            |                  |
|  |  • ADAS ECU FW ()      |                  |
|  |  • IVI ECU FW ()       |                  |
|  |                            |                  |
|  |  CAN Zone ():          |                  |
|  |  • Engine ECU FW (3 MB)    | <-  !     |
|  |  • Brake ECU FW (3 MB)     |                  |
|  |  • Trans ECU FW (3 MB)     |                  |
|  +----------------------------+                  |
+----------+-------------------+-------------------+
           | Ethernet          | CAN
           | ()            | ()
           v                   v
    +----------+        +----------+
    | Ethernet |        | CAN ECU  |
    |   ECU    |        |          |
    |          |        |      |
    | Flash 4  |        | Flash  |
    | Click OK |        | (6 MB)   |
    +----------+        +----------+
```

---

## [UPDATE] **CAN ECU OTA  **

### **: Engine ECU (CAN) **

```
Day 1, 10:00 ( ):
+-----------------------------------------+
| Server -> Gateway (Internet)             |
| "Engine ECU v2.0 available"             |
+-----------------------------------------+
         |
+-----------------------------------------+
| Gateway: Download (Ethernet, )      |
| -> Flash 4 Click                   |
|    /flash4/ecu_engine_v2.0.bin (3 MB)   |
|                                         |
| Time: 2                               |
|  :  [OK]                      |
+-----------------------------------------+
         |
+-----------------------------------------+
| Gateway: Verify                         |
| • CRC32: OK                             |
| • PQC Signature: OK                     |
| • Version: OK                           |
+-----------------------------------------+
         |
+-----------------------------------------+
| Gateway -> Driver                        |
| "Engine ECU update ready"               |
| "Will install when parked"              |
|                                         |
| 10:00 ~ 18:00:   (!)       |
+-----------------------------------------+

Day 1, 18:00 (,  OFF):
+-----------------------------------------+
| Gateway: IGN OFF !                  |
+-----------------------------------------+
         |
+-----------------------------------------+
| Gateway -> Engine ECU (CAN)              |
| UDS: RequestDownload (Bank B)           |
+-----------------------------------------+
         |
+-----------------------------------------+
| Gateway -> Engine ECU (CAN)              |
| UDS: TransferData (Blocks)              |
|                                         |
| Flash 4 Click -> CAN -> Engine ECU        |
| Time: 1.5 ( OK,  )      |
+-----------------------------------------+
         |
+-----------------------------------------+
| Engine ECU:                             |
| • Bank B                    |
| •  (CRC, Signature)                 |
| • Active Bank = B                   |
| • !                                 |
+-----------------------------------------+

Day 2, 08:00 ( ):
+-----------------------------------------+
| Engine ECU Bootloader:                  |
| • Bank B                            |
| • Bank B                            |
| • v2.0 ! [OK]                         |
+-----------------------------------------+
```

---

## [SECURITY] ** **

### **Critical ECU :**

```cpp
enum class EcuCriticality {
    CRITICAL,      // Engine, Brake, Steering
    IMPORTANT,     // Transmission, Battery
    NON_CRITICAL   // Infotainment, HVAC
};

bool isSafeToUpdate(const EcuInfo& ecu, VehicleState state) {
    // Critical ECU IGN OFF!
    if (ecu.criticality == EcuCriticality::CRITICAL) {
        return (state == VehicleState::IGN_OFF || 
                state == VehicleState::CHARGING);
    }
    
    // Non-Critical  
    if (ecu.criticality == EcuCriticality::NON_CRITICAL) {
        if (state == VehicleState::IGN_ON_PARKED) {
            return true;  //   Infotainment  OK
        }
    }
    
    return false;
}
```

---

## [PACKAGE] **Flash 4 Click  ( )**

### **64 MB  :**

```
Gateway Flash 4 Click (64 MB):

+-------------------------------------+
|  Zone 1: Ethernet ECU         |
|  (ECU  ,  ) |
|  ---------------------------------  |
|  • ADAS ECU backup: 5 MB           |
|  • IVI ECU backup: 8 MB            |
|                                     |
|  Zone 2: CAN ECU  (!)     |
|  (Gateway  )              |
|  ---------------------------------  |
|  • Engine ECU: 3 MB               |
|  • Brake ECU: 3 MB                |
|  • Trans ECU: 3 MB                |
|  • Body ECU: 2 MB                 |
|  • Door ECU #1-4: 4 MB            |
|                                     |
|  Zone 3: Gateway              |
|  ---------------------------------  |
|  • Gateway v1.1: 5 MB              |
|                                     |
|  Zone 4: Logs & Data                |
|  ---------------------------------  |
|  • OTA : 10 MB                  |
|  •  : 10 MB                 |
|                                     |
|  Total: ~56 MB / 64 MB ( )  |
+-------------------------------------+
```

---

## [TARGET] ****
