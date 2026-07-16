# Development Status

Last updated: 2026-07-16

이 문서는 새 개발 세션이 현재 작업 지점부터 바로 이어갈 수 있도록 관리하는
인수인계 문서다. 전체 시스템의 장기 구조와 설계 결정은
[`ARCHITECTURE.md`](ARCHITECTURE.md)를 참고한다. `DESIGN.md`는 UI 디자인 전용
문서이므로 이 개발 상태 문서와 역할이 다르다.

## 1. 학습 및 진행 원칙

- 사용자가 코드를 직접 작성한다.
- 한 번에 함수 하나, 멤버 하나, 또는 작은 개념 하나만 진행한다.
- 큰 코드를 먼저 완성하거나 여러 파일을 한꺼번에 수정하지 않는다.
- C, C++, Java, Wakaama 개념을 이미 안다고 가정하지 않는다.
- 포인터가 가리키는 대상, 소유권, 수명, 반환값을 매 단계 설명한다.
- 사용자가 빌드 또는 실행 결과를 확인한 뒤 다음 단계로 이동한다.
- 현재는 구조를 배우며 만드는 단계이므로 불필요한 추상화를 추가하지 않는다.

## 2. 확정된 기술 선택

- Central Server: Java 21, Maven, Spring Boot, Eclipse Leshan 내장
- Gateway Agent: C++20 애플리케이션 + Eclipse Wakaama C 라이브러리
- LwM2M transport: POSIX UDP
- 개발 단계 보안: NoSec (`coap://`)
- 향후 보안: DTLS 적용 필요
- 개발용 LwM2M Server: Leshan Demo Server 2.0.0-M18
- Gateway endpoint: `gateway-01`
- Leshan CoAP 주소: `127.0.0.1:5683`
- Gateway 로컬 UDP 포트: `56830`
- Short Server ID: `123`
- Registration lifetime: `300`초
- Binding: `U`

이전에 작성하던 C# ASP.NET Core 서버 방향은 중단했다. 현재 서버 방향은
Java/Spring Boot/Leshan이며, 실제 제품 서버 프로젝트는 아직 생성하지 않았다.

## 3. 의존성 및 실험 환경

- Wakaama Git submodule:
  `94ff56f77a2d24a5890e0e703809a47633aa7d4b`
- Wakaama 내부 tinyDTLS submodule:
  `86f23c65ffde9d46b6487d2210d98d58a41257d5`
- Wakaama 위치: `experiments/wakaama`
- Leshan Demo 파일 위치: `experiments/leshan-demo`
- Java 확인 버전: OpenJDK/Javac 21.0.11
- Maven 확인 버전: 3.8.7

Leshan Demo와 Wakaama 예제 클라이언트 간에는 다음 동작을 확인했다.

- Wakaama endpoint 등록
- Leshan 웹 UI에서 endpoint 조회
- Device Object Manufacturer 리소스 Read 요청 및 응답
- Registration Update
- Deregistration 후 Leshan 목록에서 endpoint 제거

## 4. 현재 저장소 구조

```text
ota_project/
├── ARCHITECTURE.md                 # 전체 소프트웨어 아키텍처
├── DEVELOPMENT_STATUS.md           # 현재 작업 인수인계
├── DESIGN.md                       # UI 디자인 전용, 수정하지 않음
├── experiments/
│   ├── leshan-demo/                # 개발용 Leshan Demo 실행 파일
│   └── wakaama/                    # Wakaama Git submodule
└── gateway/
    ├── CMakeLists.txt
    ├── include/
    │   ├── gateway_app.hpp
    │   ├── object_bms.h
    │   ├── standard_objects.h
    │   └── wakaama_hooks.h
    ├── src/
    │   ├── main.cpp
    │   ├── gateway_app.cpp
    │   ├── object_bms.c
    │   └── wakaama_hooks.c
    └── tests/
        └── gateway_smoke.cpp
```

## 5. 구현 완료 항목

### 5.1 빌드 구조

- C99와 C++20을 함께 사용하는 CMake 프로젝트
- Wakaama를 subdirectory로 포함
- Wakaama 예제 빌드는 끄고 필요한 라이브러리만 링크
- `ota_gateway_objects` 정적 라이브러리 생성
- 실제 애플리케이션 실행 파일 `ota_gateway` 생성
- 실험 및 통합 확인용 실행 파일 `ota_gateway_smoke` 분리

### 5.2 Custom BMS Object

- Object ID: `/33000`
- Instance ID: `/33000/0`
- Voltage Resource: `/33000/0/0`
- 데이터 타입: Float
- 현재 초기값: `12.7`
- Object factory: `get_bms_object()`
- Object cleanup: `free_bms_object()`
- Read callback에서 전체 리소스 요청과 특정 리소스 요청 처리
- C와 C++ 양쪽에서 사용할 수 있는 공개 헤더 구성

### 5.3 Wakaama 연결 어댑터

`gateway_client_context_t`에 다음 값을 모았다.

- Security Object 포인터
- UDP socket file descriptor
- Wakaama UDP connection 연결 목록
- address family
- Leshan host와 port

다음 Wakaama platform hook을 구현했다.

- `lwm2m_connect_server()`
- `lwm2m_close_connection()`
- Wakaama가 요구하는 전역 `g_reboot`

연결 목록에서 첫 노드와 중간 노드를 제거하는 동작을 smoke test에서 확인했다.

### 5.4 Standard Object 연결

Wakaama 예제 구현을 현재 gateway target에 연결했다.

- Security Object `/0`
- Server Object `/1`
- Device Object `/3`
- Custom BMS Object `/33000`

