# App Layer

`App` 계층은 EVSE Bootloader의 전체 실행 흐름과 명령의 의미를 관리하는 최상위 사용자 코드 계층입니다.

App은 BSP가 제공하는 보드 기능과 Middleware가 제공하는 순수 프로토콜 기능을 조합합니다.

## 책임

```text
App
├── Bootloader 초기화
├── Bootloader 진입 조건 판단
├── Text/Binary 통신 모드 관리
├── 텍스트 콘솔 관리
├── 명령 해석
├── Protocol Parser 실행
├── 응답 Packet 생성 및 전송
├── LED 주기 처리
├── Application Jump 요청
└── 이후 OTA Update 상태머신 관리
```

## 파일 구성

```text
App/
├── ap_bootloader.c
├── ap_bootloader.h
├── ap_boot_command.c
├── ap_boot_command.h
├── ap_boot_console.c
├── ap_boot_console.h
└── README.md
```

## 파일별 역할

### `ap_bootloader.c/h`

부트로더의 최상위 실행 진입점입니다.

외부 공개 인터페이스:

```c
bool ApBootloader_Init(void);

void ApBootloader_Run(void);
```

`main.c`는 이 두 함수만 호출합니다.

```c
if (!ApBootloader_Init())
{
    Error_Handler();
}

ApBootloader_Run();
```

주요 내부 기능:

```text
BSP Boot 초기화
BSP Transport 초기화
Console 초기화
Protocol Parser 초기화
USER 버튼 확인
Application 유효성 확인
텍스트 명령 루프
바이너리 Packet 처리
상태 LED 비차단 점멸
Application Jump
```

### `ap_boot_command.c/h`

텍스트 및 바이너리 명령을 해석하고 실행 Action과 응답을 생성합니다.

명령을 직접 전송하거나 Application으로 직접 Jump하지 않습니다.

텍스트 명령 결과:

```c
typedef struct
{
    ap_boot_action_t action;
    const char *response;
} ap_boot_text_command_result_t;
```

바이너리 명령 결과:

```c
typedef struct
{
    ap_boot_action_t action;
    bool response_ready;
    boot_protocol_packet_t response;
} ap_boot_binary_command_result_t;
```

Action:

```c
typedef enum
{
    AP_BOOT_ACTION_NONE = 0,
    AP_BOOT_ACTION_ENTER_BINARY_PROTOCOL,
    AP_BOOT_ACTION_RUN_APPLICATION
} ap_boot_action_t;
```

명령 처리 함수:

```c
bool ApBootCommand_ProcessText(
    const char *command,
    ap_boot_text_command_result_t *result
);

bool ApBootCommand_ProcessPacket(
    const boot_protocol_packet_t *request,
    ap_boot_binary_command_result_t *result
);

void ApBootCommand_BuildNack(
    uint8_t request_command,
    boot_protocol_error_t error,
    boot_protocol_packet_t *response
);
```

### `ap_boot_console.c/h`

개발 및 진단용 텍스트 콘솔을 담당합니다.

주요 기능:

```text
NULL 종료 문자열 송신
CR/LF 기반 한 줄 수신
수신 버퍼 관리
Buffer Overflow 검출
```

주요 인터페이스:

```c
void ApBootConsole_Init(void);

bool ApBootConsole_SendString(
    const char *message
);

ap_boot_console_result_t ApBootConsole_PollLine(
    char *output_line,
    size_t output_line_size
);
```

실제 UART 선택과 송수신은 `BspTransport`가 담당합니다.

## 실행 흐름

### 초기화

```text
main.c
  │
  ▼
ApBootloader_Init()
  ├─ BspBoot_Init()
  ├─ BspTransport_Init()
  ├─ ApBootConsole_Init()
  └─ BootProtocol_Init()
```

### 일반 부팅

```text
ApBootloader_Run()
  │
  ├─ USER 버튼 미입력
  ├─ Application 유효
  │
  ▼
ApBootloader_JumpToApplication()
  │
  ├─ 로그 출력
  ├─ 상태 LED OFF
  └─ BspBoot_JumpToApplication()
```

### Bootloader 모드

```text
USER 버튼 입력
또는
Application 무효
또는
버튼 읽기 오류
  │
  ▼
ApBootloader_CommandLoop()
```

## 텍스트 통신 모드

Bootloader 진입 직후 기본 모드는 Text입니다.

지원 명령:

