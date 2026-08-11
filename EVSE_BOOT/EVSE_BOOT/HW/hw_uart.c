#include "HW/hw_uart.h"

#include <stddef.h>

// inners
static hw_status_t HwUart_ConvertHalStatus(HAL_StatusTypeDef hal_status) {
    switch (hal_status) {
        case HAL_OK:        return HW_STATUS_OK;
        case HAL_TIMEOUT:   return HW_STATUS_TIMEOUT;
        case HAL_BUSY:      return HW_STATUS_BUSY;
        case HAL_ERROR:
        default:            return HW_STATUS_ERROR;
    }
}

// functions
hw_status_t HwUart_Attach(hw_uart_t *uart, UART_HandleTypeDef *hal_uart) {
    // HAL UART Handle >> HW UART 객체
    if ((uart == NULL) || (hal_uart == NULL)) return HW_STATUS_INVALID_ARGUMENT;

    uart->handle = hal_uart;

    return HW_STATUS_OK;
}

hw_status_t HwUart_Write(hw_uart_t *uart, const uint8_t *data, uint16_t length, uint32_t timeout_ms) {
    // 지정된 길이 데이터 전송
    HAL_StatusTypeDef hal_status;

    if ((uart == NULL) || (data == NULL) || (length == 0U)) return HW_STATUS_INVALID_ARGUMENT;

    if (uart->handle == NULL) return HW_STATUS_NOT_INITIALIZED;

    hal_status = HAL_UART_Transmit(uart->handle, (uint8_t *)data, length, timeout_ms);

    return HwUart_ConvertHalStatus(hal_status);
}

hw_status_t HwUart_ReadByte(hw_uart_t *uart, uint8_t *received_byte, uint32_t timeout_ms) {
    // UART에서 1바이트 수신
    HAL_StatusTypeDef hal_status;

    if ((uart == NULL) || (received_byte == NULL)) return HW_STATUS_INVALID_ARGUMENT;

    if (uart->handle == NULL) return HW_STATUS_NOT_INITIALIZED;

    hal_status = HAL_UART_Receive(uart->handle, received_byte, 1U, timeout_ms);

    return HwUart_ConvertHalStatus(hal_status);
}