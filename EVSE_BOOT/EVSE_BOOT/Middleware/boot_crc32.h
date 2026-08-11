#ifndef EVSE_BOOT_MW__BOOT_CRC32_H
#define EVSE_BOOT_MW__BOOT_CRC32_H

#include <stdint.h>

#include "Middleware/mw_common.h"

// defines
#define BOOT_CRC32_INITIAL_VALUE        0xFFFFFFFFUL
#define BOOT_CRC32_FINAL_XOR_VALUE      0xFFFFFFFFUL
#define BOOT_CRC32_REFLECTED_POLYNOMIAL 0xEDB88320UL

// functions
uint32_t BootCrc32_Begin(void);

mw_status_t BootCrc32_Update(uint32_t *crc, const uint8_t *data, uint32_t length);

uint32_t BootCrc32_End(uint32_t crc);

mw_status_t BootCrc32_Calculate(const uint8_t *data, uint32_t length, uint32_t *result);

#endif // EVSE_BOOT_MW__BOOT_CRC32_H