# ISO 13400 DoIP  

## [DOCS] ISO 13400 

```
ISO 13400: Diagnostics over Internet Protocol (DoIP)

Part 1: General information and use case definition
Part 2: Transport protocol and network layer services  <- !
Part 3: Wiring harness diagnostic interface based on Ethernet
Part 4: DoIP entity address configuration
```

---

## [REFERENCE] ISO 13400-2:   

###    :

```
Section 7.1 - Connection Management:

"DoIP shall use TCP/IP as the underlying transport protocol.
The DoIP entity shall maintain a persistent TCP connection 
during the diagnostic session."

:
[OK] TCP/IP 
[OK] Persistent Connection ( )
[OK] Diagnostic Session  
```

### Socket Type :

```
Section 7.2 - Socket Communication:

"DoIP diagnostic communication shall use:
- Connection-oriented socket (TCP)
- Port number: 13400 (default)
- Multiple concurrent connections: Optional"

 :
[OK] SOCK_STREAM (TCP)
[OK] Port 13400
[OK]    
```

---

## [UPDATE] ISO 13400-2  

###   :

```
7.3 - DoIP Connection Establishment:

Step 1: TCP Connection
------------------------------------
Client -> TCP SYN -> Server (port 13400)
Client <- TCP SYN-ACK <- Server
Client -> TCP ACK -> Server

Result: TCP connection established


Step 2: Routing Activation
------------------------------------
Client -> DoIP Routing Activation Request -> Server
       [Source Address, Activation Type]
       
Client <- DoIP Routing Activation Response <- Server
       [Routing Activation Code]
       
Result: DoIP session active


Step 3: Diagnostic Communication
------------------------------------
Client -> DoIP Diagnostic Message -> Server
       [Source, Target, UDS Data]
       
Client <- DoIP Diagnostic Positive/Negative ACK <- Server
       
Client <- DoIP Diagnostic Message Response <- Server
       [Response Data]

(Repeat as needed - connection remains open)


Step 4: Connection Termination
------------------------------------
Client -> TCP FIN -> Server
OR
Server -> TCP FIN -> Client (timeout/error)
```

---

## [PACKAGE] ISO 13400-2:  

### Generic DoIP Header ():

```
Section 8.2 - Generic DoIP Header Format:

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Protocol Ver   |Inverse Ver    |Payload Type                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Payload Length                                                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Payload (variable length)                                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 :
- Protocol Version: 0x02 (ISO 13400-2:2012)
                    0x03 (ISO 13400-2:2019)
- Inverse Version: 0xFD (bitwise NOT of version)
- Payload Type: 16-bit message type
- Payload Length: 32-bit length (bytes)
```

---

## [TARGET] ISO 13400-2: Connection Management

### 7.4 - Session Timeouts:

```
  :

1. Initial Inactivity Time:
   - Default: 2000 ms
   -   Routing Activation  
   
2. General Inactivity Time:
   - Default: 5000 ms
   -    
   -   connection close

3. Alive Check Request:
   - Optional
   - Keep-alive 
```

### Connection State Machine:

```
Section 7.5 - DoIP Entity States:

+---------------------------------------------+
|  State 1: Socket Created                    |
|  - TCP connection accepted                  |
|  - Waiting for routing activation           |
+---------------+-----------------------------+
                | Routing Activation
+---------------v-----------------------------+
|  State 2: Routing Activated                 |
|  - Session established                      |
|  - Ready for diagnostic messages            |
+---------------+-----------------------------+
                | Diagnostic Messages
+---------------v-----------------------------+
|  State 3: Diagnostic Active                 |
|  - Processing UDS requests                  |
|  - Connection maintained                    |
+---------------+-----------------------------+
                | Timeout / Close
+---------------v-----------------------------+
|  State 4: Socket Closed                     |
|  - Connection terminated                    |
+---------------------------------------------+
```

---

## [LIST] ISO 13400-2: Payload Types

### Section 8.3 -   :

```c
//   Payload Types

// Vehicle identification (UDP broadcast)
#define DOIP_VEHICLE_IDENTIFICATION_REQ     0x0001
#define DOIP_VEHICLE_IDENTIFICATION_RES     0x0004

// Routing activation (TCP)
#define DOIP_ROUTING_ACTIVATION_REQUEST     0x0005
#define DOIP_ROUTING_ACTIVATION_RESPONSE    0x0006

// Alive check
#define DOIP_ALIVE_CHECK_REQUEST            0x0007
#define DOIP_ALIVE_CHECK_RESPONSE           0x0008

// DoIP entity status
#define DOIP_ENTITY_STATUS_REQUEST          0x4001
#define DOIP_ENTITY_STATUS_RESPONSE         0x4002

// Diagnostic power mode
#define DOIP_DIAGNOSTIC_POWER_MODE_REQ      0x4003
#define DOIP_DIAGNOSTIC_POWER_MODE_RES      0x4004

// Diagnostic message (!)
#define DOIP_DIAGNOSTIC_MESSAGE             0x8001
#define DOIP_DIAGNOSTIC_POSITIVE_ACK        0x8002
#define DOIP_DIAGNOSTIC_NEGATIVE_ACK        0x8003
```

