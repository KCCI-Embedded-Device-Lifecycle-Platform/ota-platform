# Development Status

Last updated: 2026-07-19

이 문서는 새 개발 세션이 현재 작업 지점부터 바로 이어갈 수 있도록 관리하는
인수인계 문서다. 전체 시스템 구조와 장기 결정은
[`ARCHITECTURE.md`](ARCHITECTURE.md)를 참고한다. `DESIGN.md`는 UI 디자인
전용 문서다.

## 1. 학습 및 진행 원칙

- 사용자가 코드를 직접 이해하면서 진행한다.
- 한 단계에서는 하나의 작은 책임이나 검증 가능한 흐름을 구현한다.
- C, C++, Java, Spring, Leshan, Wakaama 개념을 이미 안다고 가정하지 않는다.
- 포인터의 대상과 소유권, Java Bean의 생성 시점과 의존성 주입을 설명한다.
- 코드가 호출되는 시점과 protocol request/response 흐름을 함께 설명한다.
- 사용자가 build 또는 실행 결과를 확인한 뒤 다음 단계로 이동한다.
- 동작을 확인하기 전 불필요한 추상화와 대규모 rename을 먼저 하지 않는다.

## 2. 2026-07-19 아키텍처 피벗

### 2.1 결정

인터넷에 직접 연결 가능한 MCU 및 Embedded Linux 장치는 각각 LwM2M Client를
탑재하고 중앙 Leshan Server에 직접 연결한다.

```text
Device LwM2M Client  <---- CoAP/DTLS ---->  Leshan LwM2M Server
  /3 /5 /telemetry
```

Gateway를 제거해도 LwM2M을 제거하는 것이 아니다. custom 서버-클라이언트
protocol 대신 표준 LwM2M Client-Server 구조를 더 직접적으로 사용한다.

Gateway는 향후 BLE, CAN, RS-485, 레거시 non-IP 장치를 지원해야 할 때만 별도
adapter로 추가한다.

### 2.2 중단한 계획

현재 제품 기본 경로에서는 다음을 구현하지 않는다.

- LwM2M Gateway Object `/25`
- prefixed End Device Object routing
- Gateway와 하위 MCU 사이 custom TCP binary protocol
- Gateway가 여러 인터넷 연결 장치의 ID와 Object Instance를 중계하는 구조
- 하위 장치 전송을 위한 Boost.Asio session manager
- Fake STM32를 Gateway 뒤에 붙이는 기존 OTA 시나리오

### 2.3 기존 코드 처리

현재 `gateway/`, `GatewayApp`, `gateway_client_context_t`,
`ota_gateway` 이름은 기존 설계에서 남아 있다. 코드를 폐기하지 않고 **직접
연결 BMS Device의 Linux reference LwM2M Client**로 재사용한다.

이름 변경은 `/5` 기본 흐름을 보존하는 상태에서 별도 단계로 수행한다. 현재
`connectionList`는 하위 Device 목록이 아니라 LwM2M Server transport
connection 목록이므로 그대로 필요하다.

## 3. 현재 기술 선택

### Server

- Java: 21
- Build: Maven
- Framework: Spring Boot 4.1.0
- LwM2M Server: Eclipse Leshan 2.0.0-M18 embedded
- HTTP port: `8081`
- LwM2M CoAP port: `5683`

### Reference Device Client

- C++20 application + Eclipse Wakaama C library
- Wakaama snapshot: `94ff56f77a2d24a5890e0e703809a47633aa7d4b`
- Transport: POSIX UDP
- 개발 보안: NoSec `coap://`
- Server address: `127.0.0.1:5683`
- Local UDP port: `56830`
- Short Server ID: `123`
- Lifetime: `300` seconds
- Binding: `U`
- 현재 test endpoint: `gateway-01`

`gateway-01`은 현재 회귀 테스트를 보존하기 위한 역사적 이름이며 제품
identity 결정이 아니다. 이후 실제 Device serial 또는 provisioning으로 발급된
안정적인 endpoint name으로 바꾼다.

