# Device Client Integration Contract

## 1. 목적

이 문서는 OTA Platform과 실제 Device firmware 사이의 통합 계약을 정의한다.

플랫폼은 LwM2M protocol, Object model, provisioning 규격과 검증 환경을 제공한다.
장치 firmware는 해당 계약을 실제 MCU, network stack, flash, bootloader에 연결한다.

현재 `clients/linux-reference/`는 서버와 protocol 흐름을 검증하기 위한
reference implementation이며 STM32 제품용 배포 binary가 아니다.

## 2. 지원 LwM2M Profile

### 2.1 Protocol

- LwM2M protocol version: `1.1`
- Transport binding: UDP (`U`)
- 개발 환경: CoAP/UDP NoSec
- 제품 환경: CoAP/DTLS와 장치별 credential 필수
- OTA 전달 방식: Package URI를 사용하는 Pull 방식

NoSec는 localhost 통합 시험에만 사용한다.
제품용 DTLS security mode는 STM32 통합 전에 별도로 확정한다.

### 2.2 Object 범위

| Object | 경로 | 플랫폼 요구 |
|---|---|---|
| Security | `/0` | 필수 |
| Server | `/1` | 필수 |
| Device | `/3` | 필수 |
| Firmware Update v1.2 | `/5` | OTA 지원 장치에 필수 |
| BMS | `/33000` | BMS 제품 profile에서만 사용 |

`/33000`은 모든 Device가 구현해야 하는 공통 Object가 아니다.
제품별 telemetry Object는 Device Profile에서 정의한다.

### 2.3 Firmware Update v1.2 Resource

| Resource | 경로 | 플랫폼 요구 |
|---|---|---|
| Package | `/5/0/0` | 표준 필수, 현재 Pull 방식에서는 사용하지 않음 |
| Package URI | `/5/0/1` | 필수 |
| Update | `/5/0/2` | 필수 |
| State | `/5/0/3` | 필수 |
| Update Result | `/5/0/5` | 필수 |
| Protocol Support | `/5/0/8` | 필수 |
| Delivery Method | `/5/0/9` | 필수, Pull only (`0`) |
| Cancel | `/5/0/10` | 플랫폼 필수 |
| Severity | `/5/0/11` | 플랫폼 필수 |
| Maximum Defer Period | `/5/0/13` | 플랫폼 필수 |
| Automatic Upgrade | `/5/0/14` | 지원, 기본값 `false` |

## 3. Device Identity와 Credential

### 3.1 Endpoint Name

- 물리 Device마다 고유해야 한다.
- reboot와 firmware update 이후에도 유지되어야 한다.
- 다른 Device에 재사용하지 않는다.
- 사용자 표시 이름이나 비밀값으로 사용하지 않는다.
- `linux-reference-01`은 개발용 test identity다.

### 3.2 Credential

- 제품 환경에서는 Device마다 독립된 credential을 사용한다.
- 여러 Device가 하나의 credential을 공유하지 않는다.
- 플랫폼은 credential의 등록, rotation, revoke 규격을 제공한다.
- 장치 firmware는 credential을 보호된 저장소에서 읽어 `/0`에 연결한다.
- credential과 secret을 source code나 log에 노출하지 않는다.

### 3.3 Provisioning 흐름

```text
endpointName과 credential 발급
-> Server Device Registry에 등록
-> Device의 보호된 저장소에 기록
-> Device가 DTLS로 LwM2M Server에 등록
-> Server가 인증 결과를 Device identity와 연결
```

현재 Linux reference client는 NoSec와 고정 endpoint만 사용한다.
Device Registry, credential provisioning, DTLS는 아직 구현되지 않았다.

## 4. Firmware Update 책임 경계

Firmware Update는 다음 세 계층으로 분리한다.

```text
LwM2M `/5` Adapter
-> Firmware Update Service
-> Device Update Backend
```

| 계층 | 책임 | 담당 |
|---|---|---|
| `/5` Adapter | Package URI, Update, Cancel 수신과 Resource 노출 | 플랫폼 제공 |
| Download Transport | URI 다운로드, Service에 chunk 전달, network 취소 | 플랫폼 인터페이스 / 장치 구현 |
| Update Service | 상태 전이, offset 관리, 오류 변환 | 플랫폼 제공 |
| Update Backend | flash 기록, 검증, install, reboot, rollback | 플랫폼 인터페이스 / 장치 구현 |

Device Update Backend는 LwM2M message를 직접 만들지 않는다.
처리 결과만 Update Service에 반환하고, /5 Adapter가 표준 State와 Update Result로 Server에 보고한다.
현재 Wakaama Adapter는
`clients/device-integration-kit/adapters/wakaama/`에 위치한다.
State, Update Result와 정책 Resource, Package URI 전달,
Update와 Cancel Execute를 Service에 연결한다.
실제 Download Transport와 Artifact 보안 검증은 아직 구현되지 않았다.

## 5. Firmware Update 상태 전이

