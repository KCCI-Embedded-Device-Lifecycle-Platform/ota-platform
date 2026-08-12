#ifndef F429ZI_FIRMWARE_BACKEND_H
#define F429ZI_FIRMWARE_BACKEND_H

#include "firmware_update_backend.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    size_t expected_size;
    size_t written_size;
    uint32_t running_crc32;
    uint32_t image_crc32;
    uint32_t install_requested_at_ms;
    bool prepared;
    bool package_ready;
    bool install_pending;
} f429zi_firmware_backend_t;

bool f429zi_firmware_backend_init(
    f429zi_firmware_backend_t *context,
    firmware_update_backend_t *backend);

void f429zi_firmware_backend_process(
    f429zi_firmware_backend_t *context);

#endif