Wakaama는 현재 reference client 구현이다. 최종적으로 모든 MCU/Linux 장치에
동일 SDK를 강제하지 않는다. 표준 호환성, `/5` 완성도, 자원 사용량, 보안,
라이선스를 기준으로 platform별 Client를 선택할 수 있다.

### Persistence and artifacts

- 제품 persistence DB: PostgreSQL 18 방향 결정
- `server/compose.yaml`: PostgreSQL 18 개발 환경 초안, application 미연동
- JDBC/JPA 또는 migration dependency와 schema: 아직 없음
- Firmware binary storage: local filesystem부터 시작 가능, adapter 미구현
- Object Storage 제품 선택: 아직 없음
- CDN: 현재 milestone에 사용하지 않음

## 4. 현재 저장소 구조

```text
ota_project/
├── ARCHITECTURE.md
├── DEVELOPMENT_STATUS.md
├── DESIGN.md
├── experiments/
│   ├── leshan-demo/
│   └── wakaama/
├── gateway/                         # historical name; reference Device Client
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── gateway_app.hpp
│   │   ├── object_bms.h
│   │   ├── object_firmware.h
│   │   ├── standard_objects.h
│   │   └── wakaama_hooks.h
│   ├── src/
│   │   ├── main.cpp
│   │   ├── gateway_app.cpp
│   │   ├── object_bms.c
│   │   ├── object_firmware.c
│   │   └── wakaama_hooks.c
│   └── tests/
│       └── gateway_smoke.cpp
└── server/
    ├── pom.xml
    ├── compose.yaml                  # PostgreSQL development draft
    └── src/main/
        ├── java/ota/platform/server/
        │   ├── OtaServerApplication.java
        │   ├── config/LeshanServerConfiguration.java
        │   ├── listener/GatewayRegistrationListener.java
        │   ├── controller/GatewayController.java
        │   └── telemetry/
        │       ├── BmsTelemetry.java
        │       └── BmsTelemetryStore.java
        └── resources/
            ├── application.properties
            └── models/bms.xml
```

## 5. Reference Device Client 구현 완료

### 5.1 Build와 target

- C99와 C++20을 함께 사용하는 CMake project
- Wakaama를 subdirectory로 포함
- `ota_gateway_objects` static library
- `ota_gateway` reference client executable
- `ota_gateway_smoke` integration/smoke executable

Target 이름은 아직 historical name을 유지한다.

### 5.2 LwM2M Objects

- Security Object `/0`
- Server Object `/1`
- Device Object `/3`
- Firmware Update Object `/5` Read wiring scaffold
- Custom BMS Object `/33000`
- BMS voltage `/33000/0/0`, Float, initial value `12.7`

`object_bms.c`는 전체 Resource Read와 특정 Resource Read를 처리하고,
`free_bms_object()`로 정리한다.

### 5.3 Transport와 lifecycle

`GatewayApp`의 현재 동작:

1. UDP socket 생성
2. `/0`, `/1`, `/3`, `/5`, `/33000` Object 생성
3. `lwm2m_init()`과 `lwm2m_configure()`
4. `lwm2m_step()`으로 Registration 시작
5. `select()`, `recvfrom()`으로 packet 수신
6. sender를 `connectionList`에서 찾음
7. `lwm2m_handle_packet()`으로 Wakaama에 전달
8. `STATE_READY` 이후 같은 loop에서 Read 등 server request 처리
9. SIGINT/SIGTERM에서 loop 종료
10. Deregistration 후 context, connections, Objects, socket 정리

Object pointer, Wakaama context, connection list, socket의 생성 실패 및 정리
경로를 smoke test와 실제 실행으로 확인했다.

### 5.4 통합 검증

- Leshan Registration과 `STATE_READY`
- Registration Update
- `/3` Read
- `/33000/0/0` Read
- SIGINT 정상 종료와 Deregistration

Leshan Demo에는 `/33000` model이 없어 raw OPAQUE
`414b3333`으로 보였고, 제품 embedded Leshan은 `bms.xml`을 로드해 Float로
decode하는 것을 확인했다.

