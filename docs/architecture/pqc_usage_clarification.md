# PQC Usage Clarification

## Critical: PQC is NOT used in Vehicle Internal Network

### Network Segmentation

```
External Internet
       |
       | [PQC-TLS] <- ML-KEM + ECDSA/ML-DSA
       |
    [VMG/CCU]
       |
       | [Plain DoIP] <- NO PQC (performance reasons)
       |
  [Zonal Gateway]
       |
       | [Plain DoIP] <- NO PQC
       |
     [ECU]
```

## Where PQC is Used

### 1. VMG ↔ External Server (HTTPS)
- **OTA package download**
- **Fleet management API**
- **Vehicle telemetry upload**

**Protocol**: HTTPS with PQC-TLS
**PQC Config**: ML-KEM-768 + ECDSA-P256 (default)

### 2. VMG ↔ MQTT Broker
- **Real-time telemetry**
- **Remote commands**
- **Status updates**

**Protocol**: MQTT over PQC-TLS
**PQC Config**: ML-KEM-768 + ECDSA-P256 (default)

## Where PQC is NOT Used

### 1. VMG ↔ Zonal Gateway
**Protocol**: DoIP over TCP (plain or standard TLS)
**Reason**: 
- Closed vehicle network (physically isolated)
- Performance critical (real-time diagnostics)
- Limited MCU resources
- No quantum threat in vehicle internal network

### 2. Zonal Gateway ↔ ECU
**Protocol**: DoIP over TCP (plain)
**Reason**: Same as above

### 3. ECU ↔ ECU
**Protocol**: CAN/CAN-FD (plain)
**Reason**: 
- Real-time automotive bus
- Microsecond-level latency requirements
- No encryption overhead tolerated

## Configuration

### Changing PQC Parameters

Edit `vmg_gateway.cpp`:

```cpp
// Line ~20
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  1   // Change this number

// Options:
// [0] ML-KEM-512  + ECDSA-P256  (fastest)
// [1] ML-KEM-768  + ECDSA-P256  (recommended) <- DEFAULT
// [2] ML-KEM-1024 + ECDSA-P256  (highest security)
// [3] ML-KEM-512  + ML-DSA-44   (pure PQC)
// [4] ML-KEM-768  + ML-DSA-65   (pure PQC)
// [5] ML-KEM-1024 + ML-DSA-87   (pure PQC)
```

### Example: Switch to Pure PQC

```cpp
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  4   // ML-KEM-768 + ML-DSA-65
```

### Example: Highest Security

```cpp
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  5   // ML-KEM-1024 + ML-DSA-87
```

## Performance Impact

### With PQC (VMG ↔ Server)
- Handshake: ~15-20ms
- Throughput: ~100 Mbps (negligible overhead after handshake)
- **Acceptable**: Server connections are infrequent

### Without PQC (VMG ↔ ZG ↔ ECU)
- Latency: < 1ms
- Throughput: Full Ethernet speed
- **Required**: Real-time diagnostics, frequent updates

## Security Rationale

### External Network (PQC Required)
- **Threat**: "Harvest now, decrypt later" attacks
- **Exposure**: Internet-facing
- **Data**: Sensitive vehicle telemetry, OTA packages
- **Solution**: PQC-TLS for quantum-safe encryption

### Internal Vehicle Network (PQC Not Required)
- **Threat**: Physical access required
- **Exposure**: Physically isolated in vehicle
- **Data**: Diagnostic messages, sensor data (transient)
- **Solution**: Physical security + standard TLS (if needed)

## Implementation Status

| Component | Connection | PQC Status | Protocol |
|-----------|-----------|------------|----------|
| External Server | ↔ VMG | **✓ PQC** | HTTPS/MQTT + PQC-TLS |
| VMG | ↔ Zonal Gateway | **✗ No PQC** | DoIP over TCP |
| Zonal Gateway | ↔ ECU | **✗ No PQC** | DoIP over TCP |
| ECU | ↔ ECU | **✗ No PQC** | CAN/CAN-FD |

## Testing Different PQC Parameters

### Quick Test
```bash
cd vehicle_gateway
# Edit src/vmg_gateway.cpp - change PQC_CONFIG_ID_FOR_EXTERNAL_SERVER
cd build
make
./vmg_gateway
```

### Benchmark All Parameters
```bash
cd tools/build
./pqc_simulator
```

### End-to-End Test with Specific Config
```bash
# Terminal 1: VMG (config ID 1 = ML-KEM-768 + ECDSA-P256)
./vmg_https_client --config 1

# Terminal 2: Simulate external server
openssl s_server -accept 8443 \
    -cert certs/mlkem768_ecdsa_secp256r1_sha256_server.crt \
    -key certs/mlkem768_ecdsa_secp256r1_sha256_server.key \
    -groups mlkem768 \
    -sigalgs ecdsa_secp256r1_sha256
```

## FAQ

### Q: Why not use PQC everywhere?
**A**: PQC adds computational overhead. Vehicle internal networks are:
1. Physically isolated (no remote quantum threat)
2. Performance-critical (real-time diagnostics)
3. Resource-constrained (MCU limitations)

### Q: Is the internal network insecure?
**A**: No. Physical isolation + standard TLS (optional) provides adequate security. Quantum computers cannot attack physically isolated networks remotely.

### Q: Can I enable PQC for internal network?
**A**: Technically yes, but not recommended. PQC handshake (~15ms) would break real-time diagnostic requirements (<1ms).

### Q: What about CAN bus encryption?
**A**: CAN bus uses hardware-level security (SecOC) for authentication, not encryption. Data is transient and real-time critical.

## Summary

✓ **Use PQC**: VMG ↔ Internet (HTTPS, MQTT)  
✗ **Don't use PQC**: VMG ↔ ZG ↔ ECU (DoIP, CAN)

**Default Config**: ML-KEM-768 + ECDSA-P256 (fast, quantum-safe KEM, lightweight signature)

**Change Config**: Edit one number in `vmg_gateway.cpp`

