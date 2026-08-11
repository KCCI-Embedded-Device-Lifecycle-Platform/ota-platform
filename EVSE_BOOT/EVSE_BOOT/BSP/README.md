# BSP Layer

`BSP`는 Board Support Package의 약어입니다.

이 계층은 NUCLEO-F429ZI 보드에서 실제로 어떤 UART, GPIO, Application 주소를 사용할지 결정하고 상위 계층에 보드 기능 인터페이스를 제공합니다.

## 책임

```text
BSP
├── OTA 통신 UART 선택
├── HW UART와 Driver 연결
├── USER 버튼 선택 및 활성 레벨 관리
├── Bootloader 상태 LED 선택
├── Application 주소 정책
├── Application 유효성 검사 요청
├── Application Jump 요청
├── Tick 및 Delay 제공
└── 시스템 Reset 제공
```

## 파일 구성

```text
BSP/
├── bsp_common.h
├── bsp_boot.c/h
├── bsp_transport.c/h
├── bsp_storage.c/h
└── README.md
```

## 파일별 역할

### `bsp_common.h`

BSP 공통 상태를 정의합니다.

```c
typedef enum
{
    BSP_STATUS_OK = 0,
    BSP_STATUS_ERROR,
    BSP_STATUS_TIMEOUT,
    BSP_STATUS_BUSY,
    BSP_STATUS_INVALID_ARGUMENT,
    BSP_STATUS_NOT_INITIALIZED
} bsp_status_t;
```

Driver와 HW의 상태는 BSP 경계에서 `bsp_status_t`로 변환됩니다.

### `bsp_transport.c/h`

현재 보드에서 OTA 통신에 사용할 USART3를 선택하고 HW와 Driver 객체를 초기화합니다.

주요 인터페이스:

```c
bsp_status_t BspTransport_Init(void);

void BspTransport_Deinit(void);

bsp_status_t BspTransport_Send(
    const uint8_t *data,
    uint16_t length
);

bsp_status_t BspTransport_ReceiveByte(
    uint8_t *received_byte
);

bool BspTransport_IsInitialized(void);
```

현재 내부 연결:

```text
USART3 / huart3
      │
      ▼
HwUart_Attach()
      │
      ▼
DrvUartTransport_Init()
```

상위 계층은 `huart3`나 `UART_HandleTypeDef`를 알지 못합니다.

### `bsp_boot.c/h`

부트로더가 사용하는 보드 기능을 제공합니다.

현재 보드 설정:

```text
USER 버튼 : B1
상태 LED   : LD1
App 주소   : 0x08020000
```

주요 인터페이스:

```c
bsp_status_t BspBoot_Init(void);

bsp_status_t BspBoot_GetUpdateRequest(
    bool *requested
);

bool BspBoot_IsApplicationValid(void);

bsp_status_t BspBoot_SetStatusLed(
    bool enabled
);

bsp_status_t BspBoot_ToggleStatusLed(void);

uint32_t BspBoot_GetTick(void);

void BspBoot_Delay(uint32_t delay_ms);

void BspBoot_JumpToApplication(void);

void BspBoot_Reset(void);
```

## 버튼 처리

`BspBoot_GetUpdateRequest()`는 USER 버튼을 두 번 읽어 간단한 디바운싱을 수행합니다.

```text
1차 GPIO 읽기
→ BOOT_BUTTON_DEBOUNCE_MS 대기
→ 2차 GPIO 읽기
→ 두 값이 모두 활성 상태이면 Update Request
```

Bootloader에는 RTOS가 없으므로 내부 대기는 다음 경로를 사용합니다.

```text
BspBoot_Delay()
  │
  ▼
HwSystem_Delay()
  │
  ▼
HAL_Delay()
```

## Application 처리

```text
BspBoot_IsApplicationValid()
  │
  ▼
HwSystem_IsVectorTableValid()
```

```text
BspBoot_JumpToApplication()
  │
  ▼
HwSystem_JumpToVectorTable(0x08020000)
```

BSP는 Application 주소 정책을 알고 있지만 VTOR, MSP, NVIC의 실제 제어 방법은 알지 못합니다.

## 헤더 가드

```c
#ifndef EVSE_BOOT_BSP__BSP_COMMON_H
#define EVSE_BOOT_BSP__BSP_COMMON_H
#endif

#ifndef EVSE_BOOT_BSP__BSP_TRANSPORT_H
#define EVSE_BOOT_BSP__BSP_TRANSPORT_H
#endif

#ifndef EVSE_BOOT_BSP__BSP_BOOT_H
#define EVSE_BOOT_BSP__BSP_BOOT_H
#endif
```

## 의존 관계

```text
App
 ├────────────▶ BspBoot
 └────────────▶ BspTransport

BspTransport
 ├────────────▶ DrvUartTransport
 └────────────▶ HwUart

BspBoot
 ├────────────▶ HwGpio
 └────────────▶ HwSystem
```

금지되는 의존:

```text
BSP → App
BSP → Middleware 명령 처리
BSP → HAL 직접 호출
```

## 보드 변경 시 수정 위치

다음 하드웨어가 변경되면 BSP만 우선 수정합니다.

| 변경 항목 | 수정 위치 |
|---|---|
| USART3 → 다른 UART | `bsp_transport.c` |
| USER 버튼 핀 | `bsp_boot.c` |
| 상태 LED 핀 | `bsp_boot.c` |
| 버튼 활성 레벨 | `bsp_boot.c` |
| LED 활성 레벨 | `bsp_boot.c` |
| Application 시작 주소 | `boot_config.h`, Linker Script |

## 전체 검색 검증

```text
HwUart_Attach
└─ BSP/bsp_transport.c

DrvUartTransport_Init
└─ BSP/bsp_transport.c

HwGpio_Attach
HwGpio_Read
HwGpio_Write
HwGpio_Toggle
└─ BSP/bsp_boot.c

HwSystem_IsVectorTableValid
HwSystem_JumpToVectorTable
HwSystem_GetTick
HwSystem_Delay
└─ BSP/bsp_boot.c
```

## 이후 확장

Flash 업데이트 단계에서 저장소 BSP를 추가합니다.

```text
BSP/
├── bsp_storage.c
├── bsp_storage.h
├── bsp_crc.c
└── bsp_crc.h
```

예상 책임:

```text
bsp_storage
├── Bootloader 영역 보호
├── Application 주소 범위 검사
├── Application Sector Erase 요청
├── Flash Write 요청
└── Read-back 요청

bsp_crc
└── 펌웨어 전체 CRC32 검증 요청
```
