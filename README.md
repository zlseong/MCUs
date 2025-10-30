# VMG and MCUs - Vehicle OTA System

Automotive Over-The-Air (OTA) update system for TC375 microcontrollers with Post-Quantum Cryptography.

## [PROJECT] Overview

Complete vehicle software update infrastructure featuring:
- Vehicle Management Gateway (VMG) on MacBook Air
- Zonal Gateway support (TC375/Linux)
- End Node ECU (TC375)
- Dual-bank bootloader for safe OTA
- **PQC-TLS for external communication only**

## [ARCHITECTURE] System Architecture

```
External Server (OTA/Fleet Management)
        |
        | [OpenSSL + PQC] <- ML-KEM-768 + ECDSA-P256
        | (HTTPS/MQTT)
        v
    [VMG/CCU]
   (MacBook Air)
        |
        | [mbedTLS] <- Standard TLS 1.3 (RSA/ECDSA)
        | DoIP over TLS (Port 13400)
        v
  [Zonal Gateway]
   (TC375/Linux)
        |
        | [mbedTLS] <- Standard TLS 1.3
        | DoIP over TLS
        v
  [End Node ECU]
     (TC375)
```

### [CRITICAL] Cryptography Usage

**PQC (OpenSSL) for External:**
- ✓ VMG ↔ External Server (HTTPS) - ML-KEM-768 + ECDSA
- ✓ VMG ↔ MQTT Broker - ML-KEM-768 + ECDSA

**mbedTLS for In-Vehicle:**
- ✓ VMG ↔ Zonal Gateway (DoIP over TLS 1.3)
- ✓ Zonal Gateway ↔ ECU (DoIP over TLS 1.3)
- **Standard TLS 1.3** (RSA/ECDSA, No PQC)

**No Encryption:**
- ECU ↔ ECU (CAN/CAN-FD)

**Reason**: 
- External: PQC needed for quantum threat
- In-Vehicle: Standard TLS sufficient (physically isolated)
- CAN: Real-time critical, physically secure

## [COMPONENTS] Directory Structure

```
VMG_and_MCUs/
├── vehicle_gateway/           # VMG (MacBook Air)
│   ├── src/
│   │   ├── vmg_gateway.cpp    # Main gateway (DoIP + HTTPS + MQTT)
│   │   ├── https_client.cpp   # HTTPS with PQC
│   │   └── mqtt_client.cpp    # MQTT with PQC
│   ├── common/
│   │   └── pqc_config.h       # PQC configuration
│   └── scripts/
│       └── generate_pqc_certs.sh
│
├── zonal_gateway/             # Zonal Gateway
│   ├── tc375/                 # TC375 version
│   │   ├── src/zonal_gateway.c
│   │   ├── bootloader/        # Dual-bank bootloader
│   │   └── include/
│   └── linux/                 # Linux x86 version
│       ├── src/zonal_gateway_linux.cpp
│       └── CMakeLists.txt
│
├── end_node_ecu/              # End Node ECU
│   └── tc375/                 # TC375 version only
│       ├── src/ecu_node.c
│       ├── bootloader/        # Dual-bank bootloader
│       └── include/
│
├── common/                    # Shared libraries
│   └── protocol/
│       ├── doip_protocol.h    # DoIP standard
│       ├── uds_standard.h     # UDS services
│       ├── pqc_params.h       # PQC parameters (NEW)
│       └── pqc_params.c       # PQC implementation (NEW)
│
├── tools/                     # Development tools
│   ├── pqc_simulator.c        # End-to-End PQC testing (NEW)
│   └── CMakeLists.txt
│
├── tc375_simulator/           # TC375 emulator (x86/Linux)
│   ├── src/pqc_doip_client.cpp
│   └── main_pqc_client.cpp
│
└── docs/                      # Documentation
    ├── architecture/
    │   ├── system_overview.md
    │   └── pqc_usage_clarification.md  # PQC usage guide (NEW)
    ├── ota_scenario_detailed.md
    └── unified_message_format.md
```

