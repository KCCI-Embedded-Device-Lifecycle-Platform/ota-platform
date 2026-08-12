#include "esp01_uart.h"

#define ESP01_UART_RX_BUFFER_SIZE 4096U

static UART_HandleTypeDef *esp_uart;

static uint8_t rx_buffer[ESP01_UART_RX_BUFFER_SIZE];
static uint8_t interrupt_rx_byte;

static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint32_t rx_overflow_count;

static uint16_t next_index(uint16_t index)
{
    index++;

    if (index >= ESP01_UART_RX_BUFFER_SIZE)
        index = 0U;

    return index;
}

bool esp01_uart_start(UART_HandleTypeDef *uart)
{
    if (uart == NULL)
        return false;

    esp_uart = uart;
    rx_head = 0U;
    rx_tail = 0U;
    rx_overflow_count = 0U;

    __HAL_UART_CLEAR_OREFLAG(esp_uart);

    return HAL_UART_Receive_IT(
               esp_uart,
               &interrupt_rx_byte,
               1U) == HAL_OK;
}

bool esp01_uart_read(uint8_t *byte)
{
    if (byte == NULL || rx_head == rx_tail)
        return false;

    *byte = rx_buffer[rx_tail];
    rx_tail = next_index(rx_tail);

    return true;
}

bool esp01_uart_write(
    const uint8_t *data,
    size_t length,
    uint32_t timeout_ms)
{
    if (esp_uart == NULL ||
        data == NULL ||
        length == 0U ||
        length > UINT16_MAX)
    {
        return false;
    }

    return HAL_UART_Transmit(
               esp_uart,
               data,
               (uint16_t)length,
               timeout_ms) == HAL_OK;
}

size_t esp01_uart_available(void)
{
    uint16_t head = rx_head;
    uint16_t tail = rx_tail;

    if (head >= tail)
        return head - tail;

    return ESP01_UART_RX_BUFFER_SIZE - tail + head;
}

void esp01_uart_discard(void)
{
    rx_tail = rx_head;
}

uint32_t esp01_uart_overflow_count(void)
{
    return rx_overflow_count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    uint16_t next;

    if (uart != esp_uart)
        return;

    next = next_index(rx_head);

    if (next == rx_tail)
    {
        rx_overflow_count++;
    }
    else
    {
        rx_buffer[rx_head] = interrupt_rx_byte;
        rx_head = next;
    }

    (void)HAL_UART_Receive_IT(
        esp_uart,
        &interrupt_rx_byte,
        1U);
}