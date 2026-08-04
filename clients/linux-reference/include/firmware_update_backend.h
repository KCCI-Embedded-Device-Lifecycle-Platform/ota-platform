#ifndef OTA_LINUX_REFERENCE_CLIENT_FIRMWARE_UPDATE_BACKEND_H
#define OTA_LINUX_REFERENCE_CLIENT_FIRMWARE_UPDATE_BACKEND_H

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

#ifdef __cplusplus
}
#endif

#endif