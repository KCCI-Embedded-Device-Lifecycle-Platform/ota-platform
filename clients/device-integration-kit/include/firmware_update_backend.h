#ifndef OTA_DEVICE_INTEGRATION_KIT_FIRMWARE_UPDATE_BACKEND_H
#define OTA_DEVICE_INTEGRATION_KIT_FIRMWARE_UPDATE_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Device Update Backend internal status.
 *
 * These values are not LwM2M /5 Update Result values.
 * Firmware Update Service maps them explicitly.
 */


typedef enum
{
    FIRMWARE_BACKEND_STATUS_OK = 0,
    FIRMWARE_BACKEND_STATUS_NO_STORAGE,
    FIRMWARE_BACKEND_STATUS_OUT_OF_MEMORY,
    FIRMWARE_BACKEND_STATUS_INTEGRITY_FAILURE,
    FIRMWARE_BACKEND_STATUS_UNSUPPORTED_PACKAGE,
    FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE,
    FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE
} firmware_backend_status_t;

typedef enum
{
    FIRMWARE_BACKEND_RECOVERY_NONE = 0,
    FIRMWARE_BACKEND_RECOVERY_SUCCESS,
    FIRMWARE_BACKEND_RECOVERY_ROLLED_BACK
} firmware_backend_recovery_result_t;

typedef struct
{
    void *context;
    firmware_backend_status_t (*prepare)(void *context, size_t package_size);
    firmware_backend_status_t (*write_chunk)(void *context, size_t offset, const uint8_t *data, size_t length);
    firmware_backend_status_t (*finish_download)(void *context);
    firmware_backend_status_t (*install)(void *context);
    firmware_backend_status_t (*cancel)(void *context);
    firmware_backend_status_t (*recover_after_boot)(void *context, firmware_backend_recovery_result_t *recovery_result);
} firmware_update_backend_t;

#ifdef __cplusplus
}
#endif

#endif