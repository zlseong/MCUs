# Bootloader Comparison: Zonal Gateway vs End Node ECU

## 요약

**Zonal Gateway와 End Node ECU는 동일한 부트로더를 사용합니다!**

## 왜 동일한가?

### 1. 하드웨어 동일
- 모두 TC375 MCU
- 동일한 메모리 맵 (6MB PFLASH, 192KB DSRAM)
- 동일한 이중 뱅크 구조

### 2. 기능 동일
- OTA 업데이트 지원
- Fail-safe 메커니즘
- CRC32 + PQC Signature 검증
- Dual-bank switching

### 3. 코드 재사용
- 유지보수 용이
- 검증된 코드 사용
- 버그 수정 시 한 번에 적용

## 구조 비교

| Component | End Node ECU | Zonal Gateway | 비고 |
|-----------|--------------|---------------|------|
| **SSW (Stage 1)** | ✅ 동일 | ✅ 동일 | 불변, 64KB |
| **Stage 2** | ✅ 동일 | ✅ 동일 | OTA 가능, 196KB |
| **Application** | ECU App | ZG App | 다름 (역할 차이) |
| **Memory Map** | ✅ 동일 | ✅ 동일 | TC375 표준 |
| **Boot Logic** | ✅ 동일 | ✅ 동일 | Fail-safe |

## 파일 위치

### End Node ECU
```
end_node_ecu/tc375/bootloader/
├── common/boot_common.h
├── ssw/ssw_main.c
└── stage2/stage2_main.c
```

### Zonal Gateway
```
zonal_gateway/tc375/bootloader/
├── common/boot_common.h     ← ECU와 동일
├── ssw/ssw_main.c          ← ECU와 동일
└── stage2/stage2_main.c    ← ECU와 동일
```

## Application의 차이

부트로더는 동일하지만, **Application**은 다릅니다:

### End Node ECU Application
```c
// DoIP Client only
DoIPClient_t doip_client;
ecu_connect_to_zg();
```

### Zonal Gateway Application
```c
// DoIP Server + Client
int doip_server_tcp_socket;  // ECU들을 위한 서버
DoIPClient_t vmg_client;      // VMG 연결용 클라이언트

zg_serve_ecus();              // Server 역할
zg_connect_to_vmg();          // Client 역할
```

## OTA 업데이트 흐름

### End Node ECU
```
VMG → ZG → ECU
              ↓
         [Bootloader]
         1. SSW 검증
         2. Stage 2 검증
         3. Application 검증
         4. 실행
```

### Zonal Gateway
```
VMG → ZG
      ↓
 [Bootloader] ← 동일한 로직
 1. SSW 검증
 2. Stage 2 검증
 3. Application 검증
 4. 실행
```

## 메모리 사용량

| Component | Size | ECU | ZG |
|-----------|------|-----|-----|
| SSW (Stage 1) | 64 KB | ✅ | ✅ |
| Stage 2 | 196 KB | ✅ | ✅ |
| Application | ~2.1 MB | ✅ | ✅ |
| **Total** | ~2.4 MB/bank | ✅ | ✅ |

## 빌드 프로세스

### ECU
```bash
cd end_node_ecu/tc375/bootloader
# Build SSW
tricore-gcc -c ssw/ssw_main.c -o ssw.o

# Build Stage 2
tricore-gcc -c stage2/stage2_main.c -o stage2.o

# Build ECU Application
cd ../src
tricore-gcc -c ecu_node.c -o ecu_app.o
```

### Zonal Gateway
```bash
cd zonal_gateway/tc375/bootloader
# Build SSW (동일)
tricore-gcc -c ssw/ssw_main.c -o ssw.o

# Build Stage 2 (동일)
tricore-gcc -c stage2/stage2_main.c -o stage2.o

# Build ZG Application (다름)
cd ../src
tricore-gcc -c zonal_gateway.c -o zg_app.o
```

## 왜 처음에 ZG에 부트로더가 없었나?

**단순 실수!** 

- ECU에만 부트로더를 구현했음
- ZG도 동일한 TC375이므로 동일한 부트로더 필요
- 이제 ECU의 부트로더를 ZG에도 복사함

## 결론

### ✅ 동일한 것
- SSW (Stage 1 Bootloader)
- Stage 2 Bootloader
- Memory Map
- Boot Logic
- Fail-safe Mechanism

### ❌ 다른 것
- Application 코드
  - ECU: DoIP Client만
  - ZG: DoIP Server + Client

### 장점
- 코드 재사용
- 유지보수 용이
- 동일한 OTA 프로세스
- 검증된 안정성

**Zonal Gateway와 End Node ECU는 동일한 부트로더를 사용하며, Application만 다릅니다!**

