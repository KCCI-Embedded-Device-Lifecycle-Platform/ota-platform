# Software Architecture

Last updated: 2026-07-20

## 1. 문서 목적

이 문서는 Firmware OTA Platform의 제품 구조와 구성요소의 책임을 정의한다.
현재 구현 진척도와 바로 다음 작업은 `DEVELOPMENT_STATUS.md`에서 관리한다.
`DESIGN.md`는 UI 디자인 전용 문서다.
Device Client 통합 계약과 플랫폼/장치 개발자의 상세 책임은
[`DEVICE_CLIENT_CONTRACT.md`](DEVICE_CLIENT_CONTRACT.md)에서 정의한다.

문서에서 사용하는 상태는 다음과 같다.

- **현재**: 저장소에 구현되어 있거나 통합 실험으로 확인함
- **결정**: 제품 방향은 확정했지만 구현이 끝나지 않음
- **계획**: 이후 milestone에서 구현할 항목
- **선택 사항**: 실제 요구가 생길 때만 추가함

## 2. 제품 목표와 사용 시나리오

플랫폼은 인터넷에 직접 연결 가능한 MCU 및 Embedded Linux 장치를 표준
LwM2M으로 관제하고, telemetry 수집과 firmware OTA lifecycle을 관리한다.

목표 사용 시나리오는 다음과 같다.

1. 장치 개발자가 제품에 표준 LwM2M Client와 장치별 안전 업데이트 기능을
   통합한다.
2. 사용자는 서버에서 자신이 소유한 장치를 등록하거나 삭제한다.
3. 장치는 고유 endpoint name과 credential로 중앙 LwM2M Server에 직접
   연결하고 등록한다.
4. 같은 제품의 장치 한 대를 새로 추가할 때 서버나 Client 코드를 수정하거나
   다시 배포하지 않는다. 등록과 credential provisioning만 수행한다.
5. 사용자는 UI에서 장치 telemetry를 확인하고, 한 대 또는 여러 대에 firmware
   update를 배포한다.

새로운 제품 모델이 표준 OMA/IPSO Object만 사용한다면 서버 코드 변경 없이
등록할 수 있어야 한다. 새로운 custom Object가 필요한 경우에도 향후 Object
model과 UI metadata를 등록하는 것으로 끝내며 Java 서버를 다시 빌드하지 않는
구조를 목표로 한다.

## 3. 핵심 아키텍처 결정

### 3.1 Direct LwM2M을 기본 경로로 사용

인터넷에 직접 연결 가능한 장치는 중앙 서버에 직접 LwM2M Client로 등록한다.
장치와 서버 사이에 항상 거치는 Gateway를 두지 않는다.

```text
 [Admin UI]
      |
      | HTTP/REST, later WebSocket/SSE
      v
 [Spring Boot OTA Server]
      |-- Embedded Leshan LwM2M Server
      |-- Device Registry / Telemetry / OTA Campaign
      |-- PostgreSQL ---------------- structured data
      |-- Object Storage ------------ firmware binary
      |
      | LwM2M over CoAP/DTLS
      |
      +-------------------+-------------------+
      v                   v                   v
 [MCU Device]     [Embedded Linux]     [Another Vendor Device]
  /3 /5 objects     /3 /5 objects       conforming LwM2M Client
```

이 구조는 개인 custom protocol로 돌아가는 것이 아니다. LwM2M Client와 LwM2M
Server가 직접 통신하는 표준 구조를 사용한다.

### 3.2 Gateway는 선택적 호환 계층

Gateway는 다음과 같은 실제 요구가 생길 때 별도 구성요소로 추가한다.

- BLE, CAN, RS-485 등 비 IP 장치
- LwM2M Client를 탑재할 수 없는 레거시 장치
- 현장망 단절 시 로컬 제어가 필요한 장치
- 여러 장치의 데이터 집계가 반드시 필요한 제한된 uplink

현재 대상으로 삼는 직접 IP 장치를 위해 LwM2M Gateway Object `/25`, prefixed
Object routing, 하위 장치 전용 protocol을 구현하지 않는다.

### 3.3 장치가 곧 LwM2M endpoint

제품에서 관리하는 단위는 Gateway가 아니라 개별 Device다. 각 Device는 다음
정보로 식별되고 소유권과 연결된다.

```text
User
  -> owns Device
       -> endpointName
       -> credential
       -> deviceProfile
       -> reported LwM2M Objects
```

Gateway Object Instance와 별도의 `deviceId`를 연결하는 계층은 기본 구조에
존재하지 않는다.

## 4. Central Server

상태: **기본 LwM2M 서버와 BMS Read API 구현 완료, 제품 기능 확장 중**

