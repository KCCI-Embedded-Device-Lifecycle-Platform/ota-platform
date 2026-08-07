# Device Integration Kit

장치 펌웨어에 통합되는 플랫폼 제공 LwM2M 클라이언트 구성요소다.
완성된 STM32 펌웨어 자체를 제공하는 패키지는 아니다.

## 플랫폼 제공 범위

- 지원 LwM2M Client Engine
- 표준 Object Adapter (`/5` 포함)
- Firmware Update Service
- Device Update Backend 인터페이스
- 상태와 Update Result 변환 로직
- 단위 및 적합성 테스트

## 장치 개발자 구현 범위

- RTOS, 네트워크, DTLS 연결
- 인증정보 저장
- 장치별 Object 데이터
- Flash, 이미지 검증, Bootloader, 재부팅, Confirm, Rollback
- 실제 하드웨어 통합 및 검증

## Linux Reference와의 관계

`clients/linux-reference/`는 이 Kit를 사용하는 POSIX 기반 예제이자 E2E 검증 앱이다.

Linux 소켓, 시그널 처리, 임시 파일, 재부팅 모사는 Linux Reference에만 둔다.
장치에서 재사용할 인터페이스와 로직은 이 Kit에 둔다.


## Component Flow

```text
Package URI
  → Wakaama Adapter
  → Download Transport
  → Firmware Update Service
  → Device Update Backend
```

## 핵심 개념

| 구분 | 의미 |
|---|---|
| Backend Status | Flash와 Bootloader 작업의 내부 결과 |
| Update Result | Server가 `/5/0/5`로 읽는 표준 결과 |
| Service Status | Adapter가 CoAP 응답을 결정하는 즉시 결과 |

Backend 호출 순서:

```text
prepare → write_chunk → finish_download → install → recover_after_boot
```

## Current Status

구현됨:

- Firmware Update 상태 머신과 오류 변환
- Backend 및 Download Transport 인터페이스
- Wakaama `/5` v1.2 Adapter
- Linux 파일 Backend
- Linux libcoap 기반 비동기 CoAP Pull Transport
- Block2 chunk를 Service와 staging 파일로 전달하는 E2E 테스트
- Reference App의 Wakaama/libcoap 통합 이벤트 루프

현재 Linux Reference는 실제 Download Transport를 `/5` Adapter에 연결한다.

Linux 구현은 `libcoap-3-notls`, POSIX socket과 epoll을 사용하는 참조 코드다.
STM32 장치에서는 같은 Download Transport 인터페이스에 장치의 RTOS와
네트워크 스택을 연결한다.

남은 작업:

- State와 Update Result의 Observe/Notify 연결
- 전송 실패와 libcoap NACK 처리 보강
- manifest, hash, signature, anti-rollback 검증
- 재부팅 결과 영속화
- 실제 Flash 및 Bootloader 연결
