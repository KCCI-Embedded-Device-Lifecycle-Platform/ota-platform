# Development Status

Last updated: 2026-08-10

이 문서는 새 개발 세션이 현재 작업 지점부터 바로 이어갈 수 있도록 관리하는
인수인계 문서다. 전체 시스템 구조와 장기 결정은
[`ARCHITECTURE.md`](ARCHITECTURE.md)를 참고한다. `DESIGN.md`는 UI 디자인
전용 문서다.
Device Client 통합 계약은
[`DEVICE_CLIENT_CONTRACT.md`](DEVICE_CLIENT_CONTRACT.md)를 참고한다.

## 1. 학습 및 진행 원칙

- 사용자가 코드를 직접 이해하고 작성하면서 진행한다.
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

기존 Gateway prototype은 **직접 연결 BMS Device의 Linux reference LwM2M
Client**로 재분류했다. 2026-07-20에 기능을 보존한 채 다음 명칭 정리를
완료했다.

- `gateway/` -> `clients/linux-reference/`
- `GatewayApp` -> `ReferenceClientApp`
- `gateway_client_context_t` -> `wakaama_client_context_t`
- `ota_gateway*` -> `ota_linux_reference_client*`
- 서버 `Gateway*` class와 `/api/gateways` -> `Device*`와 `/api/devices`

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
- LwM2M control plane: Wakaama tinyDTLS over POSIX UDP
- DTLS Server address: `127.0.0.1:5684`
- NoSec `5683`: smoke test 회귀 검증 전용
- PSK identity와 key: 실행 환경에서 주입
- Local UDP port: `56830`
- Short Server ID: `123`
- Lifetime: `300` seconds
- Binding: `U`
- 현재 test endpoint: `linux-reference-01`

`linux-reference-01`은 reference client의 test identity이며 제품 identity
결정이 아니다. 실제 Device는 serial 또는 provisioning으로 발급된 안정적인
endpoint name을 사용한다.

Wakaama는 현재 reference client 구현이다. 최종적으로 모든 MCU/Linux 장치에
동일 SDK를 강제하지 않는다. 표준 호환성, `/5` 완성도, 자원 사용량, 보안,
라이선스를 기준으로 platform별 Client를 선택할 수 있다.

현재 산출물은 POSIX Linux용 reference implementation이며 STM32 제품용 SDK나
배포 binary가 아니다. 플랫폼과 장치 개발자의 책임 경계는
`ARCHITECTURE.md` 5.4절을 따른다.

Wakaama–tinyDTLS 통합부에서 재핸드셰이크 반환값 처리와 connection 정리 시
dangling pointer 및 `dtlsSession` 누수를 수정했다. Credential 강제 폐기와
일반 종료 모두 세그멘테이션 오류 없이 정리되는 것을 확인했다.

### Persistence and artifacts
- PostgreSQL 18 개발 환경을 `server/compose.yaml`로 실행
- Spring JDBC와 Flyway PostgreSQL 연동 완료
- `V1__create_devices.sql`: Device Registry schema
- `V2__create_device_credentials.sql`: PSK credential metadata와 lifecycle schema
- Device 등록, 단건 조회, 목록 조회 API 구현
- credential 생성, 조회, revoke, rotation API 구현
- credential 생성과 rotation은 실행 중 SecurityStore에 즉시 반영
- revoke와 rotation은 기존 DTLS session을 즉시 종료
- PostgreSQL에는 secret 원문이 아닌 `secret_reference`만 저장
- 현재 개발 secret provider는 `env:` 환경변수 참조만 지원
- 제품 secret store와 Admin API 인증 및 audit는 아직 미구현
- Firmware binary storage는 local filesystem 기반이며 Object Storage adapter는 미구현
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
├── clients/
│   ├── device-integration-kit/
│   │   ├── CMakeLists.txt
│   │   ├── README.md
│   │   ├── include/
│   │   │   ├── firmware_download_transport.h
│   │   │   ├── firmware_update_backend.h
│   │   │   └── firmware_update_service.h
│   │   ├── src/
│   │   │   └── firmware_update_service.c
│   │   ├── adapters/wakaama/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/object_firmware.h
│   │   │   └── src/object_firmware.c
│   │   └── tests/
│   │       └── firmware_update_service_test.c
│   └── linux-reference/
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── linux_firmware_update_backend.h
│       │   ├── reference_client_app.hpp
│       │   ├── object_bms.h
│       │   ├── standard_objects.h
│       │   └── wakaama_hooks.h
│       ├── src/
│       │   ├── linux_firmware_update_backend.c
│       │   ├── main.cpp
│       │   ├── reference_client_app.cpp
│       │   ├── object_bms.c
│       │   └── wakaama_hooks.c
│       └── tests/
│           ├── linux_firmware_update_backend_test.c
│           └── reference_client_smoke.cpp
└── server/
    ├── pom.xml
    ├── compose.yaml                  # PostgreSQL development draft
    └── src/main/
        ├── java/ota/platform/server/
        │   ├── OtaServerApplication.java
        │   ├── config/LeshanServerConfiguration.java
        │   ├── listener/DeviceRegistrationListener.java
        │   ├── controller/DeviceController.java
        │   └── telemetry/
        │       ├── BmsTelemetry.java
        │       └── BmsTelemetryStore.java
        └── resources/
            ├── application.properties
            └── models/bms.xml
