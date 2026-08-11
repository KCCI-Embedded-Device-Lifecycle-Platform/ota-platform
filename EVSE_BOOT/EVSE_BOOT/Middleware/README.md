# Middleware Layer

`Middleware` 계층은 하드웨어와 통신 장치에 독립적인 순수 데이터 처리 기능을 제공합니다.

현재는 OTA 바이너리 프로토콜 파싱·인코딩과 패킷 CRC16 계산을 담당합니다.

## 책임

```text
Middleware
├── SOF 탐색
├── Command 파싱
├── Length 파싱
├── Data 수집
├── CRC16 검증
├── Packet 객체 생성
└── Packet을 송신 Frame으로 인코딩
```

## 파일 구성

```text
Middleware/
├── mw_common.h
├── boot_crc16.c
├── boot_crc16.h
├── boot_protocol.c
├── boot_protocol.h
└── README.md
```

## 파일별 역할

### `mw_common.h`

Middleware 공통 반환 상태를 정의합니다.

```c
typedef enum
{
    MW_STATUS_OK = 0,
    MW_STATUS_ERROR,
    MW_STATUS_INVALID_ARGUMENT,
    MW_STATUS_BUFFER_TOO_SMALL
} mw_status_t;
```

### `boot_crc16.c/h`

CRC-16/CCITT-FALSE 알고리즘을 구현합니다.

설정:

```text
Polynomial : 0x1021
Initial    : 0xFFFF
RefIn      : false
RefOut     : false
XorOut     : 0x0000
```

주요 인터페이스:

```c
uint16_t BootCrc16_Update(
    uint16_t crc,
    uint8_t data
);

uint16_t BootCrc16_Calculate(
    const uint8_t *data,
    uint16_t length
);
```

패킷 CRC 대상:

```text
CMD + LENGTH_L + LENGTH_H + DATA
```

SOF는 CRC 계산에서 제외합니다.

### `boot_protocol.c/h`

수신된 바이트를 상태머신으로 조립해 패킷을 만들고, 응답 패킷을 전송 가능한 Frame으로 인코딩합니다.

## 패킷 구조

```text
┌──────┬──────┬─────────┬──────────┬────────────┬─────────┐
│ SOF1 │ SOF2 │ Command │ Length   │ Data       │ CRC16   │
│ 1 B  │ 1 B  │ 1 B     │ 2 B, LE  │ 0~512 B    │ 2 B, LE │
└──────┴──────┴─────────┴──────────┴────────────┴─────────┘
```

고정 값:

```c
#define BOOT_PROTOCOL_SOF_1         0xAAU
#define BOOT_PROTOCOL_SOF_2         0x55U
#define BOOT_PROTOCOL_MAX_DATA_SIZE 512U
```

## Parser 상태

```c
typedef enum
{
    BOOT_PROTOCOL_STATE_WAIT_SOF_1 = 0,
    BOOT_PROTOCOL_STATE_WAIT_SOF_2,
    BOOT_PROTOCOL_STATE_COMMAND,
    BOOT_PROTOCOL_STATE_LENGTH_LOW,
    BOOT_PROTOCOL_STATE_LENGTH_HIGH,
    BOOT_PROTOCOL_STATE_DATA,
    BOOT_PROTOCOL_STATE_CRC_LOW,
    BOOT_PROTOCOL_STATE_CRC_HIGH
} boot_protocol_state_t;
```

처리 흐름:

```text
WAIT_SOF_1
→ WAIT_SOF_2
→ COMMAND
→ LENGTH_LOW
→ LENGTH_HIGH
→ DATA
→ CRC_LOW
→ CRC_HIGH
→ PACKET_READY
```

## Parser 객체

```c
typedef struct
{
    boot_protocol_state_t state;
    boot_protocol_packet_t packet;
    uint16_t data_index;
    uint16_t calculated_crc;
    uint16_t received_crc;
    uint8_t last_command;
} boot_protocol_parser_t;
```

전역 단일 Parser를 사용하지 않고 객체를 함수에 전달합니다.

장점:

```text
통신 채널별 Parser 생성 가능
단위 테스트 용이
전역 상태 의존 감소
재초기화 시점 명확화
```

## 주요 인터페이스

```c
void BootProtocol_Init(
    boot_protocol_parser_t *parser
);

boot_protocol_result_t BootProtocol_ProcessByte(
    boot_protocol_parser_t *parser,
    uint8_t received_byte,
    boot_protocol_packet_t *output_packet
);

mw_status_t BootProtocol_Encode(
    const boot_protocol_packet_t *packet,
    uint8_t *output_frame,
    uint16_t output_capacity,
    uint16_t *output_length
);

uint8_t BootProtocol_GetLastCommand(
    const boot_protocol_parser_t *parser
);
```

## 프로토콜 명령

| 명령 | 값 |
|---|---:|
| `HELLO` | `0x01` |
| `GET_VERSION` | `0x02` |
| `START_UPDATE` | `0x10` |
| `DATA` | `0x11` |
| `END_UPDATE` | `0x12` |
| `ABORT` | `0x13` |
| `RUN_APP` | `0x20` |
| `NACK` | `0x1F` |
| `ACK` | `0x79` |
| `VERSION_RESPONSE` | `0x82` |

현재 실제 명령 의미와 동작은 App 계층에서 처리합니다.

## 하지 않는 일

Middleware는 다음 작업을 하지 않습니다.

```text
UART 송수신
RS485 송수신
ESP8266 제어
USER 버튼 처리
LED 제어
Application Jump
Flash Erase
Flash Write
명령의 실제 동작 결정
```

## 헤더 가드

```c
#ifndef EVSE_BOOT_MW__MW_COMMON_H
#define EVSE_BOOT_MW__MW_COMMON_H
#endif

#ifndef EVSE_BOOT_MW__BOOT_CRC16_H
#define EVSE_BOOT_MW__BOOT_CRC16_H
#endif

#ifndef EVSE_BOOT_MW__BOOT_PROTOCOL_H
#define EVSE_BOOT_MW__BOOT_PROTOCOL_H
#endif
```

## 의존 관계

```text
App
 ├────────▶ BootProtocol
 └────────▶ BootCrc16

BootProtocol
 └────────▶ BootCrc16

Middleware
 └────────▶ 표준 C 라이브러리
```

금지되는 include:

```c
#include "BSP/bsp_transport.h"
#include "Driver/drv_uart_transport.h"
#include "HW/hw_uart.h"
#include "stm32f4xx_hal.h"
#include "main.h"
#include "usart.h"
```

## 검증 항목

```text
□ 정상 SOF 인식
□ Length Little Endian 처리
□ 0바이트 Data Packet 처리
□ 최대 512바이트 Data 처리
□ CRC 정상 패킷 PACKET_READY
□ CRC 오류 CRC_ERROR
□ 최대 길이 초과 LENGTH_ERROR
□ Packet Encode 결과와 Python 결과 일치
□ Middleware 폴더에서 UART/HAL 참조 없음
```

## 이후 확장

향후 다음 순수 로직을 Middleware로 추가할 수 있습니다.

```text
Middleware/
├── boot_metadata.c/h
├── boot_image_crc32.c/h
└── boot_version_compare.c/h
```

단, STM32 CRC Peripheral을 직접 제어하는 코드는 HW 계층에 두고 Middleware에는 알고리즘 또는 데이터 처리 정책만 둡니다.