기술 스택:

- Java 21
- Maven
- Spring Boot 4.1.0
- Eclipse Leshan 2.0.0-M18 embedded server
- PostgreSQL 18 계획

현재 구현:

- Spring이 `LeshanServerConfiguration`을 component scan으로 발견
- `LeshanServer`를 Spring Bean으로 생성하고 시작 및 종료 lifecycle 관리
- 기본 LwM2M Object model과 `models/bms.xml` 로드
- Client 등록, 갱신, 해제 event logging
- 등록된 endpoint에 `/33000/0/0` Read 전송
- 읽은 BMS voltage를 process memory에 endpoint별 최신값으로 보관
- HTTP API로 현재 Read와 최신값 조회

서버의 장치 중심 이름은 `DeviceRegistrationListener`, `DeviceController`,
`/api/devices`를 사용한다. HTTP `502 Bad Gateway` 상태 이름은 장치 Gateway
도메인과 무관한 표준 HTTP 용어다.

목표 책임:

- 사용자, 장치 소유권, endpoint, credential 관리
- LwM2M registration과 session 관리
- Object model 및 Device Profile 관리
- telemetry ingestion, 최신 상태, 이력 저장
- firmware artifact metadata와 binary 위치 관리
- OTA campaign 생성, 대상 선택, 동시성 및 rollout policy 관리
- `/5` 명령 전송과 진행 상태 및 결과 추적
- Admin API와 Web UI 제공

Leshan은 Spring Boot 애플리케이션 내부 library다. 별도 Demo Server를 제품
서버 앞에 두거나 custom protocol로 변환하지 않는다.

## 5. Device Client와 Device SDK

### 5.1 구현 정책

서버는 특정 Client SDK에 종속되지 않는다. 표준에 맞는 여러 Client가 같은
서버와 상호운용되어야 한다.

- 현재 Linux reference client: C++20 + Eclipse Wakaama C library
- Zephyr 기반 MCU 후보: Zephyr native LwM2M engine
- 다른 MCU/Linux 제품: 자원, 보안, 라이선스, `/5` 완성도에 맞는 conforming
  Client 선택

현재 저장소의 `clients/linux-reference/`는 BMS 장치 하나를 표현하는 직접 연결
Linux reference client다. 서버 상호운용과 protocol lifecycle 검증을 위한
reference implementation이며, 범용 MCU SDK나 STM32 배포 binary가 아니다.

제품 목표 Profile은 LwM2M `1.1`과 Firmware Update Object `/5` version `1.2`다.
현재 Linux reference client는 `/5` version `1.0` Read scaffold이므로 v1.2로
확장해야 한다.

### 5.2 현재 reference client

```text
main.cpp
  -> ReferenceClientApp
       -> wakaama_client_context_t
       -> Wakaama context
       -> LwM2M Object array
       -> UDP socket and server connections

object_bms.c
  -> BMS custom Object /33000

wakaama_hooks.c
  -> Wakaama POSIX UDP platform adapter

reference_client_smoke.cpp
  -> integration and resource-lifetime checks
```

현재 구성한 Object:

| Object | 경로 | 용도 | 상태 |
|---|---|---|---|
| Security | `/0` | Server URI와 security 설정 | 현재 |
| Server | `/1` | Server ID, lifetime, binding | 현재 |
| Device | `/3` | 제조사, 모델, firmware version 등 | 현재 |
| Firmware Update | `/5` | 표준 OTA control과 상태 | Read wiring scaffold |
| BMS | `/33000` | reference BMS telemetry | 현재 |

현재 BMS Object는 `/33000/0/0`에서 voltage `12.7`을 Float로 읽는다.
`/33000`은 하위 장치를 대신 표현하는 Object가 아니라 이 LwM2M endpoint 자체가
제공하는 custom Object다.

### 5.3 자원 소유권과 event loop

현재 `ReferenceClientApp`은 다음 C 자원의 RAII 경계다.

| 자원 | 현재 소유자 | 정리 방식 |
|---|---|---|
| UDP socket fd | `ReferenceClientApp` | `close()` |
| `lwm2m_context_t *` | `ReferenceClientApp` | `lwm2m_close()` |
| LwM2M Object 포인터 | `ReferenceClientApp` | Object별 cleanup 함수 |
| Server connection list | client context를 통한 `ReferenceClientApp` | Wakaama close hook/free |
| `securityObjectP` | 소유하지 않는 alias | Object 배열의 `/0`을 참조 |

`connectionList`는 하위 Device 목록이 아니라 이 Client가 연결한 LwM2M Server
transport session 목록이다.

현재 event loop:

