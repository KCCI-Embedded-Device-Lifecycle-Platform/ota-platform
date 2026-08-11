#include "bsp_transport.h"

#include <stddef.h>

#include "usart.h"

#include "Driver/drv_uart_transport.h"
#include "HW/hw_uart.h"
#include "Common/boot_config.h"

// inners
static hw_uart_t s_ota_hw_uart;

static drv_uart_transport_t s_ota_uart_transport;

static bool s_initialized;

static bsp_status_t BspTransport_ConvertHwStatus(hw_status_t status) {
    switch (status) {
        case HW_STATUS_OK:               return BSP_STATUS_OK;
        case HW_STATUS_TIMEOUT:          return BSP_STATUS_TIMEOUT;
        case HW_STATUS_BUSY:             return BSP_STATUS_BUSY;
        case HW_STATUS_INVALID_ARGUMENT: return BSP_STATUS_INVALID_ARGUMENT;
        case HW_STATUS_NOT_INITIALIZED:  return BSP_STATUS_NOT_INITIALIZED;
        case HW_STATUS_ERROR:
        default:                         return BSP_STATUS_ERROR;
    }
}

static bsp_status_t BspTransport_ConvertDriverStatus(drv_status_t status) {
    switch (status) {
        case DRV_STATUS_OK:               return BSP_STATUS_OK;
        case DRV_STATUS_TIMEOUT:          return BSP_STATUS_TIMEOUT;
        case DRV_STATUS_BUSY:             return BSP_STATUS_BUSY;
        case DRV_STATUS_INVALID_ARGUMENT: return BSP_STATUS_INVALID_ARGUMENT;
        case DRV_STATUS_NOT_INITIALIZED:  return BSP_STATUS_NOT_INITIALIZED;
        case DRV_STATUS_ERROR:
        default:                          return BSP_STATUS_ERROR;
    }
}

// functions
bsp_status_t BspTransport_Init(void) {
    // 보드에서 OTA 통신에 사용할 Transport 초기화
    hw_status_t hw_status;
    drv_status_t driver_status;

    if (s_initialized) return BSP_STATUS_OK;
    
    hw_status = HwUart_Attach(&s_ota_hw_uart, &huart3);

    if (hw_status != HW_STATUS_OK) return BspTransport_ConvertHwStatus(hw_status);

    driver_status = DrvUartTransport_Init(&s_ota_uart_transport, &s_ota_hw_uart, BOOT_UART_TX_TIMEOUT_MS, BOOT_UART_RX_TIMEOUT_MS);

    if (driver_status != DRV_STATUS_OK) return BspTransport_ConvertDriverStatus(driver_status);

    s_initialized = true;

    return BSP_STATUS_OK;
}

void BspTransport_Deinit(void) {
    // BSP Transport 객체만 초기화
    DrvUartTransport_Deinit(&s_ota_uart_transport);

    s_initialized = false;
}

bsp_status_t BspTransport_Send(const uint8_t *data, uint16_t length) {
    // 지정한 바이트 배열 전송
    drv_status_t driver_status;

    if ((data == NULL) || (length == 0U)) return BSP_STATUS_INVALID_ARGUMENT;

    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;

    driver_status = DrvUartTransport_Send(&s_ota_uart_transport, data, length);

    return BspTransport_ConvertDriverStatus(driver_status);
}

bsp_status_t BspTransport_ReceiveByte(uint8_t *received_byte) {
    // 1바이트 수신
    drv_status_t driver_status;

    if (received_byte == NULL) return BSP_STATUS_INVALID_ARGUMENT;

    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;

    driver_status = DrvUartTransport_ReceiveByte(&s_ota_uart_transport, received_byte);

    return BspTransport_ConvertDriverStatus(driver_status);
}

bool BspTransport_IsInitialized(void) {
    // Transport 초기화 여부 확인
    return s_initialized && DrvUartTransport_IsInitialized(&s_ota_uart_transport);
}