```

## 5. Linux Reference Client Foundation 구현 완료

### 5.1 Build와 target

- C99와 C++20을 함께 사용하는 CMake project
- Wakaama를 subdirectory로 포함
- `ota_linux_reference_client_objects` static library
- `ota_linux_reference_client` reference client executable
- `ota_linux_reference_client_smoke` integration/smoke executable

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

`ReferenceClientApp`의 현재 동작:

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

2026-07-20 명칭 변경 회귀 검증에서 새 CMake target build와 smoke test,
`linux-reference-01` Registration/READY, `/api/devices/{endpoint}/bms/voltage`
HTTP `200`, 정상 Deregistration를 다시 확인했다.

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

`DeviceRegistrationListener`가 다음 event를 logging한다.

- registered
- updated
- unregistered와 expired 여부

실제 reference client의 등록과 정상 해제 log를 확인했다.

### 6.3 BMS API와 임시 저장

현재 API:

```text
GET /api/devices/{endpoint}/bms/voltage
GET /api/devices/{endpoint}/bms/voltage/latest
```

첫 API는 Leshan으로 `ReadRequest(33000, 0, 0)`을 보내고, Float 응답을
`BmsTelemetry` record로 변환한 뒤 `BmsTelemetryStore`에 저장한다.

`BmsTelemetryStore`는 `ConcurrentHashMap<String, BmsTelemetry>`으로
endpoint별 최신값 하나만 보관한다. 서버를 재시작하면 사라지는 의도적인
임시 구현이며 DB를 대체하지 않는다.

서버 build, UDP `5683` listen, reference client registration, HTTP Read 및
latest 응답을 확인했다.

## 7. 현재 한계와 기술 부채

- Reference Client의 endpoint, Server URI와 port가 아직 code에 고정됨
- Artifact data plane은 NoSec CoAP Block2이며 CoAPS, 인증, retry 정책이 없음
- Linux Backend는 reference simulation이며 실제 hash/signature와 Bootloader 검증은 장치 통합 범위임
- boot recovery는 파일 marker 기반 simulation임
- credential secret은 임시 `env:` provider를 사용하며 제품용 secret store는 없음
- credential 관리 API에 Admin 인증, 권한과 audit log가 없음
- DB와 실행 중 SecurityStore 사이의 장애 복구 및 재동기화 절차가 없음
- Device ownership, disable/delete lifecycle과 사용자 model이 없음
- telemetry는 범용 ingestion/history 구조가 아니며 BMS 중심 임시 구현임
- Firmware artifact metadata와 Object Storage adapter가 없음
- custom Object model은 classpath에서 정적으로 로드됨
- automated server integration test와 전체 CTest 등록이 없음

## 8. Milestone 3 Direct Device Firmware Update 구현 결과

DB 구현보다 먼저 한 장치에서 표준 `/5` OTA protocol을 끝까지 검증했다.
다음 절은 이 vertical slice의 구현 과정과 결과를 기록한다.

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

### 8.2 Device Integration Kit와 `/5` wiring 완료

재사용 가능한 Client 구성요소를 `clients/device-integration-kit/`으로 분리했다.

- Firmware Update Service가 State, Update Result와 offset을 소유
- Device Update Backend 함수표와 오류 변환 구현
- Download Transport 함수표 정의
- Wakaama `/5` v1.2 Adapter를 Kit 아래로 이동
- State, Update Result, Protocol Support, Delivery Method Read
- Severity와 Maximum Defer Period Read/Write
- Package URI를 Download Transport에 전달
- Update와 Cancel Execute를 Service에 연결
- Cancel 시 Transport 중단 후 Backend staging 데이터 제거
- Linux 파일 Backend와 Service/Backend/Adapter 테스트 추가
- Server에 OMA 공식 Firmware Update Object v1.2 DDF 적용

Package `/5/0/0` Push는 `5.01 Not Implemented`로 유지한다.
Linux 실행 앱은 실제 Download Transport가 없어 Package URI도 `5.01`을 반환한다.
Smoke test에서는 가짜 Transport로 Adapter 연결과 실패 결과 변환을 검증한다.

### 8.3 `/5` v1.2 기반 정리 완료

Reference client의 Registration에서 `</5>;ver=1.2,</5/0>`을 확인했다.
Embedded Leshan이 `/5/0/8`과 `/5/0/9`를 실제로 Read하고 다음 capability
API가 HTTP `200`을 반환하는 것을 확인했다.

```text
GET /api/devices/{endpoint}/firmware/capabilities
```

응답에서 Protocol Support `[0]`과 Delivery Method `0`을 확인했다.

### 8.4 Firmware Status API 완료

Spring Server가 등록된 Device의 Firmware Update 상태를 실제로 Read한다.

```text
GET /api/devices/{endpoint}/firmware/status
```

처리 흐름:

1. endpoint로 Leshan Registration 조회
2. State `/5/0/3` Read
3. Update Result `/5/0/5` Read
4. 두 값을 `FirmwareStatus`로 변환
5. HTTP JSON 응답

Reference client를 등록한 E2E 검증에서 HTTP `200`과 다음 응답을 확인했다.

```json
{
  "endpoint": "linux-reference-01",
  "state": 0,
  "updateResult": 0
}
```

Client 종료 후 `expired=false`인 정상 Deregistration도 확인했다.

### 8.5 Package URI OTA vertical slice 완료

완료한 흐름:

1. REST API가 Package URI를 `/5/0/1`로 Write
2. Linux libcoap Transport가 artifact를 Block2로 다운로드
3. Service가 firmware를 staging Backend에 기록
4. State와 Update Result를 Observe/Notify로 보고
5. REST API가 `/5/0/2` Update Execute
6. 설치 마커를 파일에 영속화
7. Client 재시작 후 Success 복구와 staging 정리

검증 결과:

- Invalid URI: State `0`, Update Result `7`
- 다운로드 완료: State `2`, Update Result `0`
- Update Execute: State `3`, Update Result `0`
- 재시작 복구: State `0`, Update Result `1`
- install marker와 staging 파일 제거 확인

플랫폼 남은 작업:

- artifact metadata/storage와 manifest 배포 계약
- 서명 형식과 device verification contract
- 영구 Device Registry와 credential 발급, rotation, revoke
- 보안 실패 Result 매핑과 상호운용 테스트

장치 개발자 통합 범위:

- 실제 hash/signature 검증
- secure key storage
- Bootloader anti-rollback
- boot confirmation과 rollback

### 8.6 DTLS-PSK secure client E2E 완료

구현 결과:

- Leshan Server가 CoAP `5683`과 CoAPS `5684` endpoint를 함께 제공
- endpoint, PSK identity, key를 SecurityStore에 연결
- Linux reference client가 Wakaama tinyDTLS transport 사용
- PSK identity와 key는 실행 환경에서 주입
- `/0` Server URI를 `coaps://127.0.0.1:5684`로 구성
- NoSec 경로는 smoke test 회귀 검증용으로 유지
- tinyDTLS 재핸드셰이크의 양수 성공 반환값 처리 수정

