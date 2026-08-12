#include "f429zi_lwm2m_client.h"

#include "object_firmware.h"
#include "standard_objects.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

enum
{
    SECURITY_OBJECT_INDEX = 0,
    SERVER_OBJECT_INDEX,
    DEVICE_OBJECT_INDEX,
    FIRMWARE_OBJECT_INDEX
};

enum
{
    LWM2M_SERVER_SHORT_ID = 123,
    LWM2M_SERVER_LIFETIME_SECONDS = 300,
    LWM2M_ESP01_LINK_ID = 0
};

int g_reboot;

static void destroy_objects(f429zi_lwm2m_client_t *client)
{
    if (client->objects[FIRMWARE_OBJECT_INDEX] != NULL)
    {
        free_firmware_update_object(
            client->objects[FIRMWARE_OBJECT_INDEX]);
        client->objects[FIRMWARE_OBJECT_INDEX] = NULL;
    }

    if (client->objects[DEVICE_OBJECT_INDEX] != NULL)
    {
        free_object_device(client->objects[DEVICE_OBJECT_INDEX]);
        client->objects[DEVICE_OBJECT_INDEX] = NULL;
    }

    if (client->objects[SERVER_OBJECT_INDEX] != NULL)
    {
        clean_server_object(client->objects[SERVER_OBJECT_INDEX]);
        lwm2m_free(client->objects[SERVER_OBJECT_INDEX]);
        client->objects[SERVER_OBJECT_INDEX] = NULL;
    }

    if (client->objects[SECURITY_OBJECT_INDEX] != NULL)
    {
        clean_security_object(client->objects[SECURITY_OBJECT_INDEX]);
        lwm2m_free(client->objects[SECURITY_OBJECT_INDEX]);
        client->objects[SECURITY_OBJECT_INDEX] = NULL;
    }
}

static bool create_objects(f429zi_lwm2m_client_t *client)
{
    client->objects[SECURITY_OBJECT_INDEX] =
        get_security_object(
            LWM2M_SERVER_SHORT_ID,
            client->server_uri,
            NULL,
            NULL,
            0U,
            false);

    client->objects[SERVER_OBJECT_INDEX] =
        get_server_object(
            LWM2M_SERVER_SHORT_ID,
            "U",
            LWM2M_SERVER_LIFETIME_SECONDS,
            false);

    client->objects[DEVICE_OBJECT_INDEX] =
        get_object_device();

    client->objects[FIRMWARE_OBJECT_INDEX] =
        get_firmware_update_object(
            client->firmware_service,
            client->download_transport);

    if (client->objects[SECURITY_OBJECT_INDEX] == NULL ||
        client->objects[SERVER_OBJECT_INDEX] == NULL ||
        client->objects[DEVICE_OBJECT_INDEX] == NULL ||
        client->objects[FIRMWARE_OBJECT_INDEX] == NULL)
    {
        destroy_objects(client);
        return false;
    }

    return true;
}

static void notify_firmware_changes(
    f429zi_lwm2m_client_t *client)
{
    lwm2m_uri_t uri;

    if (client->firmware_service->state !=
        client->notified_firmware_state)
    {
        printf(
            "[OTA KIT] state=%u\r\n",
            (unsigned)client->firmware_service->state);

        LWM2M_URI_RESET(&uri);
        uri.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID;
        uri.instanceId = 0U;
        uri.resourceId = 3U;

        lwm2m_resource_value_changed(client->context, &uri);
        client->notified_firmware_state =
            client->firmware_service->state;
    }

    if (client->firmware_service->update_result !=
        client->notified_update_result)
    {
        printf(
            "[OTA KIT] updateResult=%u\r\n",
            (unsigned)client->firmware_service->update_result);

        LWM2M_URI_RESET(&uri);
        uri.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID;
        uri.instanceId = 0U;
        uri.resourceId = 5U;

        lwm2m_resource_value_changed(client->context, &uri);
        client->notified_update_result =
            client->firmware_service->update_result;
    }
}

static void log_state_change(f429zi_lwm2m_client_t *client)
{
    if (client->context->state == client->previous_state)
        return;

    if (client->context->state == STATE_REGISTERING)
        printf("[LwM2M] state=REGISTERING\r\n");
    else if (client->context->state == STATE_READY)
        printf("[LwM2M] state=READY\r\n");
    else
        printf(
            "[LwM2M] state=%u\r\n",
            (unsigned)client->context->state);

    client->previous_state = client->context->state;
}

bool f429zi_lwm2m_client_init(
    f429zi_lwm2m_client_t *client,
    const char *endpoint,
    const char *server_host,
    uint16_t server_port,
    uint16_t local_port,
    firmware_update_service_t *firmware_service,
    firmware_download_transport_t *download_transport)
{
    int uri_length;
    int configure_result;

    if (client == NULL ||
        endpoint == NULL ||
        endpoint[0] == '\0' ||
        server_host == NULL ||
        server_host[0] == '\0' ||
        server_port == 0U ||
        local_port == 0U ||
        firmware_service == NULL ||
        download_transport == NULL)
    {
        return false;
    }

    memset(client, 0, sizeof(*client));
    client->firmware_service = firmware_service;
    client->download_transport = download_transport;

    uri_length = snprintf(
        client->server_uri,
        sizeof(client->server_uri),
        "coap://%s:%u",
        server_host,
        (unsigned)server_port);

    if (uri_length <= 0 ||
        (size_t)uri_length >= sizeof(client->server_uri))
    {
        return false;
    }

    if (!esp01_transport_init(
            &client->esp01_transport,
            server_host,
            server_port,
            local_port,
            LWM2M_ESP01_LINK_ID,
            &client->transport) ||
        !wakaama_adapter_init(
            &client->adapter,
            &client->transport) ||
        !create_objects(client))
    {
        destroy_objects(client);
        return false;
    }

    client->context = lwm2m_init(&client->adapter);

    if (client->context == NULL)
    {
        destroy_objects(client);
        return false;
    }

    wakaama_adapter_set_context(
        &client->adapter,
        client->context);

    configure_result = lwm2m_configure(
        client->context,
        endpoint,
        NULL,
        NULL,
        F429ZI_LWM2M_OBJECT_COUNT,
        client->objects);

    if (configure_result != COAP_NO_ERROR)
    {
        lwm2m_close(client->context);
        client->context = NULL;
        destroy_objects(client);
        return false;
    }

    client->previous_state = client->context->state;
    client->notified_firmware_state = firmware_service->state;
    client->notified_update_result = firmware_service->update_result;
    client->initialized = true;

    printf(
        "[LwM2M] endpoint=%s, server=%s\r\n",
        endpoint,
        client->server_uri);

    return true;
}

int f429zi_lwm2m_client_step(
    f429zi_lwm2m_client_t *client)
{
    time_t timeout_seconds = 1;
    int result;

    if (client == NULL ||
        !client->initialized ||
        client->context == NULL)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    wakaama_adapter_poll(&client->adapter);
    notify_firmware_changes(client);

    result = lwm2m_step(
        client->context,
        &timeout_seconds);

    log_state_change(client);
    return result;
}

bool f429zi_lwm2m_client_is_ready(
    const f429zi_lwm2m_client_t *client)
{
    return client != NULL &&
           client->initialized &&
           client->context != NULL &&
           client->context->state == STATE_READY;
}
