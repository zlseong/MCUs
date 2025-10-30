# Zonal Gateway TC375 Bootloader

Zonal Gateway용 이중 뱅크 부트로더

## 구조

```
zonal_gateway/tc375/bootloader/
├── common/
│   └── boot_common.h        # TC375 메모리 맵, 구조체 정의
├── ssw/
│   ├── ssw_main.c          # Stage 1 부트로더 (불변)
│   └── ssw_linker.ld       # Linker script
└── stage2/
    └── stage2_main.c       # Stage 2 부트로더 (OTA 가능)
```

## 메모리 맵

### Region A (3 MB) - 0x80000000
```
0x80000000  BMI Header (256B)
0x80000100  SSW (Stage 1) - 64KB [불변]
0x800A0000  Bootloader Metadata (4KB)
0x800A1000  Stage 2 Bootloader - 196KB [OTA 가능]
0x800D2000  Application Metadata (4KB)
0x800D3000  Application - 2.1MB [OTA 가능]
```

### Region B (3 MB) - 0x82000000
```
동일 구조 (백업/대체 뱅크)
```

## End Node ECU와 동일

Zonal Gateway와 End Node ECU는 동일한 부트로더 구조를 사용합니다.
- 모두 TC375 MCU
- 모두 OTA 업데이트 지원
- 동일한 이중 뱅크 메커니즘
- 동일한 Fail-safe 로직

**차이점:**
- Zonal Gateway: DoIP Server + Client 역할
- End Node ECU: DoIP Client만

## 빌드

```bash
# TC375 toolchain 필요
cd zonal_gateway/tc375/bootloader

# SSW 빌드
tricore-gcc -c ssw/ssw_main.c -o ssw.o -T ssw/ssw_linker.ld

# Stage 2 빌드
tricore-gcc -c stage2/stage2_main.c -o stage2.o
```

## OTA 업데이트 프로세스

1. VMG → ZG: 새 펌웨어 전송 (DoIP)
2. ZG: 비활성 뱅크에 저장
3. ZG: 메타데이터 업데이트
4. ZG: Reboot
5. SSW: 새 Stage 2 검증 및 실행
6. Stage 2: 새 Application 검증 및 실행

## Fail-safe

- 부트 실패 3회 시 자동으로 백업 뱅크로 전환
- CRC32 + PQC Signature 검증
- Rollback 지원

## 참고

End Node ECU의 부트로더와 동일한 코드 베이스를 사용합니다.