검증 결과:

- 올바른 PSK에서 Registration과 `/5/0/3`, `/5/0/5` Observe 성공
- 잘못된 PSK에서는 Client가 READY로 전환되지 않고 Server 등록도 거부
- DTLS 경로의 Firmware Status Read 성공
- Package URI Write와 State/Update Result Notify가 DTLS로 동작
- 40초 이후 재핸드셰이크와 firmware 다운로드 완료 확인

현재 경계:

- LwM2M control plane은 DTLS-PSK 적용 완료
- Device와 credential metadata 및 create/revoke/rotation은 PostgreSQL에 영속화
- 실제 PSK 값은 임시 환경변수 provider를 사용하며 제품 secret store는 미구현
- Artifact data plane은 아직 NoSec CoAP

### 8.7 PostgreSQL Device Registry와 PSK lifecycle 구현 완료

구현 결과:

- PostgreSQL 18, Spring JDBC, Flyway 연동
- `devices`와 `device_credentials` schema 및 migration 적용
- endpoint 중복과 공백 입력 방지
- Device 생성, 단건 조회, 목록 조회 API 구현
- PSK secret 원문 대신 `secret_reference`만 DB에 저장
- DB의 ACTIVE credential을 시작 시 Leshan SecurityStore에 로드
- 여러 Device credential을 하나의 서버 process에서 관리
- 실행 중 credential 생성, revoke, rotation 반영
- revoke와 rotation에서 기존 DTLS session 즉시 종료
- credential 이력을 `ACTIVE`, `ROTATED`, `REVOKED` 상태로 보존

Credential API:

