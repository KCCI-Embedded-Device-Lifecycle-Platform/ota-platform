#ifndef OTA_DEVICE_INTEGRATION_KIT_WAKAAMA_OBJECT_FIRMWARE_H
#define OTA_DEVICE_INTEGRATION_KIT_WAKAAMA_OBJECT_FIRMWARE_H

#include "liblwm2m.h"
#include "firmware_update_service.h"
#include "firmware_download_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

lwm2m_object_t *get_firmware_update_object(
    firmware_update_service_t *service,
    firmware_download_transport_t *download_transport);
void free_firmware_update_object(lwm2m_object_t *objectP);

#ifdef __cplusplus
}
#endif

#endif
