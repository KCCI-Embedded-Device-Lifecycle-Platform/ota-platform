# Software Architecture

Last updated: 2026-07-16

## 1. 문서 목적

이 문서는 Firmware OTA Platform의 전체 소프트웨어 구조와 구성요소 사이의
책임을 정의한다. 구현 진척도와 바로 다음 작업은 `DEVELOPMENT_STATUS.md`에서
관리한다. `DESIGN.md`는 UI 디자인 전용 문서이며 여기에서 대체하지 않는다.

문서에서 사용하는 상태는 다음과 같다.

- **현재**: 저장소에 코드가 있거나 end-to-end 실험으로 확인함
- **결정**: 기술 방향은 확정했지만 제품 코드가 아직 없음
- **계획**: 이후 milestone에서 구현할 후보
- **미정**: 구현 전에 별도 설계 결정이 필요함

## 2. 프로젝트 목표

중앙 서버가 Linux gateway를 LwM2M으로 관리하고, gateway가 자신과 하위 MCU의
상태를 수집하면서 firmware update lifecycle을 수행하는 작은 OTA 플랫폼을
만든다.

첫 통합 목표는 실제 STM32 없이 Fake STM32 Device를 사용해 다음 흐름을 끝까지
검증하는 것이다.

```text
배포 생성
-> gateway에 배포 전달
-> artifact 획득 및 검증
-> 하위 device update
-> health check
-> commit 또는 rollback
-> 결과를 중앙 서버에 저장
```

## 3. System Context

```text
                    HTTP/REST
 [Admin UI] <------------------------> [Central Server]
                                                |
                                                | LwM2M / CoAP
                                                | registration, control,
                                                | telemetry, OTA lifecycle
                                                v
                                      [C++ Gateway Agent]
                                         |            |
                           Local HTTP API |            | TCP custom protocol
                                         v            v
                                  [Qt Monitor]   [Fake STM32 Device]
                                                       |
                                                       | later replacement
                                                       v
                                                  [Real STM32]
```

현재 동작이 검증된 경계는 Leshan Demo Server와 Wakaama gateway 사이의
LwM2M/CoAP 통신뿐이다. 나머지는 목표 구조다.

## 4. Central Server

상태: **기술 결정 완료, 제품 코드 미구현**

기술 스택:

- Java 21
- Maven
- Spring Boot
- Eclipse Leshan embedded server

주요 책임:

- LwM2M gateway 등록과 lifetime 관리
- Gateway 및 하위 device 상태 모델 관리
- BMS telemetry 수집
- Firmware artifact metadata와 binary 관리
- Deployment 생성과 대상 선택
- Gateway에 update 작업 전달
- Update 진행 상태, 성공, 실패, rollback 결과 저장
- Admin API와 이후 Web UI 제공

Leshan Demo Server는 protocol 학습과 상호운용성 확인을 위한 실험 도구다. 최종
서버는 Demo JAR를 별도 프로세스로 중계하는 구조가 아니라, Spring Boot
애플리케이션 안에 Leshan을 library로 포함한다.

Persistence DB와 schema는 Java 서버 구현을 시작하기 전에 다시 확정한다. 이전
C# 실험의 EF Core/SQLite 모델을 현재 아키텍처의 확정 사항으로 간주하지 않는다.

## 5. Gateway Agent

상태: **Milestone 1 제품 실행 경로 구현 및 통합 검증 완료**

기술 스택:

- C++20 application
- Eclipse Wakaama C library
- CMake
- POSIX UDP
- Linux 우선, Raspberry Pi Linux 호환 목표

Gateway의 책임:

- 중앙 Leshan 서버에 LwM2M Client로 등록
- 표준 및 custom LwM2M Object 제공
- Gateway/BMS 상태와 telemetry 노출
- 서버 명령과 firmware update 요청 처리
- Artifact 획득 및 무결성 검증
- 하위 MCU 연결과 update protocol 수행
- Update state machine, commit, rollback 수행
- 중앙 서버에 진행 상태와 최종 결과 노출
- 현장 Qt Monitor용 local API 제공

초기 실행 모델은 하나의 process와 하나의 event loop다. 통신 경계가 실제로
복잡해지기 전에는 thread, message bus, plugin system을 도입하지 않는다.

### 5.1 현재 내부 모듈