`standard_objects.h`는 Wakaama 예제 C 함수들을 C++ 코드에서 호출하기 위한
공개 선언을 제공한다.

### 5.5 제품 Gateway LwM2M 흐름

`GatewayApp` 제품 실행 경로에서 다음 흐름을 구현하고 확인했다.

1. UDP socket 생성
2. Security, Server, Device, BMS Object 생성
3. `lwm2m_init()`과 `lwm2m_configure()` 호출
4. 반복 `lwm2m_step()`에서 Registration 요청 송신
5. `select()`와 `recvfrom()`으로 Leshan 응답 수신
6. 송신자 주소를 connection과 매칭해 `lwm2m_handle_packet()` 호출
7. 다음 `lwm2m_step()`에서 `STATE_READY` 전환
8. READY 이후 같은 loop에서 서버 요청과 시간 기반 작업 처리
9. SIGINT 또는 SIGTERM에서 loop 종료
10. 소멸자에서 Deregistration과 자원 정리

Leshan Demo의 HTTP API로 `/33000/0/0`을 실제 Read했다.

```text
GET /api/clients/gateway-01/33000/0/0
-> CONTENT(205)
-> OPAQUE 414b3333
-> IEEE-754 Float 12.7
```

Demo JAR에는 `/33000` Object model이 없어 응답을 Float 대신 OPAQUE raw bytes로
표시한다. 현재 Demo 실험에는 별도 XML model을 추가하지 않는다. 제품용 embedded
Leshan 서버에서는 BMS Object schema와 model metadata를 정식으로 제공해야 한다.

## 6. GatewayApp 리팩터링 완료 상태

기존 `main.cpp`의 spike 코드는 `tests/gateway_smoke.cpp`로 옮겼고 제품 진입점은
`GatewayApp` 생성, `initialize()`, `run()` 호출만 담당한다.

`GatewayApp`이 소유하는 자원:

- `gateway_client_context_t`와 UDP socket 상태
- 네 개의 LwM2M Object 포인터 배열
- `lwm2m_context_t *`
- Wakaama UDP connection list

`initialize()`는 socket, Object, Wakaama context를 순서대로 생성하고 configure한다.
`run()`은 Wakaama timeout을 사용한 단일 event loop를 실행한다. 소멸자는 Wakaama
context, 남은 connection, Object, socket 순서로 정리하며 각 포인터와 fd를 빈
상태로 되돌린다.

## 7. Milestone 1 완료 상태

2026-07-16에 다음 항목을 확인했다.

- `GatewayApp` RAII 수명 관리
- Leshan Registration과 `STATE_READY`
- 반복 event loop
- 실제 Leshan 요청을 통한 `/33000/0/0` Read
- SIGINT 기반 정상 종료와 Deregistration

## 8. 다음 작업 순서

다음 개발 단계는 Milestone 2 Embedded Leshan Server다. 새 세션에서는 먼저 Java
프로젝트를 만들기 위한 Maven coordinates, package 이름, project directory와
호환되는 Spring Boot/Leshan 버전을 확인한다. 그 뒤에도 한 번에 작은 단계로
진행한다.

1. Java 21/Maven/Spring Boot project 골격 생성
2. Leshan server dependency와 lifecycle 통합
3. Gateway registration event 수신
4. BMS custom Object model과 telemetry 저장 방식 결정
5. 최소 registration/telemetry 관리 API 구현

## 9. 아직 구현하지 않은 항목

- 제품용 Java/Spring Boot/Leshan 서버 프로젝트
- DTLS credential과 보안 연결
- 표준 Firmware Update Object `/5`
- firmware artifact 다운로드, hash 검증, update state machine
- Gateway와 하위 MCU 사이의 통신
- Fake STM32 Device
- 서버 persistence와 관리 API/UI
- Qt local monitor와 local API
- 자동화된 단위 테스트 프레임워크 및 CTest 등록

## 10. 빌드 및 실행

Configure:

```bash
cmake -S gateway -B /tmp/ota-gateway-build
```

제품 애플리케이션 빌드:

```bash
cmake --build /tmp/ota-gateway-build --target ota_gateway
```

Smoke 실행 파일 빌드:

```bash
cmake --build /tmp/ota-gateway-build --target ota_gateway_smoke
```

제품 애플리케이션 실행:

```bash
/tmp/ota-gateway-build/ota_gateway
```

Smoke 실행에는 `127.0.0.1:5683`에서 실행 중인 Leshan이 필요하다.

```bash
/tmp/ota-gateway-build/ota_gateway_smoke
```

2026-07-16 기준으로 CMake configure와 두 target의 build를 다시 확인했다. Leshan
Demo와 제품 Gateway를 함께 실행해 Registration, READY, BMS Read, signal shutdown,
Deregistration까지 확인했다.

## 11. 검증 기준

Gateway 회귀 확인 시 최소한 다음을 확인한다.

- `ota_gateway`와 `ota_gateway_smoke` target build
- 제품 Gateway의 `STATE_READY` 전환
- `/33000/0/0` Read 결과가 Float `12.7`의 wire representation인지 확인
- `Ctrl+C` 후 Deregistration 전송과 endpoint 제거

## 12. 새 세션 시작 지침

새 세션에서는 먼저 `ARCHITECTURE.md`, 이 문서, 그리고 아래 파일만 읽는다.

- `gateway/include/gateway_app.hpp`
- `gateway/src/gateway_app.cpp`
- `gateway/src/main.cpp`
- `gateway/tests/gateway_smoke.cpp`
- `gateway/CMakeLists.txt`

Milestone 1 Gateway 작업을 다시 구현하지 않는다. 다음에는 8절의 Embedded Leshan
Server 준비 항목부터 이어간다.
