#include "boot_crc16.h"

#include <stddef.h>

uint16_t BootCrc16_Update(uint16_t crc, uint8_t data) {
    uint8_t bit_index;

    crc ^= (uint16_t)data << 8U;

    for (bit_index = 0U; bit_index < 8U; bit_index++) {
        if ((crc & 0x8000U) != 0U) crc = (uint16_t)((crc << 1U) ^ BOOT_CRC16_POLYNOMIAL);
        else                       crc = (uint16_t)(crc << 1U);
    }

    return crc;
}

uint16_t BootCrc16_Calculate(const uint8_t *data, uint16_t length) {
    uint16_t crc;
    uint16_t index;

    if ((data == NULL) && (length > 0U)) return 0U;

    crc = BOOT_CRC16_INITIAL_VALUE;

    for (index = 0U; index < length; index++) crc = BootCrc16_Update(crc, data[index]);

    return crc;
}