```text
main.cpp
  -> GatewayApp
       -> gateway_client_context_t
       -> Wakaama context
       -> LwM2M Object array
       -> UDP socket and connections

object_bms.c
  -> Custom Object /33000

wakaama_hooks.c
  -> Wakaama C platform hooks
  -> UDP connection create/close adapter

standard_objects.h
  -> Wakaama example Object C API declarations

gateway_smoke.cpp
  -> 초기 통합 실험과 회귀 확인
```

### 5.2 LwM2M Object Model

현재 gateway가 구성한 Object:

| Object | 경로 | 용도 | 상태 |
|---|---|---|---|
| Security | `/0` | Leshan server URI와 security mode | 현재 |
| Server | `/1` | Short Server ID, lifetime, binding | 현재 |
| Device | `/3` | 표준 device 정보 | 현재 |
| BMS | `/33000` | 프로젝트 custom battery telemetry | 현재 |
| Firmware Update | `/5` | Gateway OTA lifecycle | 계획 |

현재 BMS Object는 Instance 0의 Resource 0에서 voltage `12.7`을 Float로 읽는다.
추가 BMS resource ID와 의미는 구현 전에 별도 Object schema로 문서화해야 한다.

Firmware Update Object `/5`를 gateway 자체 update에 사용할지, 하위 MCU update
작업을 표현하는 custom Object를 추가할지는 **미정**이다. 표준 Object와 custom
Object의 역할을 먼저 분리한 뒤 구현한다.

### 5.3 Gateway 자원 소유권

`GatewayApp`이 C 자원의 수명을 감싸는 RAII 경계가 된다.

| 자원 | 소유자 | 정리 방식 |
|---|---|---|
| UDP socket fd | `GatewayApp` | `close()` |
| `lwm2m_context_t *` | `GatewayApp` | `lwm2m_close()` |
| LwM2M Object 포인터 | `GatewayApp` | Object별 cleanup 함수 |
| UDP connection list | `gateway_client_context_t`를 통한 `GatewayApp` | Wakaama close hook, 필요 시 `lwm2m_connection_free()` |
| `securityObjectP` | 소유하지 않는 alias | Object 배열의 `/0`을 가리킴 |
| server host/port 문자열 | 소유하지 않는 설정값 | 현재 string literal이므로 free하지 않음 |

Wakaama context는 configure 이후 Object 포인터를 사용하지만 Object 자체를
GatewayApp 대신 소유하지 않는다. 따라서 shutdown 때 Wakaama context 사용을
끝낸 후 Object를 해제해야 한다.

현재 cleanup 순서:

1. event loop 정지
2. `lwm2m_close()`로 Wakaama context 종료
3. 남은 connection list 정리
4. LwM2M Object 정리
5. UDP socket 닫기

각 단계는 null pointer와 `-1` fd에서도 안전해야 하며, 정리 후 멤버를 다시 빈
상태로 바꿔 중복 해제를 막는다.

## 6. Gateway Event Loop

상태: **제품 반복 loop 및 signal 기반 정상 종료 검증 완료**

현재 단일 반복 흐름:

```text
lwm2m_step(context, &timeout)
-> Wakaama가 계산한 timeout까지 socket 대기
-> recvfrom()
-> sender address로 Wakaama connection 검색
-> lwm2m_handle_packet(context, buffer, size, session)
-> 종료 요청이 없으면 반복
```

Registration의 핵심 순서:

```text
STATE_INITIAL
-> lwm2m_step()
-> Registration request 송신
-> STATE_REGISTERING
-> UDP response 수신
-> lwm2m_handle_packet()
-> 다음 lwm2m_step()
-> STATE_READY
```

제품 `GatewayApp`에서 Registration 응답을 connection과 매칭해
`lwm2m_handle_packet()`에 전달하고 다음 `lwm2m_step()`에서 `STATE_READY`로
전환하는 것을 확인했다. READY 이후에도 같은 loop가 Leshan 요청을 처리한다.
SIGINT와 SIGTERM은 종료 flag만 설정하며 실제 deregistration과 자원 정리는
`run()` 반환 후 `GatewayApp` 소멸자가 수행한다.

## 7. Gateway와 하위 MCU

상태: **계획, 미구현**

초기에는 실제 STM32 대신 Linux에서 실행되는 Fake STM32 Device를 사용한다.

```text
Gateway Agent: TCP server
Fake STM32:    TCP client
```

계획된 gateway 책임:

- Device 접속과 session 관리
- Hello와 heartbeat 처리
- Device 정보와 update 가능 상태 조회
- Firmware chunk 전송
- Verify, boot pending, health check 처리
- Commit 또는 rollback 명령

