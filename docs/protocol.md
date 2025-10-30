# TC375 ↔ Gateway  

## 

TC375  Vehicle Gateway   .

##  

- ****: TLS 1.3
- ****: 8765 ( )
- ****: JSON over TCP

##  

###  

```json
{
  "type": "MESSAGE_TYPE",
  "device_id": "tc375-xxx",
  "payload": {},
  "timestamp": "2025-10-21 00:00:00"
}
```

##  

### 1. HEARTBEAT ( -> )

   

```json
{
  "type": "HEARTBEAT",
  "device_id": "tc375-sim-001",
  "payload": {
    "status": "alive"
  },
  "timestamp": "2025-10-21 15:30:00"
}
```

****:  10

### 2. SENSOR_DATA ( -> )

  

```json
{
  "type": "SENSOR_DATA",
  "device_id": "tc375-sim-001",
  "payload": {
    "temperature": 25.5,
    "pressure": 101.3,
    "voltage": 12.0
  },
  "timestamp": "2025-10-21 15:30:05"
}
```

****:  5

### 3. STATUS_REPORT ( -> )

  

```json
{
  "type": "STATUS_REPORT",
  "device_id": "tc375-sim-001",
  "payload": {
    "uptime": 3600,
    "memory_usage": 45.2,
    "cpu_usage": 12.5,
    "firmware_version": "1.0.0"
  },
  "timestamp": "2025-10-21 15:30:00"
}
```

### 4. COMMAND_ACK ( -> )

  

```json
{
  "type": "COMMAND_ACK",
  "device_id": "tc375-sim-001",
  "payload": {
    "command_id": "cmd-12345",
    "success": true,
    "result": "Command executed successfully"
  },
  "timestamp": "2025-10-21 15:30:10"
}
```

### 5. ERROR ( -> )

 

```json
{
  "type": "ERROR",
  "device_id": "tc375-sim-001",
  "payload": {
    "error": "Sensor read timeout",
    "code": 1001
  },
  "timestamp": "2025-10-21 15:30:15"
}
```

##  

```
1. TCP  
   TC375 -> Gateway (port 8765)

2. TLS 
   - TLS 1.3
   - ()  
   - () PQC Hybrid

3.  
   TC375 -> Gateway: STATUS_REPORT ( )

4.  
   - Heartbeat: 10
   - Sensor Data: 5
   - Command ACK:   

5.  
   - Graceful: FIN/ACK
   -  Heartbeat timeout
```

##  

###  
-   (5 )
-  : 

### 
- Heartbeat : 30 ->   
-   : 10

## 

- ****: TLS 1.3 
- ****: ()  
- ****: TLS  

##  

- CAN  
- OTA  
-   (Protocol Buffers)
- PQC (Post-Quantum Cryptography)  

