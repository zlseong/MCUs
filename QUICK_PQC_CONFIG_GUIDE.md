# Quick PQC Configuration Guide

## How to Change PQC Parameters

### Step 1: Edit Configuration Number

Open `vehicle_gateway/src/vmg_gateway.cpp` and find line 22:

```cpp
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  1   // Change this number!
```

### Step 2: Choose Your Configuration

| Number | ML-KEM | Signature | Speed | Security | Use Case |
|--------|--------|-----------|-------|----------|----------|
| **1** | **768** | **ECDSA-P256** | **Fast** | **192-bit** | **Default (Recommended)** |
| 0 | 512 | ECDSA-P256 | Fastest | 128-bit | Testing/Low-power |
| 2 | 1024 | ECDSA-P256 | Fast | 256-bit | High security |
| 3 | 512 | ML-DSA-44 | Medium | 128-bit | Pure PQC (light) |
| 4 | 768 | ML-DSA-65 | Medium | 192-bit | Pure PQC (balanced) |
| 5 | 1024 | ML-DSA-87 | Slow | 256-bit | Pure PQC (max security) |

### Step 3: Rebuild

```bash
cd vehicle_gateway/build
make
```

### Step 4: Run

```bash
./vmg_gateway
```

You'll see:
```
[VMG] PQC Configuration (External Server only):
  KEM:       ML-KEM-768
  Signature: ECDSA-P256
  Config ID: 1
```

## Examples

### Example 1: Maximum Performance (Testing)
```cpp
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  0   // ML-KEM-512 + ECDSA
```

### Example 2: Pure PQC (Future-proof)
```cpp
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  4   // ML-KEM-768 + ML-DSA-65
```

### Example 3: Maximum Security (Critical Systems)
```cpp
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  5   // ML-KEM-1024 + ML-DSA-87
```

## Advanced: Test All Configurations

Use the PQC simulator tool:

```bash
cd tools
mkdir build && cd build
cmake ..
make

# Test all 12 combinations
./pqc_simulator

# Test specific configuration
./pqc_simulator 1
```

## Important Reminders

### ✓ PQC is Used For:
- VMG → External Server (HTTPS downloads)
- VMG → MQTT Broker (telemetry)

### ✗ PQC is NOT Used For:
- VMG → Zonal Gateway (plain DoIP)
- Zonal Gateway → ECU (plain DoIP)

**Why?** Vehicle internal network is:
- Physically isolated (no remote quantum threat)
- Performance-critical (< 1ms latency required)
- Resource-constrained (MCU limitations)

## Configuration Details

### ML-KEM (Key Exchange)
- **512**: Fastest, 128-bit security, ~800 bytes keys
- **768**: Balanced, 192-bit security, ~1184 bytes keys ← **Default**
- **1024**: Slowest, 256-bit security, ~1568 bytes keys

### Signatures
- **ECDSA**: Fast, small (~64 bytes), classical security ← **Default**
- **ML-DSA**: Slower, large (~2-5KB), quantum-safe

### Recommended Combinations
1. **Production**: ML-KEM-768 + ECDSA-P256 (config 1)
2. **Research**: ML-KEM-768 + ML-DSA-65 (config 4)
3. **Testing**: ML-KEM-512 + ECDSA-P256 (config 0)

## Troubleshooting

### OpenSSL Version
```bash
openssl version
# Require: OpenSSL 3.2+

# Check PQC support
openssl list -kem-algorithms | grep mlkem
openssl list -signature-algorithms | grep mldsa
```

### Certificate Mismatch
If you change config, regenerate certificates:
```bash
cd vehicle_gateway
./scripts/generate_pqc_certs.sh
```

### Performance Issues
If handshake is too slow:
1. Use ECDSA instead of ML-DSA (configs 0-2)
2. Use smaller ML-KEM (512 instead of 1024)

## Summary

**Change ONE number** (line 22 in `vmg_gateway.cpp`):
```cpp
#define PQC_CONFIG_ID_FOR_EXTERNAL_SERVER  1   // 0-5
```

**Default = 1** (ML-KEM-768 + ECDSA-P256)
- Fast handshake (~14ms)
- Quantum-safe key exchange
- Lightweight signature
- Best for production

That's it! 🎉