```text
lwm2m_step()
-> Wakaama timeout까지 UDP socket 대기
-> recvfrom()
-> sender에 해당하는 server connection 검색
-> lwm2m_handle_packet()
-> 반복
```

Registration, READY 전환, Read 요청 처리, signal 기반 종료와 Deregistration가
검증됐다.

### 5.4 Device Client 배포 및 책임 경계

플랫폼은 특정 STM32 제품의 완성 firmware를 범용 산출물로 제공하지 않는다.
대신 장치 개발자가 제품 firmware에 통합할 수 있는 Device Integration Kit를
제공한다.

| 플랫폼이 제공할 책임 | 장치 개발자가 제공할 책임 |
|---|---|
| 지원 LwM2M/Object version과 Resource 계약 | 대상 MCU, RTOS, network stack에 Client 통합 |
| endpoint/credential provisioning 규격 | credential의 안전한 저장과 device identity 연결 |
| `/3`, `/5`, custom telemetry model | 실제 장치 정보와 sensor/actuator callback 연결 |
| `/5` 상태 전이와 Update Result mapping | firmware의 flash 저장, hash/signature 검증 |
| update backend callback 경계와 Linux reference | bootloader, A/B slot, reboot, confirmation, rollback |
| Leshan 상호운용 및 실패 시나리오 test | 실제 제품 firmware build와 hardware 검증 |

지원할 STM32 조합은 MCU family만으로 정의하지 않는다. RTOS, network stack,
LwM2M engine, bootloader, flash layout을 하나의 지원 profile로 확정한 뒤 해당
profile용 source component와 sample, 설정 규격, test를 배포한다.

## 6. Device onboarding과 Object model

장치 추가는 세 경우를 구분한다.

| 상황 | 필요한 작업 | 서버 코드 변경 |
|---|---|---|
| 기존 제품의 새 장치 한 대 | endpoint/credential 발급과 사용자 소유권 연결 | 없음 |
| 표준 Object를 사용하는 새 제품 | Device Profile 등록 | 없음 |
| 새로운 custom Object를 쓰는 새 제품 | DDF/XML과 UI metadata 등록 | 목표: 없음 |
| LwM2M을 지원하지 않는 레거시 장치 | 별도 adapter 또는 optional Gateway | adapter 구현 필요 |

현재 `bms.xml`은 classpath에서 정적으로 로드한다. 향후에는 Object model
registry와 관리 API를 만들어 재빌드 없이 모델을 추가할 수 있어야 한다.

## 7. Telemetry flow

현재 단계:

```text
HTTP 요청
-> 서버가 Device의 /33000/0/0 Read
-> 응답 decode
-> BmsTelemetryStore의 endpoint별 최신값 갱신
-> HTTP 응답
```

제품 목표:

```text
Device Observe/Notify 또는 LwM2M Send
-> validation / normalization
-> 최신값 cache 갱신
-> PostgreSQL history 저장
-> UI에 실시간 event 전달
```

매 화면 새로고침마다 Device에 Read를 보내지 않는다. 실시간 화면과 영속 이력은
같은 수신 event에서 파생하되, 높은 빈도의 telemetry가 LwM2M 처리 thread나
UI 전달을 막지 않도록 저장 작업을 분리하고 batch 정책을 둘 수 있다.

서버는 BMS, Motor, Sensor별 Java Controller를 계속 추가하는 구조를 목표로 하지
않는다. Object ID, Instance ID, Resource ID, timestamp, typed value를 기준으로
범용 수집 경로를 만들고, Device Profile metadata로 UI 의미를 제공한다.

## 8. Firmware OTA

### 8.1 두 상태 머신의 책임

LwM2M Client를 탑재해도 실제 안전 업데이트 기능은 장치에 필요하다.

| LwM2M `/5`와 Client library | Device application/bootloader |
|---|---|
| Package 또는 Package URI 수신 | 임시 영역 또는 flash에 기록 |
| 표준 State와 Update Result 보고 | hash와 signature 검증 |
| Update Execute 수신 | A/B slot 전환 또는 설치 |
| CoAP/DTLS, block transfer, retry | reboot, boot confirmation, rollback |

Custom Device-Gateway protocol을 만들면 오른쪽 기능은 그대로 필요하고 왼쪽의
관리 protocol까지 별도로 구현해야 한다.

### 8.2 목표 Pull OTA flow

```text
관리자가 firmware artifact 업로드
-> Object Storage에 binary 저장
-> PostgreSQL에 version/hash/signature/location metadata 저장
-> OTA Campaign과 대상 Device 선택
-> 서버가 /5/0/1 Package URI Write
-> Device가 artifact를 직접 다운로드
-> Device가 hash/signature 검증 후 /5 State=Downloaded 보고
-> 서버가 /5/0/2 Update Execute
-> Device가 설치 및 reboot
-> boot confirmation 또는 rollback
-> /5/0/5 Update Result와 새 /3 firmware version 수집
```

