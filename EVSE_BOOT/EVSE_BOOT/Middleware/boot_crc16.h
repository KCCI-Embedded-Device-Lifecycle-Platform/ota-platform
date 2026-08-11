#ifndef EVSE_BOOT_MW__BOOT_CRC16_H
#define EVSE_BOOT_MW__BOOT_CRC16_H

#include <stdint.h>

// defines
#define BOOT_CRC16_INITIAL_VALUE    0xFFFFU
#define BOOT_CRC16_POLYNOMIAL       0x1021U

// functions
uint16_t BootCrc16_Update(uint16_t crc, uint8_t data);
uint16_t BootCrc16_Calculate(const uint8_t *data, uint16_t length);

#endif // EVSE_BOOT_MW__BOOT_CRC16_H