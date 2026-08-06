#ifndef OTA_DEVICE_INTEGRATION_KIT_FIRMWARE_DOWNLOAD_TRANSPORT_H
#define OTA_DEVICE_INTEGRATION_KIT_FIRMWARE_DOWNLOAD_TRANSPORT_H

#include "firmware_update_service.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * start() accepts a download request and returns without waiting for the
 * complete firmware image.
 *
 * An asynchronous implementation must copy the URI before returning, then
 * feed received data through:
 *
 *   firmware_update_service_begin_download()
 *   firmware_update_service_write_chunk()
 *   firmware_update_service_finish_download()
 *
 * The complete firmware image must not be buffered in RAM.
 *
 * cancel() must stop future chunk delivery and be safe to call even when
 * no download is active.
 */

typedef enum
{
    FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK = 0,
    FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI,
    FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_UNSUPPORTED_PROTOCOL,
    FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE,
    FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE
} firmware_download_transport_status_t;

typedef struct
{
    void *context;

    firmware_download_transport_status_t (*start)(
        void *context,
        const char *uri,
        size_t uri_length,
        firmware_update_service_t *service);

    firmware_download_transport_status_t (*cancel)(
        void *context);

} firmware_download_transport_t;

#ifdef __cplusplus
}
#endif

#endif