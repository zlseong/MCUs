#    ()

## [TARGET] 3-Tier Hierarchical Architecture

```
+-----------------------------------------------------+
|  Tier 1: Central Gateway                            |
|  ----------------------------------------------   |
|  VMG (MacBook / Linux Server)                       |
|  +- Role: Central Gateway                           |
|  +- DoIP: Server (Port 13400)  [SERVER]                   |
|  +- Network: 192.168.1.1                            |
|  +- Passive: Waits for connections                  |
+----------------+------------------------------------+
                 | Ethernet (Client connects)
                 |
+----------------v------------------------------------+
|  Tier 2: Domain/Zonal Controller                    |
|  ----------------------------------------------   |
|  MCU #1 (TC375 Simulator)                           |
|  +- Role: Domain Controller                         |
|  +- Uplink:   DoIP Client -> VMG  [CLIENT]                 |
|  +- Downlink: DoIP Server (Port 13401)  [SERVER]          |
|  +- Network: 192.168.1.10                           |
|  +- Dual Role: ECU + Gateway                        |
+----------------+------------------------------------+
                 | Ethernet / CAN
                 |
+----------------v------------------------------------+
|  Tier 3: End ECUs                                   |
|  ----------------------------------------------   |
|  MCU #2, #3, #4... (TC375 Simulator)                |
|  +- Role: ECU (End Node)                            |
|  +- DoIP: Client -> MCU#1  [CLIENT]                        |
|  +- Network: 192.168.1.11, .12, .13...              |
|  +- Passive: Waits for commands                     |
+-----------------------------------------------------+
```

---

## [TABLE]  

### Startup Sequence:

```
1. VMG 
   +- DoIP Server  (Port 13400)
   +- Listen & Wait

2. MCU #1 
   +- DoIP Client  (to VMG)
   +- Connect to 192.168.1.1:13400
   +- Routing Activation
   +- Register as Domain Controller
   +- DoIP Server  (Port 13401)
       +- Listen for downstream

3. MCU #2 
   +- DoIP Client  (to MCU#1)
   +- Connect to 192.168.1.10:13401
   +- Routing Activation
   +- Register as ECU
```

---

## [CODE]  

### 1. VMG (Central Gateway) - DoIP Server

```cpp
// vmg_server.cpp

#include "doip_server.hpp"
#include <map>
#include <memory>

class VMGServer {
public:
    VMGServer() : port_(13400) {}
    
    void start() {
        // DoIP Server 
        doip_server_.listen(port_);
        
        std::cout << "[VMG] Central Gateway started" << std::endl;
        std::cout << "[VMG] Listening on port " << port_ << std::endl;
        
        //   (Passive)
        while (true) {
            // MCU#1 (Domain Controller)  
            if (auto client = doip_server_.acceptClient()) {
                std::cout << "[VMG] Domain Controller connected" << std::endl;
                
                //   
                handleDomainController(client);
            }
        }
    }
    
    void handleDomainController(std::shared_ptr<DoIPConnection> conn) {
        // Domain Controller 
        domain_controllers_.push_back(conn);
        
        //   
        while (conn->isConnected()) {
            auto msg = conn->receiveMessage();
            
            if (msg.type == MessageType::STATUS_REPORT) {
                // MCU#1  
                processStatusReport(msg);
            }
            else if (msg.type == MessageType::OTA_REQUEST) {
                // OTA  
                handleOTARequest(conn, msg);
            }
        }
    }
    
    // VMG MCU#1  
    void sendCommandToDomain(const std::string& domain_id,
                             const Command& cmd) {
        for (auto& dc : domain_controllers_) {
            if (dc->getId() == domain_id) {
                dc->sendCommand(cmd);
                break;
            }
        }
    }
    
private:
    uint16_t port_;
    DoIPServer doip_server_;
    std::vector<std::shared_ptr<DoIPConnection>> domain_controllers_;
};

int main() {
    VMGServer vmg;
    vmg.start();  // Blocking call
    return 0;
}
```

