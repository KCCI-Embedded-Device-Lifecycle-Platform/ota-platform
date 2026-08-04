#ifndef OTA_LINUX_REFERENCE_FIRMWARE_UPDATE_BACKEND_H
#define OTA_LINUX_REFERENCE_FIRMWARE_UPDATE_BACKEND_H

#include "firmware_update_backend.h"

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *staging_path;
    FILE *staging_file;
    size_t expected_size;
    size_t received_size;
    bool package_ready;
    bool install_requested;
} linux_firmware_update_backend_context_t;

bool linux_firmware_update_backend_init(
    linux_firmware_update_backend_context_t *context,
    const char *staging_path,
    firmware_update_backend_t *backend);

void linux_firmware_update_backend_deinit(
    linux_firmware_update_backend_context_t *context);

#ifdef __cplusplus
}
#endif

#endif