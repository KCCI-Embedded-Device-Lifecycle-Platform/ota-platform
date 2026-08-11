# HW Layer

`HW` 계층은 STM32 HAL과 CMSIS에 직접 접근하는 최하위 사용자 코드 계층입니다.

상위 계층은 HAL 함수나 Cortex-M 시스템 레지스터를 직접 사용하지 않고 반드시 HW 인터페이스를 경유합니다.

## 책임

```text
HW
├── UART HAL 접근
├── GPIO HAL 접근
├── Tick 및 Delay
├── MCU Reset
├── Vector Table 검사
├── VTOR 변경
├── MSP 설정
└── NVIC 및 SysTick 정리
```

## 파일 구성

```text
HW/
├── hw_common.h
├── hw_uart.c
├── hw_uart.h
├── hw_gpio.c
├── hw_gpio.h
├── hw_system.c
├── hw_system.h
└── README.md
```

## 파일별 역할

### `hw_common.h`

HW 계층 공통 반환 상태를 정의합니다.

```c
typedef enum
{
    HW_STATUS_OK = 0,
    HW_STATUS_ERROR,
    HW_STATUS_TIMEOUT,
    HW_STATUS_BUSY,
    HW_STATUS_INVALID_ARGUMENT,
    HW_STATUS_NOT_INITIALIZED
} hw_status_t;
```

HAL 상태를 그대로 상위 계층에 노출하지 않고 `hw_status_t`로 변환합니다.

### `hw_uart.c/h`

CubeMX가 생성한 `UART_HandleTypeDef`를 HW UART 객체에 연결하고 실제 HAL UART 송수신을 수행합니다.

주요 인터페이스:

```c
hw_status_t HwUart_Attach(
    hw_uart_t *uart,
    UART_HandleTypeDef *hal_uart
);

hw_status_t HwUart_Write(
    hw_uart_t *uart,
    const uint8_t *data,
    uint16_t length,
    uint32_t timeout_ms
);

hw_status_t HwUart_ReadByte(
    hw_uart_t *uart,
    uint8_t *received_byte,
    uint32_t timeout_ms
);
```

이 파일에서만 다음 HAL 함수를 직접 호출합니다.

```c
HAL_UART_Transmit();
HAL_UART_Receive();
```

### `hw_gpio.c/h`

GPIO Port와 Pin을 HW GPIO 객체로 추상화합니다.

주요 인터페이스:

```c
hw_status_t HwGpio_Attach(
    hw_gpio_t *gpio,
    GPIO_TypeDef *port,
    uint16_t pin
);

hw_status_t HwGpio_Read(
    const hw_gpio_t *gpio,
    GPIO_PinState *state
);

hw_status_t HwGpio_Write(
    const hw_gpio_t *gpio,
    GPIO_PinState state
);

hw_status_t HwGpio_Toggle(
    const hw_gpio_t *gpio
);
```

HW GPIO는 해당 핀이 버튼인지 LED인지 알지 못합니다. 핀의 용도와 활성 레벨은 BSP가 결정합니다.

이 파일에서만 다음 HAL 함수를 직접 호출합니다.

```c
HAL_GPIO_ReadPin();
HAL_GPIO_WritePin();
HAL_GPIO_TogglePin();
```

### `hw_system.c/h`

Cortex-M 시스템 제어와 부트 전환에 필요한 저수준 처리를 담당합니다.

주요 인터페이스:

```c
bool HwSystem_IsVectorTableValid(
    uint32_t vector_table_address,
    uint32_t executable_end_address
);

void HwSystem_JumpToVectorTable(
    uint32_t vector_table_address
);

void HwSystem_Reset(void);

uint32_t HwSystem_GetTick(void);

void HwSystem_Delay(uint32_t delay_ms);
```

주요 처리:

```text
Application 초기 MSP 검사
Reset Handler Thumb bit 검사
Reset Handler Flash 범위 검사
전역 인터럽트 비활성화
HAL 및 RCC DeInit
SysTick 중지
NVIC Enable/Pending Clear
VTOR 변경
MSP 설정
Application Reset Handler 호출
```

## Delay 정책

Bootloader에는 RTOS를 사용하지 않으므로 `osDelay()`를 사용하지 않습니다.

```text
짧은 블로킹 대기
└─ HwSystem_Delay()
   └─ HAL_Delay()

주기 처리
└─ HwSystem_GetTick()
   └─ HAL_GetTick()
```

버튼 디바운싱이나 Application Jump 직전의 짧은 대기에는 `HwSystem_Delay()`를 사용할 수 있습니다.

명령 대기 루프나 LED 주기 처리에는 블로킹 Delay를 사용하지 않고 Tick 차이를 이용합니다.

## 헤더 가드

```c
#ifndef EVSE_BOOT_HW__HW_COMMON_H
#define EVSE_BOOT_HW__HW_COMMON_H
#endif

#ifndef EVSE_BOOT_HW__HW_UART_H
#define EVSE_BOOT_HW__HW_UART_H
#endif

#ifndef EVSE_BOOT_HW__HW_GPIO_H
#define EVSE_BOOT_HW__HW_GPIO_H
#endif

#ifndef EVSE_BOOT_HW__HW_SYSTEM_H
#define EVSE_BOOT_HW__HW_SYSTEM_H
#endif
```

## 의존 관계

```text
Driver ─────▶ hw_uart
BSP ────────▶ hw_gpio
BSP ────────▶ hw_system
HW ─────────▶ STM32 HAL / CMSIS
```

HW 계층은 상위 계층을 include하지 않습니다.

금지되는 의존:

```text
HW → Driver
HW → BSP
HW → Middleware
HW → App
```

## 전체 검색 검증

사용자 작성 코드에서 다음 호출은 지정된 HW 파일에만 있어야 합니다.

| 호출 | 허용 파일 |
|---|---|
| `HAL_UART_Transmit` | `hw_uart.c` |
| `HAL_UART_Receive` | `hw_uart.c` |
| `HAL_GPIO_ReadPin` | `hw_gpio.c` |
| `HAL_GPIO_WritePin` | `hw_gpio.c` |
| `HAL_GPIO_TogglePin` | `hw_gpio.c` |
| `SCB->VTOR` | `hw_system.c` |
| `__set_MSP` | `hw_system.c` |
| `NVIC->ICER` | `hw_system.c` |
| `NVIC->ICPR` | `hw_system.c` |

CubeMX가 생성한 Core 초기화 파일은 예외입니다.

## 이후 확장

실제 OTA Flash 업데이트 단계에서 다음 모듈을 추가합니다.

```text
HW/
├── hw_flash.c
├── hw_flash.h
├── hw_crc.c
└── hw_crc.h
```

예상 책임:

```text
hw_flash
├── HAL_FLASH_Unlock
├── HAL_FLASHEx_Erase
├── HAL_FLASH_Program
└── HAL_FLASH_Lock

hw_crc
└── STM32 CRC Peripheral 접근
```