---

### 2. MCU #1 (Domain Controller) - Dual Role

```cpp
// mcu1_domain_controller.cpp

#include "doip_client.hpp"
#include "doip_server.hpp"
#include <thread>

class DomainController {
public:
    DomainController(const std::string& id) 
        : id_(id), vmg_port_(13400), local_port_(13401) {}
    
    void start() {
        // Thread 1: VMG Client (Uplink)
        std::thread uplink_thread([this]() {
            this->connectToVMG();
        });
        
        // Thread 2: ECU Server (Downlink)
        std::thread downlink_thread([this]() {
            this->startECUServer();
        });
        
        uplink_thread.join();
        downlink_thread.join();
    }
    
private:
    // Uplink: VMG  (Client)
    void connectToVMG() {
        DoIPClient vmg_client;
        
        // VMG  (Active)
        if (!vmg_client.connect("192.168.1.1", vmg_port_)) {
            std::cerr << "[MCU1] Failed to connect to VMG" << std::endl;
            return;
        }
        
        std::cout << "[MCU1] Connected to VMG" << std::endl;
        
        // Routing Activation
        vmg_client.activateRouting(id_);
        
        // VMG  
        while (true) {
            auto cmd = vmg_client.receiveCommand();
            processVMGCommand(cmd);
        }
    }
    
    // Downlink: ECU   (Server)
    void startECUServer() {
        DoIPServer ecu_server;
        
        // Local server 
        ecu_server.listen(local_port_);
        
        std::cout << "[MCU1] ECU Server started on port " 
                  << local_port_ << std::endl;
        
        // ECU   (Passive)
        while (true) {
            if (auto ecu = ecu_server.acceptClient()) {
                std::cout << "[MCU1] ECU connected: " 
                          << ecu->getId() << std::endl;
                
                // ECU 
                std::thread([this, ecu]() {
                    this->handleECU(ecu);
                }).detach();
            }
        }
    }
    
    void processVMGCommand(const Command& cmd) {
        std::cout << "[MCU1] Command from VMG: " << cmd.type << std::endl;
        
        if (cmd.type == CommandType::OTA_UPDATE) {
            // OTA  ECU 
            forwardOTAToECU(cmd.target_ecu, cmd.data);
        }
        else if (cmd.type == CommandType::DIAGNOSTIC) {
            //    ECU 
            routeToECU(cmd.target_ecu, cmd);
        }
    }
    
    void handleECU(std::shared_ptr<DoIPConnection> ecu) {
        // ECU 
        ecus_[ecu->getId()] = ecu;
        
        // ECU  
        while (ecu->isConnected()) {
            auto msg = ecu->receiveMessage();
            
            // VMG  (Routing)
            forwardToVMG(msg);
        }
    }
    
    void forwardOTAToECU(const std::string& ecu_id, 
                         const std::vector<uint8_t>& data) {
        auto it = ecus_.find(ecu_id);
        if (it != ecus_.end()) {
            it->second->sendOTA(data);
        }
    }
    
private:
    std::string id_;
    uint16_t vmg_port_;
    uint16_t local_port_;
    
    std::map<std::string, std::shared_ptr<DoIPConnection>> ecus_;
};

int main() {
    DomainController dc("Domain_Powertrain");
    dc.start();
    return 0;
}
```

---

### 3. MCU #2 (End ECU) - DoIP Client Only

