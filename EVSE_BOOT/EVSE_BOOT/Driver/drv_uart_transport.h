#ifndef EVSE_BOOT_DRIVER__DRV_UART_TRANSPORT_H
#define EVSE_BOOT_DRIVER__DRV_UART_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "Driver/drv_common.h"
#include "HW/hw_uart.h"

// tables
typedef struct {
    hw_uart_t *hw_uart;

    uint32_t tx_timeout_ms;
    uint32_t rx_timeout_ms;

    bool initialized;
} drv_uart_transport_t;

// functions
drv_status_t DrvUartTransport_Init(drv_uart_transport_t *transport, hw_uart_t *hw_uart, uint32_t tx_timeout_ms, uint32_t rx_timeout_ms);
void DrvUartTransport_Deinit(drv_uart_transport_t *transport);

drv_status_t DrvUartTransport_Send(drv_uart_transport_t *transport, const uint8_t *data, uint16_t length);
drv_status_t DrvUartTransport_ReceiveByte(drv_uart_transport_t *transport, uint8_t *received_byte);

bool DrvUartTransport_IsInitialized(const drv_uart_transport_t *transport);

#endif // EVSE_BOOT_DRIVER__DRV_UART_TRANSPORT_H