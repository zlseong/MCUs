# [NOTICE] 이 폴더는 더 이상 사용되지 않습니다

## 상태: DEPRECATED (단계적 제거 예정)

이 `tc375_bootloader/` 폴더는 새로운 구조로 마이그레이션되었습니다.

## 마이그레이션 완료

### Bootloader 코드
- `ssw/` -> `end_node_ecu/tc375/bootloader/ssw/`
- `ssw/` -> `zonal_gateway/tc375/bootloader/ssw/`
- `bootloader/` -> `end_node_ecu/tc375/bootloader/stage2/`
- `bootloader/` -> `zonal_gateway/tc375/bootloader/stage2/`
- `common/boot_common.h` -> 복사됨

### 새 위치

```
VMG_and_MCUs/
├── end_node_ecu/tc375/bootloader/
│   ├── ssw/              (Stage 1)
│   ├── stage2/           (Stage 2)
│   └── common/           (boot_common.h)
│
└── zonal_gateway/tc375/bootloader/
    ├── ssw/              (Stage 1)
    ├── stage2/           (Stage 2)
    └── common/           (boot_common.h)
```

## 아직 이동되지 않은 파일들

### common/ 폴더
다음 파일들은 아직 재배치되지 않았습니다:
- `doip_client.c/h`
- `doip_message.c/h`
- `doip_socket_lwip.c`
- `uds_handler.c/h`
- `uds_platform_tc375.c`
- `zonal_gateway.h`

이 파일들은 다음 위치에서 사용되고 있습니다:
- `end_node_ecu/tc375/src/` (DoIP Client, UDS)
- `zonal_gateway/tc375/src/` (DoIP Server/Client, UDS)

### Example 파일들
- `example_doip_client.c` - 참고용
- `example_zonal_gateway.c` - 새 구조에 반영됨

## 이 폴더를 언제 삭제해야 하나요?

**지금은 삭제하지 마세요!** 다음 경우에만 삭제:

1. `common/` 폴더의 모든 파일이 새 위치로 이동됨
2. 모든 빌드가 새 구조에서 정상 작동함
3. 더 이상 참조할 필요가 없음

## 임시 사용

필요한 경우 이 폴더의 파일들을 참조하여:
- DoIP/UDS 구현 확인
- Example 코드 참고
- 빌드 스크립트 참고

## 대체 방법

대신 다음을 사용하세요:
- End Node ECU: `end_node_ecu/tc375/`
- Zonal Gateway: `zonal_gateway/tc375/`
- 공통 프로토콜: `common/protocol/`

## 관련 문서

- [MIGRATION_COMPLETE.md](../MIGRATION_COMPLETE.md)
- [CLEANUP_GUIDE.md](../CLEANUP_GUIDE.md)

---

**최종 업데이트:** 2025-10-31
**상태:** DEPRECATED - 참고용으로만 사용