```cpp
// mcu2_ecu.cpp

#include "doip_client.hpp"
#include "uds_handler.hpp"

class ECU {
public:
    ECU(const std::string& id) : id_(id) {}
    
    void start() {
        DoIPClient client;
        
        // MCU#1 (Domain Controller) 
        if (!client.connect("192.168.1.10", 13401)) {
            std::cerr << "[MCU2] Failed to connect to Domain Controller" 
                      << std::endl;
            return;
        }
        
        std::cout << "[MCU2] Connected to Domain Controller" << std::endl;
        
        // Routing Activation
        client.activateRouting(id_);
        
        //  
        while (true) {
            auto cmd = client.receiveCommand();
            
            if (cmd.type == CommandType::UDS_DIAGNOSTIC) {
                // UDS 
                auto response = uds_handler_.process(cmd.data);
                client.sendResponse(response);
            }
            else if (cmd.type == CommandType::OTA_UPDATE) {
                // OTA 
                handleOTA(cmd.data);
            }
        }
    }
    
private:
    std::string id_;
    UDSHandler uds_handler_;
    
    void handleOTA(const std::vector<uint8_t>& firmware) {
        std::cout << "[MCU2] Receiving OTA update..." << std::endl;
        // OTA 
    }
};

int main() {
    ECU ecu("ECU_Engine");
    ecu.start();
    return 0;
}
```

---

## [SIGNAL]  

### Scenario 1: VMG -> MCU#2  (Routing)

```
1. VMG  :
   VMG -> MCU#1: "Send UDS 0x22 to ECU_Engine"

2. MCU#1 :
   MCU#1 -> MCU#2: DoIP Diagnostic Message (UDS 0x22)

3. MCU#2 :
   MCU#2: Process UDS
   MCU#2 -> MCU#1: DoIP Response

4. MCU#1 :
   MCU#1 -> VMG: Response
```

### Scenario 2: OTA (MCU#2 )

```
1. VMG:
   -  
   - : "Update ECU_Engine"

2. MCU#1:
   - OTA  
   -  ECU 
   MCU#1 -> MCU#2: OTA Data (chunked)

3. MCU#2:
   -  
   - Flash 
   - 
   - 
```

### Scenario 3:   (Uplink)

```
:

MCU#2 -> MCU#1: Status Report
MCU#1 -> VMG: Aggregated Status
  ( ECU  )
```

---

## [NETWORK]  

### IP :

```
VMG (Central):     192.168.1.1    Port: 13400 (Server)
MCU#1 (Domain):    192.168.1.10   Port: 13401 (Server for downstream)
MCU#2 (ECU):       192.168.1.11   (Client only)
MCU#3 (ECU):       192.168.1.12   (Client only)
MCU#4 (ECU):       192.168.1.13   (Client only)
```

###  :

```
VMG:13400  <---- MCU#1 (Client)
                  |
               MCU#1:13401 <---- MCU#2 (Client)
                            <---- MCU#3 (Client)
                            <---- MCU#4 (Client)
```

---

## [CONNECT]  

###  =  (!)

```
   :
+- Network Interface (eth0, en0)
+- TCP/IP Stack
+- Socket API (BSD Sockets)
    +- socket()
    +- bind()
    +- listen()  <- Server
    +- connect() <- Client
    +- accept()
    +- send()
    +- recv()
```

###   :

```
VMG:
- Server Socket: bind(13400), listen(), accept()

MCU#1:
- Client Socket: connect(VMG:13400)
- Server Socket: bind(13401), listen(), accept()

MCU#2:
- Client Socket: connect(MCU#1:13401)
```

---

## [OK]  

### :

```
VMG (Server) [SERVER]
   | Client
MCU#1 (Client + Server) [CLIENT][SERVER]
   | Client
MCU#2 (Client) [CLIENT]
```

### :

|  |  | Uplink | Downlink |
|------|------|--------|----------|
| **VMG** | Central Gateway | - | DoIP Server |
| **MCU#1** | Domain Controller | DoIP Client | DoIP Server |
| **MCU#2** | ECU | DoIP Client | - |

###  :

```
 : VMG -> MCU#1 -> MCU#2
```

** !** [TARGET]

 :
1. VMG DoIP Server 
2. MCU#1 Dual Role 
3. MCU#2 Client 

?