기존 초안에는 SOF, version, message type, sequence, device ID, payload length,
payload, CRC16으로 구성한 binary frame이 있다. 이 frame은 아직 구현하지 않았고,
구현 전 `docs/device_protocol.md`에서 byte order, 최대 payload, timeout, retry,
중복 sequence 처리까지 확정해야 한다.

## 8. Firmware Update Flow

상태: **목표 흐름만 결정, protocol mapping 미정**

```text
IDLE
-> DEPLOYMENT_PENDING
-> ACQUIRE_ARTIFACT
-> VERIFY_ARTIFACT
-> CHECK_DEVICE_STATUS
-> WAIT_DEVICE_READY
-> PREPARE_UPDATE
-> TRANSFER_FIRMWARE
-> VERIFY_IMAGE
-> BOOT_NEW_FIRMWARE
-> HEALTH_CHECK
-> COMMIT or ROLLBACK
-> REPORT_RESULT
-> IDLE
```

다음 사항은 구현 전에 결정해야 한다.

- Central Server에서 Gateway로 artifact를 전달하는 방식
- LwM2M Firmware Update Object `/5`의 Push/Pull 사용 방식
- Gateway 자체 firmware와 하위 MCU firmware의 구분
- Artifact manifest와 SHA-256 검증 규칙
- 전원 중단 후 재개 지점과 영속화 범위
- Retry, timeout, rollback policy

## 9. Qt Local Monitor

상태: **계획, Gateway 핵심 흐름 이후 구현**

Qt Monitor는 중앙 배포 관리 UI가 아니라 현장 gateway 진단 도구다.

계획된 표시 범위:

- Gateway와 중앙 서버 연결 상태
- 연결된 MCU 목록과 heartbeat
- BMS telemetry와 fault
- OTA 단계와 진행률
- Update, commit, rollback log
- 제한된 debug command

Gateway Agent가 localhost HTTP/JSON API를 제공하는 방향을 우선 검토한다. API와
동시성 모델은 Gateway event loop가 안정된 뒤 확정한다.

## 10. 통신 경계

| 구간 | Protocol | 현재 보안 | 목표 상태 |
|---|---|---|---|
| Admin UI <-> Central Server | HTTP/REST | 미정 | 인증/권한 포함 |
| Central Server <-> Gateway | LwM2M over CoAP/UDP | NoSec | DTLS credential 적용 |
| Gateway <-> Fake/Real STM32 | Custom binary over TCP | 미정 | 무결성, timeout, retry 정의 |
| Qt Monitor <-> Gateway | localhost HTTP/JSON 후보 | localhost only | 접근 제어 검토 |

NoSec는 localhost 개발 실험에만 사용한다. 외부 network 배포의 기본값으로
사용하지 않는다.

## 11. Milestones

### Milestone 1: LwM2M Gateway Foundation

상태: **완료 (2026-07-16)**

- `GatewayApp` RAII 수명 관리 완료
- Leshan Registration과 `STATE_READY` 전환 확인
- 반복 event loop 구현
- `/33000/0/0` Read를 실제 Leshan 요청으로 확인
- signal 기반 정상 shutdown과 Deregistration 확인

### Milestone 2: Embedded Leshan Server

- Java 21/Maven/Spring Boot project 생성
- Leshan server lifecycle을 Spring에 통합
- Gateway registration과 telemetry 저장
- 최소 관리 API

### Milestone 3: Simulated Device OTA

- Device binary protocol 문서와 encoder/decoder test
- Fake STM32 연결
- Artifact와 update state machine
- Success, failure, busy defer, rollback 시나리오

### Milestone 4: Operations UI

- Server Admin UI
- Gateway local monitor API
- Qt Gateway/Battery Monitor

## 12. 아키텍처 원칙

- Central Server와 Gateway Agent는 독립 실행 파일로 유지한다.
- Leshan은 서버 애플리케이션 내부 library이며 별도 JVM 중계 계층이 아니다.
- Wakaama C API는 작은 adapter와 `GatewayApp` 수명 경계 뒤에 둔다.
- 표준 LwM2M Object를 우선 사용하고 프로젝트 고유 데이터만 custom Object로 둔다.
- Protocol과 binary format은 코드 전에 문서와 단위 테스트로 고정한다.
- 실제 STM32 구현 전 Fake Device로 정상, 실패, rollback을 검증한다.
- 처음에는 단일 process와 단일 event loop로 시작한다.
- 학습 단계에서는 함수와 객체를 작은 단위로 직접 작성하고 매번 빌드한다.