현재 Wakaama source에는 예제 `object_firmware.c`가 있지만 실제 binary 저장,
URI 다운로드, 검증, 재부팅, 상태 영속화를 구현하지 않는 demonstration
skeleton이다. 제품 코드는 이 예제를 그대로 완료 구현으로 간주하지 않고,
표준 Resource callback과 장치별 update backend의 경계를 명시적으로 만든다.

첫 통합은 Linux의 임시 firmware 저장 영역과 simulated install/reboot로 `/5`
전체 protocol 흐름을 검증한다. 실제 MCU에서는 같은 관리 흐름에 flash driver와
bootloader callback을 연결한다.

## 9. Persistence와 artifact storage

### 9.1 PostgreSQL

PostgreSQL에는 query와 transaction이 필요한 구조화 데이터를 저장한다.

- User와 Device ownership
- Device, endpoint, credential metadata
- Device Profile과 Object model metadata
- Telemetry sample 및 latest state
- Firmware artifact metadata, hash, signature, version
- OTA Campaign, Target, 상태 전이와 audit log

### 9.2 Object Storage

실제 `.bin`, `.hex`, `.swu` 같은 firmware binary는 Object Storage에
저장한다. PostgreSQL에는 binary 자체가 아니라 위치와 검증 metadata를 둔다.

초기 개발에서는 local filesystem으로 시작할 수 있다. 제품화 시 MinIO 또는
S3-compatible storage로 교체한다. CDN은 장치 수와 지역이 늘어날 때 붙이는
선택 사항이며 현재 milestone에는 포함하지 않는다.

## 10. Security와 신뢰성

현재 NoSec `coap://`는 localhost 개발 실험에만 사용한다.

제품 적용 전에 필요한 항목:

- Device별 DTLS PSK, RPK 또는 certificate provisioning
- credential rotation과 revoke
- firmware manifest의 hash와 digital signature 검증
- anti-rollback version policy
- update state의 전원 중단 후 복구
- retry, timeout, staged rollout, 동시 업데이트 제한
- OTA 작업과 소유권 변경의 audit log

## 11. Milestones

### Milestone 1: Direct LwM2M Client Foundation

상태: **완료, Linux reference client foundation으로 정리**

- Wakaama Client 초기화와 Object 수명 관리
- Leshan Registration과 READY 전환
- 반복 UDP event loop
- `/3`, `/33000/0/0` Read
- 정상 Deregistration

### Milestone 2: Embedded Leshan Server Foundation

상태: **기본 흐름 완료**

- Spring Boot application에 Leshan lifecycle 통합
- BMS Object model 로드
- registration listener
- BMS Read API와 in-memory latest store

### Milestone 3: Direct Device Firmware Update

상태: **다음 작업**

- 표준 `/5` Object를 reference client에 연결 완료
- State와 Update Result local Read 검증 완료
- Embedded Leshan을 통한 State와 Update Result Read 검증
- firmware storage/verification/install callback 경계 설계
- Package URI download와 simulated install
- Execute, reboot simulation, success/failure/result end-to-end 검증

### Milestone 4: Persistence and Generic Device Model

- PostgreSQL schema와 migration
- Object Storage adapter
- Device/ownership/credential model
- 범용 telemetry ingestion과 history
- 동적 Object model 및 Device Profile 관리

### Milestone 5: Fleet Operations

- OTA Campaign과 staged rollout
- 동시성, retry, pause/cancel policy
- Admin UI
- 여러 독립 LwM2M Client 구현과 상호운용성 검증
- optional Edge Gateway는 실제 비 IP 요구가 생긴 경우에만 별도 milestone으로 추가

## 12. 아키텍처 원칙

- 직접 연결 가능한 Device는 LwM2M Server에 직접 연결한다.
- 표준 LwM2M Object를 우선하고 제품 고유 데이터만 custom Object로 둔다.
- 장치 한 대 추가는 provisioning 작업이며 코드 변경 작업이 아니다.
- Client SDK는 교체 가능하고 서버는 특정 SDK에 종속되지 않는다.
- firmware 전달과 update 제어를 분리한다.
- DB에는 구조화된 상태를, Object Storage에는 firmware binary를 저장한다.
- 실제 flash/bootloader 구현 전 simulated backend로 정상, 실패, rollback을 검증한다.
- 학습 단계에서는 작은 단위로 구현하고 매 단계 build와 실행 결과를 확인한다.
