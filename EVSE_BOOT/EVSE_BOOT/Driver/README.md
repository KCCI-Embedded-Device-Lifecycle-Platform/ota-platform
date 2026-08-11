# Driver Layer

`Driver` 계층은 HW 기능을 특정 장치 또는 통신 방식에 적합한 인터페이스로 변환합니다.

현재 구현된 Driver는 USART3 기반 OTA 바이트 스트림 전송을 담당하는 UART Transport Driver입니다.

## 책임

```text
Driver
├── HW UART 객체 사용
├── 송수신 Timeout 관리
├── 바이트 배열 송신
├── 1바이트 단위 수신
├── 초기화 상태 관리
└── HW 상태를 Driver 상태로 변환
```

## 파일 구성

```text
Driver/
├── drv_common.h
├── drv_uart_transport.c
├── drv_uart_transport.h
└── README.md
```

## 파일별 역할

### `drv_common.h`

Driver 계층 공통 상태를 정의합니다.

```c
typedef enum
{
    DRV_STATUS_OK = 0,
    DRV_STATUS_ERROR,
    DRV_STATUS_TIMEOUT,
    DRV_STATUS_BUSY,
    DRV_STATUS_INVALID_ARGUMENT,
    DRV_STATUS_NOT_INITIALIZED
} drv_status_t;
```

HW 상태는 Driver 내부에서 `drv_status_t`로 변환됩니다.

```text
HW_STATUS_OK              → DRV_STATUS_OK
HW_STATUS_TIMEOUT         → DRV_STATUS_TIMEOUT
HW_STATUS_BUSY            → DRV_STATUS_BUSY
HW_STATUS_INVALID_ARGUMENT→ DRV_STATUS_INVALID_ARGUMENT
HW_STATUS_NOT_INITIALIZED → DRV_STATUS_NOT_INITIALIZED
HW_STATUS_ERROR           → DRV_STATUS_ERROR
```

### `drv_uart_transport.c/h`

HW UART를 OTA 통신용 바이트 스트림 인터페이스로 제공합니다.

Driver 객체:

```c
typedef struct
{
    hw_uart_t *hw_uart;
    uint32_t tx_timeout_ms;
    uint32_t rx_timeout_ms;
    bool initialized;
} drv_uart_transport_t;
```

주요 인터페이스:

```c
drv_status_t DrvUartTransport_Init(
    drv_uart_transport_t *transport,
    hw_uart_t *hw_uart,
    uint32_t tx_timeout_ms,
    uint32_t rx_timeout_ms
);

void DrvUartTransport_Deinit(
    drv_uart_transport_t *transport
);

drv_status_t DrvUartTransport_Send(
    drv_uart_transport_t *transport,
    const uint8_t *data,
    uint16_t length
);

drv_status_t DrvUartTransport_ReceiveByte(
    drv_uart_transport_t *transport,
    uint8_t *received_byte
);

bool DrvUartTransport_IsInitialized(
    const drv_uart_transport_t *transport
);
```

## 하지 않는 일

Driver 계층은 다음 기능을 담당하지 않습니다.

```text
문자열 길이 계산
CR/LF 기반 명령 한 줄 구성
HELLO 또는 VERSION 명령 해석
SOF 및 Packet Length 파싱
CRC16 계산
Application Jump
USER 버튼 및 LED 제어
```

이 기능은 각각 App, Middleware, BSP가 담당합니다.

## 헤더 가드

```c
#ifndef EVSE_BOOT_DRV__DRV_COMMON_H
#define EVSE_BOOT_DRV__DRV_COMMON_H
#endif

#ifndef EVSE_BOOT_DRV__DRV_UART_TRANSPORT_H
#define EVSE_BOOT_DRV__DRV_UART_TRANSPORT_H
#endif
```

## 의존 관계

```text
BSP Transport
      │
      ▼
drv_uart_transport
      │
      ▼
hw_uart
      │
      ▼
STM32 HAL UART
```

Driver는 `HW`만 사용할 수 있습니다.

금지되는 의존:

```text
Driver → BSP
Driver → Middleware
Driver → App
Driver → HAL 직접 호출
```

## 현재 통신 처리

```text
BspTransport_Send()
  │
  ▼
DrvUartTransport_Send()
  │
  ▼
HwUart_Write()
  │
  ▼
HAL_UART_Transmit()
```

```text
BspTransport_ReceiveByte()
  │
  ▼
DrvUartTransport_ReceiveByte()
  │
  ▼
HwUart_ReadByte()
  │
  ▼
HAL_UART_Receive()
```

## 전체 검색 검증

`HwUart_Write()`와 `HwUart_ReadByte()`는 원칙적으로 이 파일에서만 사용합니다.

```text
HwUart_Write
└─ Driver/drv_uart_transport.c

HwUart_ReadByte
└─ Driver/drv_uart_transport.c
```

`HwUart_Attach()`는 보드에서 실제 UART를 선택하는 `BSP/bsp_transport.c`에서 호출합니다.

## 이후 확장

통신 방식이 확장되면 다음 Driver를 추가할 수 있습니다.

```text
Driver/
├── drv_uart_transport.c/h
├── drv_rs485.c/h
└── drv_esp8266.c/h
```

확장 예시:

```text
유선 개발
BspTransport → DrvUartTransport

RS485 적용
BspTransport → DrvRs485

Wi-Fi OTA
BspTransport → DrvEsp8266
```

상위 App과 Middleware는 통신 장치 변경의 영향을 최소화해야 합니다.