### 5.1 State

| 값 | 상태 | 의미 |
|---|---|---|
| `0` | Idle | update 작업 없음 |
| `1` | Downloading | firmware 수신 중 |
| `2` | Downloaded | 검증된 firmware 준비 완료 |
| `3` | Updating | 설치 또는 reboot 진행 중 |

### 5.2 전이 규칙

| Event | 이전 State | 다음 State | Update Result |
|---|---|---|---|
| Package URI 수신 | Idle | Downloading | Initial (`0`) |
| download와 검증 성공 | Downloading | Downloaded | Initial (`0`) |
| download 또는 검증 실패 | Downloading | Idle | 실패 원인에 맞는 값 |
| Update Execute 후 연기 | Downloaded | Downloaded | Deferred (`11`) |
| 연기 승인 또는 제한시간 만료 | Downloaded | Updating | Initial (`0`) |
| Update Execute 후 즉시 설치 | Downloaded | Updating | Initial (`0`) |
| Cancel Execute | Downloading 또는 Downloaded | Idle | Cancelled (`10`) |
| reboot 후 새 firmware 확인 | Updating | Idle | Success (`1`) |
| 설치 또는 boot 실패 | Updating | Idle | 실패 원인에 맞는 값 |

Update Execute는 State가 Downloaded일 때만 허용한다.
Cancel은 Downloading 또는 Downloaded 상태에서만 허용한다.
Updating 상태에서 Cancel을 받으면 Method Not Allowed로 거부한다.

Cancel 성공 시 내려받던 데이터와 저장된 firmware package를 제거한다.
Deferred 상태에서는 설치를 시작하지 않고 State를 Downloaded로 유지한다.
Maximum Defer Period가 만료되면 설치를 시작한다.

Severity가 Critical (`0`)이면 연기를 허용하지 않는다.
Mandatory (`1`) 또는 Optional (`2`)은 Maximum Defer Period 안에서 연기할 수 있다.
Maximum Defer Period가 `0`이면 연기를 허용하지 않는다.

Server는 `/5/0/3` State와 `/5/0/5` Update Result를 Observe한다.
Client는 값 변경을 알리고, Observe 관계가 설정된 경우 Notify가 전송된다.
Updating 상태는 reboot 전에 영속화하고, boot 후 성공 또는 rollback 결과를 복구한다.
성공이 확인된 뒤 `/3/0/3` Firmware Version을 새 version으로 갱신한다.

## 6. Update Result Mapping

| 내부 결과 | `/5/0/5` | 의미 |
|---|---:|---|
| Initial | `0` | 작업 시작 전 또는 새 작업 시작 |
| Success | `1` | firmware update 성공 |
| No Storage | `2` | firmware 저장 공간 부족 |
| Out of Memory | `3` | download 중 RAM 부족 |
| Connection Lost | `4` | download 중 연결 끊김 |
| Integrity Failure | `5` | hash 또는 signature 검증 실패 |
| Unsupported Package | `6` | 지원하지 않는 package 형식 |
| Invalid URI | `7` | 잘못된 Package URI |
| Update Failed | `8` | 설치, boot 또는 rollback 처리 실패 |
| Unsupported Protocol | `9` | 지원하지 않는 URI protocol |
| Cancelled | `10` | firmware update 취소 성공 |
| Deferred | `11` | firmware 설치 연기 |

Update Service는 가능한 경우 구체적인 실패값을 사용한다.
다른 값으로 표현할 수 없는 실패만 Update Failed (`8`)로 보고한다.

Device Update Backend는 숫자값을 직접 반환하지 않는다.
의미가 있는 내부 결과를 반환하고 Update Service가 `/5/0/5` 값으로 변환한다.

Firmware Update Object version `1.2`를 사용하므로
Update Result `0`부터 `11`까지 지원한다.

## 7. Device Update Backend 계약

| Operation | 호출 시점 | 책임 |
|---|---|---|
| `prepare` | download 시작 전 | 저장 공간 확인과 staging 영역 초기화 |
| `writeChunk` | firmware chunk 수신 시 | 전체 image를 RAM에 올리지 않고 순차 저장 |
| `finishDownload` | 마지막 chunk 저장 후 | hash, signature와 package 형식 검증 |
| `install` | Update Execute 승인 후 | image를 boot 대상으로 설정하고 reboot 요청 |
| `cancel` | Cancel Execute 수신 시 | 진행 작업 중단과 저장된 package 제거 |
| `recoverAfterBoot` | Device 시작 시 | 새 image 성공, 실패 또는 rollback 결과 복구 |

각 Operation은 성공 또는 6절의 의미 있는 내부 실패 결과를 반환한다.
Backend는 LwM2M State와 Update Result를 직접 변경하지 않는다.

`install` 성공은 update 완료를 의미하지 않는다.
새 firmware의 boot가 확인된 뒤에만 Update Result를 Success (`1`)로 보고한다.

