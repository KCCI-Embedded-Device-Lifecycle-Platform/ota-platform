#ifndef EVSE_BOOT_HW__HW_UART_H
#define EVSE_BOOT_HW__HW_UART_H

#include <stdint.h>

#include "stm32f4xx_hal.h"

#include "HW/hw_common.h"

// tables
typedef struct {
    UART_HandleTypeDef *handle;
} hw_uart_t;

// functions
hw_status_t HwUart_Attach(hw_uart_t *uart, UART_HandleTypeDef *hal_uart);

hw_status_t HwUart_Write(hw_uart_t *uart, const uint8_t *data, uint16_t length, uint32_t timeout_ms);

hw_status_t HwUart_ReadByte(hw_uart_t *uart, uint8_t *received_byte, uint32_t timeout_ms);

#endif // EVSE_BOOT_HW__HW_UART_H