| 명령 | 결과 |
|---|---|
| `HELLO` | `[BOOT] ACK` |
| `VERSION` | Bootloader 버전 |
| `PROTO` | Binary Protocol Mode 전환 |
| `RUN` | Application 실행 |
| 기타 | `[BOOT] NACK` |

```text
ApBootConsole_PollLine()
  │
  ▼
ApBootCommand_ProcessText()
  │
  ├─ response 문자열 전송
  └─ action 실행
```

## 바이너리 통신 모드

`PROTO` 명령 이후 바이너리 패킷 모드로 전환합니다.

```text
BspTransport_ReceiveByte()
  │
  ▼
BootProtocol_ProcessByte()
  │
  ▼
ApBootCommand_ProcessPacket()
  │
  ├─ Response Packet 생성
  └─ Action 반환
  │
  ▼
BootProtocol_Encode()
  │
  ▼
BspTransport_Send()
```

## RUN_APP 안전 처리

바이너리 `RUN_APP` 요청은 ACK 패킷이 정상 전송된 경우에만 Application Jump Action을 실행합니다.

```text
RUN_APP Packet
→ ACK Packet Encode
→ BSP Transport 전송 성공
→ Application Jump
```

## LED 처리

Bootloader 명령 루프에서 상태 LED는 Tick 기반 비차단 방식으로 처리합니다.

```c
if ((BspBoot_GetTick() - previous_tick) >=
    BOOT_LED_TOGGLE_INTERVAL_MS)
{
    previous_tick = BspBoot_GetTick();
    BspBoot_ToggleStatusLed();
}
```

명령 루프에 `HAL_Delay()`를 직접 넣지 않습니다.

## 헤더 가드

```c
#ifndef EVSE_BOOT_APP__AP_BOOTLOADER_H
#define EVSE_BOOT_APP__AP_BOOTLOADER_H
#endif

#ifndef EVSE_BOOT_APP__AP_BOOT_COMMAND_H
#define EVSE_BOOT_APP__AP_BOOT_COMMAND_H
#endif

#ifndef EVSE_BOOT_APP__AP_BOOT_CONSOLE_H
#define EVSE_BOOT_APP__AP_BOOT_CONSOLE_H
#endif
```

## 의존 관계

```text
App
├── BSP
└── Middleware
```

App에서 허용되는 include 예시:

```c
#include "BSP/bsp_boot.h"
#include "BSP/bsp_transport.h"
#include "Middleware/boot_protocol.h"
```

App에서 금지되는 직접 의존:

```text
App → HW
App → Driver
App → STM32 HAL
App → UART_HandleTypeDef
App → GPIO Port/Pin
```

## 기존 기능 대응

| 기존 기능 | 현재 담당 |
|---|---|
| Bootloader 전체 흐름 | `ap_bootloader` |
| 텍스트 문자열 송수신 | `ap_boot_console` |
| HELLO/VERSION/RUN | `ap_boot_command` |
| PROTO 전환 | `ap_boot_command` + `ap_bootloader` |
| Binary Packet 처리 | `ap_bootloader` + Middleware |
| 버튼/LED/App Jump | BSP |
| UART 장치 선택 | BSP |
| CRC16 | Middleware |

## 회귀 테스트

```text
□ 일반 Reset에서 Application 실행
□ USER 버튼 + Reset에서 Bootloader 진입
□ Bootloader 상태 LED 점멸
□ HELLO 문자열 ACK
□ VERSION 문자열 응답
□ 잘못된 문자열 NACK
□ RUN 문자열 후 Application 실행
□ PROTO 모드 전환
□ Binary HELLO ACK
□ GET_VERSION 응답
□ CRC 오류 NACK
□ Binary RUN_APP ACK 후 Application 실행
```

## 이후 확장

실제 OTA 업데이트 상태머신은 다음 파일로 분리할 예정입니다.

```text
App/
├── ap_boot_update.c
└── ap_boot_update.h
```

예상 상태:

```c
typedef enum
{
    AP_BOOT_UPDATE_IDLE = 0,
    AP_BOOT_UPDATE_STARTED,
    AP_BOOT_UPDATE_RECEIVING,
    AP_BOOT_UPDATE_VERIFYING,
    AP_BOOT_UPDATE_COMPLETE,
    AP_BOOT_UPDATE_FAILED
} ap_boot_update_state_t;
```

예상 처리:

```text
START_UPDATE
→ Firmware Size/CRC32 검사
→ Application 영역 Erase
→ DATA Packet 반복 처리
→ Flash Write
→ END_UPDATE
→ 전체 CRC32 검증
→ Metadata 저장
→ Reset 또는 Application 실행
```