Device Backend는 전원 중단 후에도 update 상태를 복구할 수 있어야 한다.
Linux reference backend는 임시 파일과 simulated reboot로 같은 계약을 검증한다.

## 8. Firmware Artifact 계약

Firmware Artifact는 manifest와 firmware image로 구성한다.

| Manifest Field | 의미 |
|---|---|
| `deviceProfile` | 설치 가능한 제품 종류 |
| `hardwareRevision` | 호환되는 hardware revision |
| `firmwareVersion` | 설치 후 `/3/0/3`에 보고할 version |
| `securityVersion` | anti-rollback 비교용 단조 증가 값 |
| `packageType` | binary 또는 bootloader package 형식 |
| `imageSize` | firmware image 크기 |
| `imageSha256` | firmware image hash |
| `signingKeyId` | 검증에 사용할 public key 식별자 |
| `signature` | manifest와 image hash에 대한 전자서명 |

signature는 `signature` 필드를 제외한 manifest 전체를 보호한다.
manifest에 포함된 `imageSha256`을 통해 signature와 firmware image가 연결된다.

Release signing system은 private key로 Artifact를 서명한다.
Device는 보호된 저장소의 public key로 signature를 검증한다.
private signing key를 Device나 OTA Server source code에 포함하지 않는다.

Device는 다음 검증을 모두 통과한 뒤에만 State를 Downloaded로 변경한다.

1. image size 확인
2. Device Profile과 hardware revision 확인
3. SHA-256 hash 확인
4. signature 확인
5. security version을 이용한 anti-rollback 확인

manifest encoding과 signature algorithm은 지원할 STM32 profile을 확정할 때 결정한다.


## 9. 개발 산출물과 책임

### 9.1 플랫폼 제공물

| 제공물 | 내용 |
|---|---|
| Device Client 계약 | LwM2M profile, Object와 상태 전이 |
| Device Integration Kit | `/5` Adapter, Update Service, Backend interface |
| Linux reference client | 정상·실패 OTA 흐름의 reference implementation |
| Artifact 도구 | manifest 생성, hash와 test signature 생성 |
| 검증 환경 | Leshan Server와 자동화된 상호운용 test |
| 지원 Profile 문서 | 지원하는 RTOS, network stack, Client engine 조합 |

플랫폼은 특정 STM32 제품의 최종 firmware binary를 범용 산출물로 제공하지 않는다.

### 9.2 장치 개발자 제공물

| 제공물 | 내용 |
|---|---|
| Target Client 통합 | LwM2M engine을 MCU, RTOS와 network stack에 연결 |
| Device Object 연결 | `/3` 정보와 제품 telemetry 연결 |
| Security 연결 | credential과 signing public key를 보호된 저장소에 연결 |
| Update Backend | flash, 검증, install, reboot, confirmation, rollback |
| 최종 firmware | 실제 제품에서 build된 image와 resource 사용량 |
| Hardware 검증 결과 | 성공, 실패와 전원 중단 시험 결과 |

### 9.3 공동 결정 항목

다음 항목은 STM32 구현 시작 전에 플랫폼과 장치 개발자가 함께 확정한다.

- MCU와 board
- RTOS와 network stack
- LwM2M Client engine
- bootloader와 flash partition
- manifest encoding과 signature algorithm
- firmware download protocol

## 10. Acceptance Test

| 영역 | 시험 | 통과 기준 |
|---|---|---|
| Registration | 고유 endpoint와 DTLS credential로 연결 | Server에서 인증 후 Device로 등록 |
| Reconnect | network 단절과 reboot | 같은 endpoint로 자동 재등록 |
| Object | `/3`과 `/5` v1.2 Read | 계약에 정의한 Resource와 값 확인 |
| 정상 OTA | Package URI download와 Update | `Downloading -> Downloaded -> Updating -> Idle`, Result `1` |
| Cancel | Downloading 또는 Downloaded에서 Cancel | State `Idle`, Result `10`, 저장된 package 제거 |
| Deferred | 설치 연기 후 승인 또는 시간 만료 | Result `11` 후 정상적으로 Updating 진입 |
| 검증 실패 | 손상 image 또는 잘못된 signature | 설치하지 않고 Result `5` |
| Download 실패 | 연결 단절, 잘못된 URI 또는 protocol | 원인에 맞는 Result `4`, `7`, `9` |
| 저장 공간 부족 | staging 공간 부족 | download를 시작하지 않고 Result `2` |
| Boot 실패 | 새 firmware boot 실패 | 이전 image로 rollback하고 Result `8` |
| 전원 중단 | Downloading과 Updating 중 전원 차단 | reboot 후 일관된 State와 Result 복구 |

Linux reference client는 simulated backend로 가능한 항목을 검증한다.
STM32 지원 Profile은 동일한 계약을 실제 hardware에서 다시 검증한다.

모든 필수 시험을 통과하기 전에는 해당 STM32 Profile을 지원 완료로 표시하지 않는다.