## [QUICK START] Getting Started

### 1. Generate Certificates

#### For In-Vehicle (mbedTLS - Standard TLS)
```bash
cd vehicle_gateway/scripts
chmod +x generate_standard_tls_certs.sh
./generate_standard_tls_certs.sh
```

#### For External Server (OpenSSL - PQC)
```bash
cd vehicle_gateway/scripts
chmod +x generate_pqc_certs.sh
./generate_pqc_certs.sh
```

### 2. Build VMG

```bash
cd vehicle_gateway
mkdir build && cd build
cmake ..
make
```

**Generated binaries:**
- `vmg_doip_server` - mbedTLS DoIP server (In-Vehicle) **PRIMARY**
- `vmg_https_client` - OpenSSL+PQC HTTPS client (External)
- `vmg_mqtt_client` - OpenSSL+PQC MQTT client (External)

### 3. Configure PQC Parameters

Edit `vehicle_gateway/src/vmg_gateway.cpp`:

```cpp
// Line 22: Change this number to test different PQC parameters
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  1   // Default

// Available options:
// [0] ML-KEM-512  + ECDSA-P256  (fastest, 128-bit)
// [1] ML-KEM-768  + ECDSA-P256  (recommended, 192-bit) <- DEFAULT
// [2] ML-KEM-1024 + ECDSA-P256  (highest security, 256-bit)
// [3] ML-KEM-512  + ML-DSA-44   (pure PQC, 128-bit)
// [4] ML-KEM-768  + ML-DSA-65   (pure PQC, 192-bit)
// [5] ML-KEM-1024 + ML-DSA-87   (pure PQC, 256-bit)
```

### 4. Run VMG

#### In-Vehicle DoIP Server (mbedTLS)
```bash
./vmg_doip_server \
    certs/vmg_server.crt \
    certs/vmg_server.key \
    certs/ca.crt \
    13400
```

#### External Communication Test
```bash
# Gateway (PQC configuration display)
./vmg_gateway
```

### 5. Test PQC Parameters

```bash
cd tools/build
./pqc_simulator        # Test all 12 combinations
./pqc_simulator 1      # Test specific config (ML-KEM-768 + ECDSA-P256)
```

## [FEATURES] Key Features

### Vehicle Management Gateway (VMG)
- DoIP server for vehicle internal network (NO PQC)
- HTTPS client for OTA downloads (WITH PQC)
- MQTT client for telemetry (WITH PQC)
- JSON-based message protocol
- Hierarchical ECU management

### Zonal Gateway
- Dual-mode operation (DoIP server + client)
- Zone-level VCI collection
- TC375 and Linux x86 support
- Dual-bank bootloader

### End Node ECU
- DoIP client
- UDS diagnostic services
- Dual-bank OTA update
- TC375 only

### Post-Quantum Cryptography (External Only)
- **ML-KEM (FIPS 203)**: 512/768/1024
- **ML-DSA (FIPS 204)**: 44/65/87
- **ECDSA**: P-256/P-384/P-521
- Easy parameter switching (one number change)
- End-to-End simulation tool

## [PROTOCOLS] Protocol Stack

### In-Vehicle Network (No PQC)
```
Application Layer:    UDS (ISO 14229)
Transport Layer:      DoIP (ISO 13400)
Network Layer:        TCP/IP
Physical Layer:       Ethernet
```

### External Communication (With PQC)
```
Application Layer:    JSON over HTTPS/MQTT
Security Layer:       PQC-TLS 1.3 (ML-KEM + ECDSA/ML-DSA)
Transport Layer:      TCP
Network Layer:        IP
```

## [OTA] OTA Update Flow

### Phase 1: Package Transfer
```
Server -> VMG (PQC) -> ZG (Plain) -> ECU (Plain)
```

### Phase 2: VCI Collection
```
ECU -> ZG (Plain) -> VMG (Plain) -> Server (PQC)
```

