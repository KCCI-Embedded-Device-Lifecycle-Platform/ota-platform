#ifndef OTA_DEVICE_INTEGRATION_KIT_FIRMWARE_UPDATE_SERVICE_H
#define OTA_DEVICE_INTEGRATION_KIT_FIRMWARE_UPDATE_SERVICE_H

#include "firmware_update_backend.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    FIRMWARE_UPDATE_STATE_IDLE = 0,
    FIRMWARE_UPDATE_STATE_DOWNLOADING = 1,
    FIRMWARE_UPDATE_STATE_DOWNLOADED = 2,
    FIRMWARE_UPDATE_STATE_UPDATING = 3
} firmware_update_state_t;

typedef enum
{
    FIRMWARE_UPDATE_SEVERITY_CRITICAL = 0,
    FIRMWARE_UPDATE_SEVERITY_MANDATORY = 1,
    FIRMWARE_UPDATE_SEVERITY_OPTIONAL = 2
} firmware_update_severity_t;

typedef enum
{
    FIRMWARE_UPDATE_RESULT_INITIAL = 0,
    FIRMWARE_UPDATE_RESULT_SUCCESS = 1,
    FIRMWARE_UPDATE_RESULT_NO_STORAGE = 2,
    FIRMWARE_UPDATE_RESULT_OUT_OF_MEMORY = 3,
    FIRMWARE_UPDATE_RESULT_CONNECTION_LOST = 4,
    FIRMWARE_UPDATE_RESULT_INTEGRITY_FAILURE = 5,
    FIRMWARE_UPDATE_RESULT_UNSUPPORTED_PACKAGE = 6,
    FIRMWARE_UPDATE_RESULT_INVALID_URI = 7,
    FIRMWARE_UPDATE_RESULT_UPDATE_FAILURE = 8,
    FIRMWARE_UPDATE_RESULT_UNSUPPORTED_PROTOCOL = 9,
    FIRMWARE_UPDATE_RESULT_CANCELLED = 10,
    FIRMWARE_UPDATE_RESULT_DEFERRED = 11
} firmware_update_result_t;

typedef enum
{
    FIRMWARE_UPDATE_SERVICE_STATUS_OK = 0,
    FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT,
    FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE,
    FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE,
    FIRMWARE_UPDATE_SERVICE_STATUS_NOT_ALLOWED
} firmware_update_service_status_t;

typedef struct
{
    firmware_update_backend_t backend;
    firmware_update_state_t state;
    firmware_update_result_t update_result;
    firmware_update_severity_t severity;
    uint64_t maximum_defer_period_seconds;
    size_t download_offset;
    
} firmware_update_service_t;

bool firmware_update_service_init(
    firmware_update_service_t *service,
    const firmware_update_backend_t *backend);

firmware_update_service_status_t firmware_update_service_begin_download(
    firmware_update_service_t *service,
    size_t package_size);

firmware_update_service_status_t firmware_update_service_write_chunk(
    firmware_update_service_t *service,
    const uint8_t *data,
    size_t length);

firmware_update_service_status_t firmware_update_service_finish_download(
    firmware_update_service_t *service);

firmware_update_service_status_t firmware_update_service_install(
    firmware_update_service_t *service);

firmware_update_service_status_t firmware_update_service_cancel(
    firmware_update_service_t *service);

firmware_update_service_status_t firmware_update_service_recover_after_boot(
    firmware_update_service_t *service);

firmware_update_service_status_t firmware_update_service_set_severity(
    firmware_update_service_t *service,
    firmware_update_severity_t severity);

firmware_update_service_status_t firmware_update_service_set_maximum_defer_period(
    firmware_update_service_t *service,
    uint64_t seconds);

firmware_update_service_status_t firmware_update_service_defer(
    firmware_update_service_t *service);

#ifdef __cplusplus
}
#endif

#endif