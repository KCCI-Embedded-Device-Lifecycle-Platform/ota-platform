#ifndef EVSE_BOOT_BSP__BSP_TRANSPORT_H
#define EVSE_BOOT_BSP__BSP_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "BSP/bsp_common.h"

// functions
bsp_status_t BspTransport_Init(void);
void BspTransport_Deinit(void);

bsp_status_t BspTransport_Send(const uint8_t *data, uint16_t length);
bsp_status_t BspTransport_ReceiveByte(uint8_t *received_byte);
bool BspTransport_IsInitialized(void);

#endif // EVSE_BOOT_BSP__BSP_TRANSPORT_H