```text
GET  /api/devices/{endpoint}/credentials/psk
POST /api/devices/{endpoint}/credentials/psk
POST /api/devices/{endpoint}/credentials/psk/revoke
POST /api/devices/{endpoint}/credentials/psk/rotate
```

## 9. 이후 작업 순서

1. Firmware artifact metadata와 storage adapter
2. Manifest 배포 계약과 보안 실패 Result mapping
3. 제품 secret store, credential provisioning, Admin 인증과 audit
4. 범용 telemetry ingestion 및 Observe/Notify
5. Device ownership과 사용자 model
6. Device Profile과 동적 Object model registry
7. OTA Campaign과 staged rollout
8. Admin UI
9. 독립적인 LwM2M 구현체와 상호운용성 검증

## 10. Build와 실행

### Reference Device Client

```bash
cmake -S clients/linux-reference -B /tmp/ota-linux-reference-client-build
cmake --build /tmp/ota-linux-reference-client-build -j2

/tmp/ota-linux-reference-client-build/ota_linux_reference_client_smoke

OTA_LWM2M_PSK_IDENTITY=linux-reference-01 \
OTA_LWM2M_PSK_KEY_HEX=00112233445566778899aabbccddeeff \
/tmp/ota-linux-reference-client-build/ota_linux_reference_client
```

### Embedded Server

```bash
cd server
mvn -o package

OTA_LWM2M_PSK_ENDPOINT=linux-reference-01 \
OTA_LWM2M_PSK_IDENTITY=linux-reference-01 \
OTA_LWM2M_PSK_KEY_HEX=00112233445566778899aabbccddeeff \
java -jar target/ota-server-0.0.1-SNAPSHOT.jar
```

Linux reference client는 CoAPS `5684`와 실행 환경의 DTLS-PSK를 사용한다.
Smoke test는 회귀 검증을 위해 NoSec CoAP `5683`을 사용한다.
문서의 PSK는 localhost 개발 검증 전용이며 제품 credential로 사용하지 않는다.

## 11. 현재 회귀 검증 기준

- Linux reference client와 smoke test build
- Spring Server package build
- Server의 CoAP `5683`, CoAPS `5684` listen
- NoSec smoke test Registration 응답
- 올바른 PSK에서 DTLS Registration과 READY 전환
- 잘못된 PSK에서 Registration 거부
- `/5/0/3`, `/5/0/5` DTLS Observe 연결
- Firmware Status API 응답
- Package URI 다운로드 후 State `2`, Update Result `0`
- staging file과 artifact의 `cmp` 일치
- 40초 이후 DTLS 재핸드셰이크
- Client 종료 후 정상 Deregistration

## 12. 새 세션 시작 지침

새 세션에서는 먼저 다음 파일을 읽는다.

- `ARCHITECTURE.md`
- `DEVELOPMENT_STATUS.md`
- `DEVICE_CLIENT_CONTRACT.md`
- `clients/device-integration-kit/README.md`
- `clients/device-integration-kit/include/firmware_download_transport.h`
- `clients/device-integration-kit/include/firmware_update_backend.h`
- `clients/device-integration-kit/include/firmware_update_service.h`
- `clients/device-integration-kit/src/firmware_update_service.c`
- `clients/device-integration-kit/adapters/wakaama/src/object_firmware.c`
- `clients/linux-reference/include/linux_firmware_update_backend.h`
- `clients/linux-reference/src/linux_firmware_update_backend.c`
- `clients/linux-reference/include/reference_client_app.hpp`
- `clients/linux-reference/src/reference_client_app.cpp`
- `clients/linux-reference/CMakeLists.txt`
- `experiments/wakaama/examples/client/common/object_firmware.c`
- `server/src/main/java/ota/platform/server/config/LeshanServerConfiguration.java`
- `server/src/main/java/ota/platform/server/controller/FirmwareController.java`
- `server/src/main/java/ota/platform/server/firmware/FirmwareCapabilities.java`
- `server/src/main/java/ota/platform/server/firmware/FirmwareStatus.java`
- `server/src/main/resources/models/firmware-update-v1_2.xml`

Milestone 1 Client foundation과 Milestone 2 Server foundation을 다시 구현하지 않는다.
Device Integration Kit의 Adapter, Service, Download Transport와 Backend 경계도
다시 설계하지 않는다.

Package URI OTA vertical slice는 완료됐다.
다음에는 PostgreSQL schema와 영구 Device Registry를 진행한다.
STM32의 실제 crypto, Flash, Bootloader와 anti-rollback 구현은
Device Integration Contract에 따라 장치 개발자가 통합한다.
`/25` 또는 Gateway-하위 장치 protocol 방향으로 돌아가지 않는다.