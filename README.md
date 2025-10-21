# MCUs - TC375 Lite Kit & Simulator

TC375 Lite Kit과 통신하는 디바이스 시뮬레이터 및 펌웨어 프로젝트입니다.

## 🎯 프로젝트 개요

이 레포지토리는 Vehicle Gateway Client와 TLS 통신을 수행하는 MCU 디바이스를 위한 프로젝트입니다.

### 개발 단계

#### Phase 1: Mac 시뮬레이터 (현재) ✅
- macOS에서 TC375 디바이스를 시뮬레이션
- Gateway와의 TLS 통신 프로토콜 검증
- 빠른 프로토타이핑 및 테스트

#### Phase 2: TC375 실제 펌웨어 (향후) ⏳
- Aurix Development Studio (ADS)
- iLLD + FreeRTOS/AUTOSAR
- 실제 하드웨어 배포

## 📁 프로젝트 구조

```
MCUs/
├── tc375_simulator/         # Mac용 시뮬레이터
│   ├── main.cpp
│   ├── include/
│   │   ├── tls_client.hpp
│   │   ├── device_simulator.hpp
│   │   └── protocol.hpp
│   ├── src/
│   │   ├── tls_client.cpp
│   │   ├── device_simulator.cpp
│   │   └── protocol.cpp
│   └── config/
│       └── device.json
├── docs/                    # 문서
│   ├── protocol.md
│   └── tc375_porting.md
└── CMakeLists.txt           # 빌드 시스템
```

## 🚀 빌드 및 실행

### 의존성 설치 (macOS)

```bash
brew install cmake openssl nlohmann-json
```

### 빌드

```bash
# 빌드
./build.sh

# 실행
./build/tc375_simulator
```

## 🔧 설정

`tc375_simulator/config/device.json`:

```json
{
  "device": {
    "id": "tc375-sim-001",
    "type": "TC375_SIMULATOR"
  },
  "gateway": {
    "host": "localhost",
    "port": 8765,
    "use_tls": true
  }
}
```

## 🔗 관련 프로젝트

- **Gateway Client**: https://github.com/zlseong/Client
- **Server**: https://github.com/ansj1105/mqtt_protocol

## 📝 통신 프로토콜

Gateway와의 통신 프로토콜은 `docs/protocol.md`를 참조하세요.

## 🔒 보안

- TLS 1.3 지원
- 클라이언트 인증서 인증
- PQC 준비

## 📄 라이선스

MIT License

## 🐛 버그 리포트

이슈를 등록해주세요: https://github.com/zlseong/MCUs/issues

