#ifndef F429ZI_LWM2M_CLIENT_H
#define F429ZI_LWM2M_CLIENT_H

#include "esp01_transport.h"
#include "firmware_download_transport.h"
#include "firmware_update_service.h"
#include "wakaama_adapter.h"

#include <stdbool.h>
#include <stdint.h>

#define F429ZI_LWM2M_OBJECT_COUNT 4U
#define F429ZI_LWM2M_SERVER_URI_CAPACITY 64U

typedef struct
{
    esp01_transport_t esp01_transport;
    lwm2m_transport_t transport;
    wakaama_adapter_t adapter;
    lwm2m_context_t *context;
    lwm2m_object_t *objects[F429ZI_LWM2M_OBJECT_COUNT];
    firmware_update_service_t *firmware_service;
    firmware_download_transport_t *download_transport;

    lwm2m_client_state_t previous_state;
    firmware_update_state_t notified_firmware_state;
    firmware_update_result_t notified_update_result;

    char server_uri[F429ZI_LWM2M_SERVER_URI_CAPACITY];
    bool initialized;
} f429zi_lwm2m_client_t;

bool f429zi_lwm2m_client_init(
    f429zi_lwm2m_client_t *client,
    const char *endpoint,
    const char *server_host,
    uint16_t server_port,
    uint16_t local_port,
    firmware_update_service_t *firmware_service,
    firmware_download_transport_t *download_transport);

int f429zi_lwm2m_client_step(
    f429zi_lwm2m_client_t *client);

bool f429zi_lwm2m_client_is_ready(
    const f429zi_lwm2m_client_t *client);

#endif