## 6. Embedded Leshan Server 구현 완료

### 6.1 Spring lifecycle

- `@SpringBootApplication` 진입점
- `@Configuration`의 `LeshanServer` Bean
- Bean `initMethod = "start"`, `destroyMethod = "destroy"`
- Californium UDP endpoint provider
- Leshan default models와 `models/bms.xml` load

Spring Boot가 `ota.platform.server` 하위 package를 component scan하면서
Configuration, Listener, Controller, Store를 생성한다. Constructor parameter를
통해 필요한 Bean을 주입한다.

### 6.2 Registration events

`GatewayRegistrationListener`가 다음 event를 logging한다.

- registered
- updated
- unregistered와 expired 여부

실제 reference client의 등록과 정상 해제 log를 확인했다.

### 6.3 BMS API와 임시 저장

현재 API:

```text
GET /api/gateways/{endpoint}/bms/voltage
GET /api/gateways/{endpoint}/bms/voltage/latest
```

첫 API는 Leshan으로 `ReadRequest(33000, 0, 0)`을 보내고, Float 응답을
`BmsTelemetry` record로 변환한 뒤 `BmsTelemetryStore`에 저장한다.

`BmsTelemetryStore`는 `ConcurrentHashMap<String, BmsTelemetry>`으로
endpoint별 최신값 하나만 보관한다. 서버를 재시작하면 사라지는 의도적인
임시 구현이며 DB를 대체하지 않는다.

서버 build, UDP `5683` listen, reference client registration, HTTP Read 및
latest 응답을 확인했다.

## 7. 현재 한계와 기술 부채

- `Gateway*` class, API, target 이름이 새 Device 중심 구조와 맞지 않음
- test endpoint가 아직 `gateway-01`
- `/5`는 State/Result Read wiring뿐이며 Package/URI/Execute는 아직 미구현
- 실제 firmware download, hash/signature 검증, install, reboot, rollback 없음
- NoSec만 사용
- Device credential provisioning 없음
- telemetry는 요청 시 Read하며 Observe/Notify 또는 Send가 아님
- BMS 전용 Controller와 in-memory latest store만 있음
- PostgreSQL dependency, schema, migration 없음
- firmware binary storage adapter 없음
- 사용자, Device ownership, Device Profile, OTA Campaign model 없음
- custom Object model을 classpath에서 정적으로만 로드함
- automated server test와 CTest 등록 없음

## 8. 다음 작업: Milestone 3 Direct Device Firmware Update

DB부터 구현하지 않는다. 먼저 한 장치에서 표준 `/5` OTA protocol이 끝까지
성립하는지 검증한다. 그래야 DB schema가 실제 OTA 상태를 기준으로 설계된다.

### 8.1 Wakaama 예제의 현재 수준

`experiments/wakaama/examples/client/common/object_firmware.c`가 존재한다.
다음 Resource callback 형태는 참고하거나 재사용할 수 있다.

- Package `/5/0/0` Write
- Package URI `/5/0/1` Write
- Update `/5/0/2` Execute
- State `/5/0/3` Read
- Update Result `/5/0/5` Read

그러나 예제는 binary를 저장하지 않고 URI를 다운로드하지 않으며, 검증, 설치,
reboot, rollback, 상태 영속화도 하지 않는다. 따라서 build에 연결되는 것만으로
OTA가 구현됐다고 간주하면 안 된다.

### 8.2 Object wiring 완료 상태

Reference client의 Object 배열에 `/5`를 추가하고 Leshan과 protocol wiring만
먼저 검증하는 단계다.

- Firmware Object의 public factory/cleanup 선언 완료
- CMake target에 project-owned `object_firmware.c` 연결 완료
- Object 배열을 4개에서 5개로 확장
- 생성 실패와 cleanup 경로에 `/5` 포함
- State `Idle(0)`, Update Result `Initial(0)` local Read smoke 검증
- Package Write와 Update Execute가 `5.01 Not Implemented`인지 검증
- Registration link-format의 `</5>;ver=1.0,</5/0>` 확인
- Embedded Leshan의 실제 State/Result Read는 아직 확인하지 않음