### Phase 3: Activation
```
Server -> VMG (PQC) -> ZG (Plain) -> ECU (Reboot to Bank B)
```

### Phase 4: Result Reporting
```
ECU -> ZG (Plain) -> VMG (Plain) -> Server (PQC)
```

## [DEV] Development Environment

### VMG Requirements
- macOS/Linux
- OpenSSL 3.2+ (PQC support)
- CMake 3.15+
- GCC/Clang

### TC375 Requirements
- AURIX Development Studio (ADS)
- tricore-gcc toolchain
- AURIX TC375 Lite Kit

### Verification
```bash
# Check OpenSSL PQC support
openssl list -kem-algorithms | grep mlkem
openssl list -signature-algorithms | grep -i "mldsa\|dilithium"
```

## [DOCS] Documentation

- [System Overview](docs/architecture/system_overview.md)
- [PQC Usage Guide](docs/architecture/pqc_usage_clarification.md) **<- READ THIS**
- [OTA Scenario](docs/ota_scenario_detailed.md)
- [Message Format](docs/unified_message_format.md)
- [VMG README](vehicle_gateway/README.md)
- [Zonal Gateway README](zonal_gateway/README.md)
- [ECU README](end_node_ecu/README.md)
- [PQC Simulator](tools/README.md)

## [SECURITY] Security Architecture

### External Server Communication (PQC-TLS)
- **Key Exchange**: ML-KEM-768 (quantum-safe)
- **Signature**: ECDSA-P256 (lightweight) or ML-DSA-65 (pure PQC)
- **mTLS**: Mutual authentication
- **Ciphers**: AES-256-GCM

### Vehicle Internal Network (Plain DoIP)
- **Authentication**: ISO 13400 routing activation
- **Physical Security**: Isolated vehicle network
- **No PQC**: Performance and resource optimization

## [BENCHMARK] Performance

### PQC Handshake (VMG ↔ Server)
| Configuration | Time | Overhead |
|--------------|------|----------|
| ML-KEM-512 + ECDSA | 13ms | ~1.7KB |
| ML-KEM-768 + ECDSA | 14ms | ~2.3KB |
| ML-KEM-1024 + ECDSA | 15ms | ~3.3KB |
| ML-KEM-768 + ML-DSA-65 | 20ms | ~6.5KB |

### DoIP Communication (VMG ↔ ZG ↔ ECU)
- Latency: < 1ms
- No encryption overhead
- Real-time capable

## [TESTING] Testing

### Simulate All PQC Combinations
```bash
cd tools/build
./pqc_simulator
```

### Test Specific Configuration
```bash
# Edit vmg_gateway.cpp line 22
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  4   # ML-KEM-768 + ML-DSA-65

# Rebuild and run
cd vehicle_gateway/build
make
./vmg_gateway
```

### End-to-End Test
```bash
# Terminal 1: VMG
./vmg_gateway

# Terminal 2: TC375 Simulator
cd tc375_simulator/build
./tc375_simulator --vmg-ip 127.0.0.1 --vmg-port 13400
```

## [REFERENCES] References

- [NIST FIPS 203 (ML-KEM)](https://csrc.nist.gov/pubs/fips/203/final)
- [NIST FIPS 204 (ML-DSA)](https://csrc.nist.gov/pubs/fips/204/final)
- [ISO 13400 (DoIP)](https://www.iso.org/standard/74785.html)
- [ISO 14229 (UDS)](https://www.iso.org/standard/72439.html)
- [PQC Benchmark Project](https://github.com/zlseong/Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-.git)
- [OTA Project](https://github.com/zlseong/OTA-Project-with-PQC-hybrid-TLS.git)

## [LICENSE] License

MIT License - See LICENSE file for details

## [CONTACT] Contact

For questions or issues, please open an issue on GitHub.

---

**Default PQC Config**: ML-KEM-768 + ECDSA-P256 (fast, quantum-safe, lightweight)  
**Change Config**: Edit one number in `vmg_gateway.cpp` line 22
