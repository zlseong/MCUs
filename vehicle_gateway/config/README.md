# VMG Configuration

VMG는 설정 파일이 필요하지 않습니다.
모든 설정은 실행 시 커맨드라인 인자로 전달됩니다.

## 인증서 디렉토리

인증서는 `certs/` 디렉토리에 저장됩니다.

생성 방법:
```bash
cd vehicle_gateway
./scripts/generate_pqc_certs.sh
```

생성되는 파일:
- `ca.crt`, `ca.key`: CA 인증서
- `mlkem{512,768,1024}_mldsa{44,65,87}_server.{crt,key}`: 서버 인증서
- `mlkem{512,768,1024}_mldsa{44,65,87}_client.{crt,key}`: 클라이언트 인증서

권장 조합: `mlkem768_mldsa65_*`

## 실행 옵션

### DoIP Server

```bash
./vmg_doip_server <cert> <key> <ca> <port>
```

예:
```bash
./vmg_doip_server \
    certs/mlkem768_mldsa65_server.crt \
    certs/mlkem768_mldsa65_server.key \
    certs/ca.crt \
    13400
```

### HTTPS Client

```bash
./vmg_https_client <url> <cert> <key> <ca>
```

예:
```bash
./vmg_https_client \
    https://ota.example.com/firmware.bin \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca.crt
```

### MQTT Client

```bash
./vmg_mqtt_client <broker_url> <cert> <key> <ca>
```

예:
```bash
./vmg_mqtt_client \
    mqtts://broker.example.com:8883 \
    certs/mlkem768_mldsa65_client.crt \
    certs/mlkem768_mldsa65_client.key \
    certs/ca.crt
```

## 보안 고려사항

- 개인키 파일 권한: `chmod 600 *.key`
- CA 키는 오프라인 저장
- 인증서 유효기간: 90일 권장
- 정기적인 인증서 갱신

