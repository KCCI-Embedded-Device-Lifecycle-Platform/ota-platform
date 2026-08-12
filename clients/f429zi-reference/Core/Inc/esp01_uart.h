#ifndef ESP01_UART_H
#define ESP01_UART_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool esp01_uart_start(UART_HandleTypeDef *uart);
bool esp01_uart_read(uint8_t *byte);
bool esp01_uart_write(
    const uint8_t *data,
    size_t length,
    uint32_t timeout_ms);
size_t esp01_uart_available(void);
void esp01_uart_discard(void);
uint32_t esp01_uart_overflow_count(void);

#endif