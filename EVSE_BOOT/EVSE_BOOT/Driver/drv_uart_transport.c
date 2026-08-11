#include "drv_uart_transport.h"

#include <stddef.h>

// inners
static drv_status_t DrvUartTransport_ConvertHwStatus(hw_status_t hw_status) {
    switch (hw_status) {
        case HW_STATUS_OK:               return DRV_STATUS_OK;
        case HW_STATUS_TIMEOUT:          return DRV_STATUS_TIMEOUT;
        case HW_STATUS_BUSY:             return DRV_STATUS_BUSY;
        case HW_STATUS_INVALID_ARGUMENT: return DRV_STATUS_INVALID_ARGUMENT;
        case HW_STATUS_NOT_INITIALIZED:  return DRV_STATUS_NOT_INITIALIZED;
        case HW_STATUS_ERROR:
        default:                         return DRV_STATUS_ERROR;
    }
}

// functions
drv_status_t DrvUartTransport_Init(drv_uart_transport_t *transport, hw_uart_t *hw_uart, uint32_t tx_timeout_ms, uint32_t rx_timeout_ms) {
    // UART Transport Driver를 초기화합니다.
    // transport:     Driver 객체
    // hw_uart:       이미 HwUart_Attach()가 완료된 HW UART 객체
    // tx_timeout_ms: 전송 제한 시간
    // rx_timeout_ms: 1바이트 수신 제한 시간
    if ((transport == NULL) || (hw_uart == NULL)) return DRV_STATUS_INVALID_ARGUMENT;

    if (hw_uart->handle == NULL) return DRV_STATUS_NOT_INITIALIZED;

    transport->hw_uart = hw_uart;

    transport->tx_timeout_ms = tx_timeout_ms;
    transport->rx_timeout_ms = rx_timeout_ms;

    transport->initialized = true;

    return DRV_STATUS_OK;
}

void DrvUartTransport_Deinit(drv_uart_transport_t *transport) {
    // Driver 상태를 초기화합니다.
    // HAL UART 자체를 DeInit하지는 않습니다.
    if (transport == NULL) return;

    transport->hw_uart = NULL;

    transport->tx_timeout_ms = 0U;
    transport->rx_timeout_ms = 0U;

    transport->initialized = false;
}

drv_status_t DrvUartTransport_Send(drv_uart_transport_t *transport, const uint8_t *data, uint16_t length) {
    // 지정된 길이의 바이트 데이터를 전송합니다.
    hw_status_t hw_status;

    if ((transport == NULL) || (data == NULL) || (length == 0U)) return DRV_STATUS_INVALID_ARGUMENT;

    if ((!transport->initialized) || (transport->hw_uart == NULL)) return DRV_STATUS_NOT_INITIALIZED;

    hw_status = HwUart_Write(transport->hw_uart, data, length, transport->tx_timeout_ms);

    return DrvUartTransport_ConvertHwStatus(hw_status);
}

drv_status_t DrvUartTransport_ReceiveByte(drv_uart_transport_t *transport, uint8_t *received_byte) {
    // UART에서 1바이트를 수신합니다.
    // 데이터가 없고 제한 시간이 지나면 DRV_STATUS_TIMEOUT을 반환합니다.
    hw_status_t hw_status;

    if ((transport == NULL) || (received_byte == NULL)) return DRV_STATUS_INVALID_ARGUMENT;

    if ((!transport->initialized) || (transport->hw_uart == NULL)) return DRV_STATUS_NOT_INITIALIZED;

    hw_status = HwUart_ReadByte(transport->hw_uart, received_byte, transport->rx_timeout_ms);

    return DrvUartTransport_ConvertHwStatus(hw_status);
}

bool DrvUartTransport_IsInitialized(const drv_uart_transport_t *transport) {
    // Driver 초기화 여부를 반환합니다.
    if (transport == NULL) return false;

    return transport->initialized && (transport->hw_uart != NULL);
}