---

## [SECURITY] ISO 13400-2:  

### Section 9 - Security Considerations:

```
  :

"ISO 13400 does not mandate encryption, but recommends:

1. TLS/SSL for confidentiality
   - Optional but recommended
   - Version: TLS 1.2 or higher
   
2. Authentication mechanisms
   - Certificate-based (X.509)
   - Pre-shared keys
   
3. Access control
   - Source address filtering
   - Port security"

:
[WARNING]  "Optional" ( )
[OK]  "Recommended" ( )
[OK] TLS 1.2+ 
```

---

## [TABLE] ISO 13400-2  

### Question: "  ?"

### Answer from Standard:

```
ISO 13400-2, Section 7.1:
------------------------------------

"DoIP uses connection-oriented transport protocol (TCP)
with persistent connection during diagnostic session.
Multiple diagnostic messages can be exchanged over
a single TCP connection."

:
"DoIP  (connection-oriented)  (TCP)
,     (persistent).
 TCP       ."
```

### :

```
[OK] Connection-Oriented (TCP)
[OK] Persistent Connection
[OK] Multiple Messages per Connection
[OK] Session-Based
[OK] Stateful

->  "WebSocket " !
```

---

## [TARGET]   

### ISO 13400-2 Conformance:

```
Section 10 - Implementation Requirements:

Mandatory ():
------------------------------
[OK] TCP socket support
[OK] Port 13400 listening
[OK] Generic DoIP header parsing
[OK] Routing activation handling
[OK] Diagnostic message support
[OK] Connection timeout handling

Optional ():
------------------------------
[WARNING] UDP vehicle discovery
[WARNING] Multiple connections
[WARNING] Alive check
[WARNING] TLS/SSL encryption  <- !

Recommended ():
------------------------------
[HOT] TLS for security
[HOT] Certificate validation
[HOT] Source address filtering
```

---

## [REFERENCE]   (ISO 13400-2 Annex)

### Annex A - Example Communication Flow:

```
  :

Tester                          ECU
  |                              |
  +- TCP Connect --------------->|
  |  (Port 13400)                |
  |                              |
  +- Routing Activation -------->|
  |  SA: 0x0E80                  |
  |<------ Activation Success ---+
  |  Code: 0x10                  |
  |                              |
  +- Diagnostic Message -------->|
  |  SA: 0x0E80, TA: 0x1234      |
  |  Data: [0x10 0x03]           |
  |<------ Diagnostic ACK -------+
  |                              |
  |<------ Diagnostic Response --+
  |  Data: [0x50 0x03]           |
  |                              |
  +- Diagnostic Message -------->|
  |  Data: [0x22 0xF1 0x90]      |
  |<------ Diagnostic Response --+
  |                              |
  ... (connection remains open) ...
  |                              |
  +- TCP Close ----------------->|
  |                              |
```

---

## [OK] ISO 13400-2 

###   :

```
 :
------------------------------------
[OK] TCP/IP 
[OK] Connection-Oriented
[OK] Persistent Connection ( )
[OK] Session-Based ( )
[OK] Multiple messages per connection
[OK] Port 13400

:
------------------------------------
[WARNING] TLS: Optional (but recommended)
[OK] Authentication: Recommended
[OK] Access Control: Recommended

:
------------------------------------
[OK] Initial: 2000 ms
[OK] General: 5000 ms
```

---

## [TARGET]  

**ISO 13400    ?**

###   :

1. **TCP ** ()
   ```
   "Use connection-oriented TCP socket"
   ```

2. ** ** ()
   ```
   "Maintain persistent connection during diagnostic session"
   ```

3. **  ** ()
   ```
   "Multiple diagnostic messages over single connection"
   ```

4. **TLS ** (,  )
   ```
   "TLS/SSL is recommended for security"
   ```

### :

```
ISO 13400-2   :

"WebSocket   "
= Connection-Oriented
= Persistent 
= Session-Based
= Multiple Messages

 HTTP  
 TCP + DoIP  
```

**   "WebSocket  "   !** [OK]