이 단계에서는 실제 firmware를 설치하지 않는다. 예제 상태값과 전이가 표준과
맞는지도 별도로 확인한 뒤 product-owned update backend를 연결한다.

### 8.3 바로 다음 작은 단계

Spring 서버에 최소 firmware status Read API를 추가한다.

1. endpoint registration 조회
2. `/5/0/3` State Read
3. `/5/0/5` Update Result Read
4. 두 값을 하나의 HTTP JSON 응답으로 반환
5. 실제 reference client를 등록해 curl로 확인

### 8.4 그 다음 vertical slice

1. Linux 임시 파일에 firmware stream 저장
2. 예상 SHA-256과 실제 파일 hash 비교
3. State와 Update Result를 올바르게 변경하고 notify
4. Update Execute에서 simulated install 수행
5. 성공, download 실패, hash 불일치, install 실패 결과 검증
6. Spring 서버에 최소 OTA trigger API 추가
7. protocol 검증 뒤 Package URI download와 Object Storage adapter로 확장

실제 MCU 단계에서는 같은 관리 흐름에 flash write, signature verification,
A/B boot, boot confirmation, rollback callback을 연결한다.

## 9. 이후 작업 순서

1. Milestone 3 direct `/5` OTA vertical slice
2. `Gateway*` 명칭을 `Device*`로 작은 단위 rename
3. Device endpoint와 credential provisioning
4. PostgreSQL schema와 migration
5. firmware artifact metadata와 Object Storage adapter
6. 범용 telemetry ingestion 및 Observe/Notify
7. Device Profile과 동적 Object model registry
8. OTA Campaign, 여러 Device 동시성, staged rollout
9. Admin UI
10. 독립적인 여러 LwM2M Client 및 Server와 상호운용성 검증

## 10. Build와 실행

### Reference Device Client

```bash
cmake -S gateway -B /tmp/ota-gateway-build
cmake --build /tmp/ota-gateway-build --target ota_gateway
cmake --build /tmp/ota-gateway-build --target ota_gateway_smoke
/tmp/ota-gateway-build/ota_gateway
```

### Embedded Server

```bash
cd server
mvn clean package
mvn spring-boot:run
```

현재 NoSec reference client 실행에는 `127.0.0.1:5683`에서 Leshan Server가
실행 중이어야 한다.

## 11. 현재 회귀 검증 기준

- `ota_gateway`와 `ota_gateway_smoke` build
- Spring server package build
- UDP `5683` listen
- reference Device의 Registration과 READY 전환
- `/33000/0/0`이 Float `12.7`로 decode
- HTTP current/latest API 응답
- Client 종료 후 Deregistration와 server unregister event

`/5` wiring에서 다음을 확인했다.

- Registration에 `/5/0` 포함
- State `/5/0/3` local Read
- Update Result `/5/0/5` local Read
- `/5` cleanup을 포함한 smoke test

Embedded Leshan을 통한 두 Resource Read는 다음 단계에 추가한다.

## 12. 새 세션 시작 지침

새 세션에서는 먼저 다음 파일을 읽는다.

- `ARCHITECTURE.md`
- `DEVELOPMENT_STATUS.md`
- `gateway/include/gateway_app.hpp`
- `gateway/src/gateway_app.cpp`
- `gateway/include/object_firmware.h`
- `gateway/src/object_firmware.c`
- `gateway/include/standard_objects.h`
- `gateway/CMakeLists.txt`
- `experiments/wakaama/examples/client/common/object_firmware.c`
- `server/src/main/java/ota/platform/server/config/LeshanServerConfiguration.java`

Milestone 1 Client foundation과 Milestone 2 Server foundation을 다시 구현하지
않는다. 다음에는 8.3절의 firmware status Read API부터 이어간다. `/25` 또는
Gateway-하위 장치 protocol 방향으로 돌아가지 않는다.
