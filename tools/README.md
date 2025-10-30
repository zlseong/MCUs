# PQC Simulation Tools

End-to-End 시뮬레이션 도구 for all PQC parameter combinations

## pqc_simulator

모든 ML-KEM (512/768/1024) + ML-DSA/ECDSA 조합을 테스트합니다.

### 빌드

```bash
cd tools
mkdir build && cd build
cmake ..
make
```

### 사용법

#### 전체 테스트 (12개 조합)
```bash
./pqc_simulator
```

출력:
```
=============================================================================
                     End-to-End PQC Performance Comparison                   
=============================================================================
ID | Configuration              | Handshake | Transfer | Total   | Overhead 
---|----------------------------|-----------|----------|---------|----------
 0 | ML-KEM-512+ECDSA-P256      |   13.00ms |  800.00ms |  813.00ms |  1696 B
 1 | ML-KEM-512+ECDSA-P384      |   13.00ms |  800.00ms |  813.00ms |  1794 B
 2 | ML-KEM-512+ECDSA-P521      |   13.00ms |  800.00ms |  813.00ms |  1833 B
 3 | ML-KEM-768+ECDSA-P256      |   14.00ms |  800.00ms |  814.00ms |  2336 B
 4 | ML-KEM-768+ECDSA-P384      |   14.00ms |  800.00ms |  814.00ms |  2434 B
 5 | ML-KEM-768+ECDSA-P521      |   14.00ms |  800.00ms |  814.00ms |  2473 B
 6 | ML-KEM-1024+ECDSA-P256     |   15.00ms |  800.00ms |  815.00ms |  3264 B
 7 | ML-KEM-1024+ECDSA-P384     |   15.00ms |  800.00ms |  815.00ms |  3362 B
 8 | ML-KEM-1024+ECDSA-P521     |   15.00ms |  800.00ms |  815.00ms |  3401 B
 9 | ML-KEM-512+ML-DSA-44       |   17.00ms |  800.00ms |  817.00ms |  4992 B
10 | ML-KEM-768+ML-DSA-65       |   20.00ms |  800.00ms |  820.00ms |  6533 B
11 | ML-KEM-1024+ML-DSA-87      |   23.00ms |  800.00ms |  823.00ms |  9255 B
=============================================================================

[RECOMMENDATIONS]
-----------------------------------------------------
  [FASTEST]        : #0 - ML-KEM-512 + ECDSA-P256
  [RECOMMENDED]    : #10 - ML-KEM-768 + ML-DSA-65 (balanced)
  [LIGHTWEIGHT]    : #0 - ML-KEM-512 + ECDSA-P256 (embedded)
  [HIGH SECURITY]  : #11 - ML-KEM-1024 + ML-DSA-87 (critical)
```

#### 특정 조합 테스트
```bash
# ML-KEM-768 + ML-DSA-65 (권장)
./pqc_simulator 10

# ML-KEM-512 + ECDSA-P256 (가장 빠름)
./pqc_simulator 0

# ML-KEM-1024 + ML-DSA-87 (최고 보안)
./pqc_simulator 11
```

### Configuration IDs

| ID | ML-KEM | Signature | Security | Type |
|----|--------|-----------|----------|------|
| 0  | 512    | ECDSA-P256 | 128-bit | Hybrid |
| 1  | 512    | ECDSA-P384 | 128-bit | Hybrid |
| 2  | 512    | ECDSA-P521 | 128-bit | Hybrid |
| 3  | 768    | ECDSA-P256 | 192-bit | Hybrid |
| 4  | 768    | ECDSA-P384 | 192-bit | Hybrid |
| 5  | 768    | ECDSA-P521 | 192-bit | Hybrid |
| 6  | 1024   | ECDSA-P256 | 256-bit | Hybrid |
| 7  | 1024   | ECDSA-P384 | 256-bit | Hybrid |
| 8  | 1024   | ECDSA-P521 | 256-bit | Hybrid |
| 9  | 512    | ML-DSA-44  | 128-bit | Pure PQC |
| **10** | **768** | **ML-DSA-65** | **192-bit** | **Pure PQC** (권장) |
| 11 | 1024   | ML-DSA-87  | 256-bit | Pure PQC |

### 시뮬레이션 시나리오

```
[Server] -> [VMG] -> [Zonal Gateway] -> [ECU]
   |         |            |               |
   |         +-- DoIP Server (13400)      |
   |         |                            |
   +---------+-- HTTPS Client             |
   |                                      |
   +-- MQTT Client                        |
```

**테스트 항목:**
1. **TLS Handshake**: KEM + Signature 시간
2. **Data Transfer**: 10 MB OTA 패키지 전송
3. **Overhead**: 핸드셰이크 오버헤드 (bytes)

### 결과 분석

#### Handshake Time
- **ML-KEM 영향**: 512 < 768 < 1024
- **Signature 영향**: ECDSA < ML-DSA (ML-DSA가 더 느림)
- **최고 보안 + 성능**: ML-KEM-768 + ML-DSA-65

#### Overhead
- **ECDSA 조합**: ~2-3 KB (가벼움)
- **ML-DSA 조합**: ~5-9 KB (크지만 Pure PQC)
- **임베디드 시스템**: ECDSA 조합 권장
- **미래 대비**: ML-DSA 조합 권장

### 실제 테스트

시뮬레이터는 추정값입니다. 실제 성능은 다음으로 측정:

#### VMG DoIP Server 테스트
```bash
cd vehicle_gateway/build
./vmg_doip_server \
    certs/mlkem768_mldsa65_server.crt \
    certs/mlkem768_mldsa65_server.key \
    certs/ca.crt 13400
```

#### TC375 Simulator 테스트
```bash
cd tc375_simulator/build
./tc375_simulator \
    --vmg-ip 127.0.0.1 \
    --vmg-port 13400 \
    --kem mlkem768 \
    --sig mldsa65 \
    --cert certs/mlkem768_mldsa65_client.crt \
    --key certs/mlkem768_mldsa65_client.key
```

### 벤치마크 프로젝트

정확한 성능 측정:
```bash
git clone https://github.com/zlseong/Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-.git
cd Benchmark_mTLS_with_PQC-ML-KEM-ML-DGS-
./benchmark_all.sh
```

## 관련 문서

- [PQC Params Header](../common/protocol/pqc_params.h)
- [VMG README](../vehicle_gateway/README.md)
- [PQC Implementation](../docs/vmg_pqc_implementation.md)

