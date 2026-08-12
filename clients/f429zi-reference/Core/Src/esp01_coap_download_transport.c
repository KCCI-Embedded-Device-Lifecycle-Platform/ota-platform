#include "esp01_coap_download_transport.h"

#include "esp01_modem.h"
#include "er-coap-13.h"
#include "main.h"

#include <stdio.h>
#include <string.h>

#define COAP_DEFAULT_PORT             5683U
#define COAP_REQUESTED_BLOCK_SIZE     512U
#define COAP_INITIAL_RETRY_TIMEOUT_MS 2000U
#define COAP_MAX_RETRY_TIMEOUT_MS     8000U
#define COAP_MAX_RETRY_COUNT          4U

static uint16_t next_message_id = 0x4000U;

static void close_request(
    esp01_coap_download_transport_t *context)
{
    if (context->state != ESP01_COAP_DOWNLOAD_IDLE)
        (void)esp01_modem_close(context->link_id);

    context->service = NULL;
    context->state = ESP01_COAP_DOWNLOAD_IDLE;
    context->host = NULL;
    context->path = NULL;
    context->query = NULL;
    context->block_number = 0U;
    context->block_size = COAP_REQUESTED_BLOCK_SIZE;
    context->total_size = 0U;
    context->retry_count = 0U;
    context->retry_timeout_ms = COAP_INITIAL_RETRY_TIMEOUT_MS;
    context->response_deadline_ms = 0U;
    context->last_progress_percent = 0U;
    context->request_length = 0U;
    context->uri[0] = '\0';
}

static firmware_download_transport_status_t fail_request(
    esp01_coap_download_transport_t *context,
    firmware_update_download_failure_t failure,
    firmware_download_transport_status_t status)
{
    firmware_update_service_t *service = context->service;

    close_request(context);

    if (service != NULL)
        (void)firmware_update_service_fail_download(service, failure);

    return status;
}

static bool parse_port(
    const char *text,
    uint16_t *port)
{
    uint32_t value = 0U;

    if (text == NULL || text[0] == '\0')
        return false;

    while (*text != '\0')
    {
        if (*text < '0' || *text > '9')
            return false;

        value = value * 10U + (uint32_t)(*text - '0');

        if (value > 65535U)
            return false;

        text++;
    }

    if (value == 0U)
        return false;

    *port = (uint16_t)value;
    return true;
}

static firmware_download_transport_status_t parse_uri(
    esp01_coap_download_transport_t *context,
    const char *uri,
    size_t uri_length)
{
    char *authority;
    char *path_separator;
    char *query_separator;
    char *port_separator;

    if (uri == NULL || uri_length == 0U ||
        uri_length >= sizeof(context->uri))
    {
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;
    }

    memcpy(context->uri, uri, uri_length);
    context->uri[uri_length] = '\0';

    if (strncmp(context->uri, "coap://", 7U) != 0)
    {
        if (strstr(context->uri, "://") != NULL)
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_UNSUPPORTED_PROTOCOL;

        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;
    }

    authority = context->uri + 7U;

    if (authority[0] == '\0')
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;

    path_separator = strchr(authority, '/');
    query_separator = strchr(authority, '?');

    if (query_separator != NULL &&
        (path_separator == NULL || query_separator < path_separator))
    {
        path_separator = query_separator;
    }

    if (path_separator != NULL)
    {
        char separator = *path_separator;
        *path_separator = '\0';

        if (separator == '/')
        {
            context->path = path_separator + 1U;
            query_separator = strchr(context->path, '?');

            if (query_separator != NULL)
            {
                *query_separator = '\0';
                context->query = query_separator + 1U;
            }
        }
        else
        {
            context->path = NULL;
            context->query = path_separator + 1U;
        }
    }
    else
    {
        context->path = NULL;
        context->query = NULL;
    }

    if (authority[0] == '\0' || strchr(authority, '[') != NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;

    port_separator = strrchr(authority, ':');
    context->remote_port = COAP_DEFAULT_PORT;

    if (port_separator != NULL)
    {
        *port_separator = '\0';

        if (!parse_port(
                port_separator + 1U,
                &context->remote_port))
        {
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;
        }
    }

    if (authority[0] == '\0')
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;

    context->host = authority;
    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

static firmware_download_transport_status_t start_download(
    void *raw_context,
    const char *uri,
    size_t uri_length,
    firmware_update_service_t *service)
{
    esp01_coap_download_transport_t *context = raw_context;
    firmware_download_transport_status_t status;

    if (context == NULL || service == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    if (context->state != ESP01_COAP_DOWNLOAD_IDLE)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    status = parse_uri(context, uri, uri_length);

    if (status != FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK)
    {
        context->uri[0] = '\0';
        return status;
    }

    context->service = service;
    context->block_number = 0U;
    context->block_size = COAP_REQUESTED_BLOCK_SIZE;
    context->total_size = 0U;
    context->retry_count = 0U;
    context->retry_timeout_ms = COAP_INITIAL_RETRY_TIMEOUT_MS;
    context->last_progress_percent = 0U;
    context->state = ESP01_COAP_DOWNLOAD_OPENING;

    printf("[OTA] download accepted: coap://%s:%u/%s\r\n",
           context->host,
           (unsigned)context->remote_port,
           context->path == NULL ? "" : context->path);

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

static firmware_download_transport_status_t cancel_download(
    void *raw_context)
{
    esp01_coap_download_transport_t *context = raw_context;

    if (context == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    close_request(context);
    printf("[OTA] download cancelled\r\n");
    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

static bool prepare_request(
    esp01_coap_download_transport_t *context,
    size_t *request_length)
{
    coap_packet_t request;
    size_t required_size;

    context->message_id = ++next_message_id;
    context->token[0] = (uint8_t)(context->message_id >> 8U);
    context->token[1] = (uint8_t)context->message_id;

    coap_init_message(
        &request,
        COAP_TYPE_CON,
        COAP_GET,
        context->message_id);

    if (!coap_set_header_token(
            &request,
            context->token,
            sizeof(context->token)) ||
        !coap_set_header_uri_host(&request, context->host))
    {
        coap_free_header(&request);
        return false;
    }

    if (context->remote_port != COAP_DEFAULT_PORT)
        coap_set_header_uri_port(&request, context->remote_port);

    if (context->path != NULL && context->path[0] != '\0')
        (void)coap_set_header_uri_path(&request, context->path);

    if (context->query != NULL && context->query[0] != '\0')
        (void)coap_set_header_uri_query(&request, context->query);

    if (!coap_set_header_block2(
            &request,
            context->block_number,
            0U,
            context->block_size))
    {
        coap_free_header(&request);
        return false;
    }

    required_size = coap_serialize_get_size(&request);

    if (required_size > sizeof(context->request_buffer))
    {
        coap_free_header(&request);
        return false;
    }

    *request_length = coap_serialize_message(
        &request,
        context->request_buffer);

    context->request_length = *request_length;

    return *request_length > 0U &&
           *request_length <= sizeof(context->request_buffer);
}

static firmware_download_transport_status_t send_request(
    esp01_coap_download_transport_t *context,
    bool retransmission)
{
    size_t request_length;

    if (!retransmission && !prepare_request(context, &request_length))
    {
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
    }

    if (retransmission)
    {
        request_length = context->request_length;

        if (request_length == 0U ||
            request_length > sizeof(context->request_buffer))
        {
            return fail_request(
                context,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
        }
    }

    if (esp01_modem_send_udp(
            context->link_id,
            context->request_buffer,
            request_length) != ESP01_MODEM_STATUS_OK)
    {
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE);
    }

    context->response_deadline_ms =
        HAL_GetTick() + context->retry_timeout_ms;
    context->state = ESP01_COAP_DOWNLOAD_WAITING;
    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

static bool token_matches(
    const esp01_coap_download_transport_t *context,
    coap_packet_t *response)
{
    uint8_t *token;
    int token_length = coap_get_header_token(response, &token);

    return token_length == 2 &&
           token[0] == context->token[0] &&
           token[1] == context->token[1];
}

static firmware_download_transport_status_t process_response(
    esp01_coap_download_transport_t *context,
    size_t response_length)
{
    coap_packet_t response;
    const uint8_t *payload;
    size_t payload_length;
    uint32_t block_number = 0U;
    uint32_t block_offset = 0U;
    uint32_t total_size = 0U;
    uint16_t block_size = 0U;
    uint8_t more = 0U;
    firmware_update_service_status_t service_status;

    if (response_length > UINT16_MAX ||
        coap_parse_message(
            &response,
            context->response_buffer,
            (uint16_t)response_length) != NO_ERROR)
    {
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
    }

    if (response.code != CONTENT_2_05 ||
        response.mid != context->message_id ||
        !token_matches(context, &response))
    {
        coap_free_header(&response);
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INVALID_URI,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI);
    }

    payload_length = coap_get_payload(&response, &payload);

    if (payload == NULL || payload_length == 0U)
    {
        coap_free_header(&response);
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
    }

    if (coap_get_header_block2(
            &response,
            &block_number,
            &more,
            &block_size,
            &block_offset))
    {
        if (block_number != context->block_number ||
            block_size == 0U ||
            block_offset !=
                context->service->download_offset)
        {
            coap_free_header(&response);
            return fail_request(
                context,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
        }
    }
    else if (context->block_number != 0U)
    {
        coap_free_header(&response);
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
    }
    else
    {
        block_size = (uint16_t)payload_length;
        block_offset = 0U;
        more = 0U;
    }

    if (context->total_size == 0U)
    {
        if (!coap_get_header_size(&response, &total_size) ||
            total_size == 0U)
        {
            coap_free_header(&response);
            return fail_request(
                context,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
        }

        context->total_size = total_size;

        service_status = firmware_update_service_begin_download(
            context->service,
            context->total_size);

        if (service_status != FIRMWARE_UPDATE_SERVICE_STATUS_OK)
        {
            coap_free_header(&response);
            close_request(context);
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
        }

        printf(
            "[OTA] package size=%lu bytes\r\n",
            (unsigned long)context->total_size);
    }

    if (block_offset > context->total_size ||
        payload_length > context->total_size - block_offset)
    {
        coap_free_header(&response);
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
    }

    service_status = firmware_update_service_write_chunk(
        context->service,
        payload,
        payload_length);

    coap_free_header(&response);

    if (service_status != FIRMWARE_UPDATE_SERVICE_STATUS_OK)
    {
        close_request(context);
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
    }

    {
        uint8_t progress = (uint8_t)(
            (context->service->download_offset * 100U) /
            context->total_size);

        if (progress == 100U ||
            progress >= (uint8_t)(context->last_progress_percent + 25U))
        {
            context->last_progress_percent = progress;
            printf("[OTA] download=%u%%\r\n", (unsigned)progress);
        }
    }

    if (!more)
    {
        if (context->service->download_offset != context->total_size)
        {
            return fail_request(
                context,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
        }

        if (firmware_update_service_finish_download(context->service) !=
            FIRMWARE_UPDATE_SERVICE_STATUS_OK)
        {
            close_request(context);
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
        }

        close_request(context);
        printf("[OTA] download complete\r\n");
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
    }

    if (context->service->download_offset % block_size != 0U)
    {
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
    }

    context->block_size = block_size;
    context->block_number =
        (uint32_t)(context->service->download_offset / block_size);
    context->retry_count = 0U;
    context->retry_timeout_ms = COAP_INITIAL_RETRY_TIMEOUT_MS;
    context->state = ESP01_COAP_DOWNLOAD_SENDING;
    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

bool esp01_coap_download_transport_init(
    esp01_coap_download_transport_t *context,
    uint8_t link_id,
    uint16_t local_port,
    firmware_download_transport_t *transport)
{
    if (context == NULL ||
        transport == NULL ||
        link_id >= ESP01_MODEM_MAX_LINKS ||
        local_port == 0U)
    {
        return false;
    }

    memset(context, 0, sizeof(*context));
    memset(transport, 0, sizeof(*transport));

    context->public_transport = transport;
    context->link_id = link_id;
    context->local_port = local_port;
    context->block_size = COAP_REQUESTED_BLOCK_SIZE;
    context->retry_timeout_ms = COAP_INITIAL_RETRY_TIMEOUT_MS;

    transport->context = context;
    transport->start = start_download;
    transport->cancel = cancel_download;
    return true;
}

firmware_download_transport_status_t
esp01_coap_download_transport_process(
    esp01_coap_download_transport_t *context)
{
    esp01_modem_status_t modem_status;
    size_t response_length = 0U;

    if (context == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    switch (context->state)
    {
    case ESP01_COAP_DOWNLOAD_IDLE:
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;

    case ESP01_COAP_DOWNLOAD_OPENING:
        modem_status = esp01_modem_open_udp(
            context->link_id,
            context->host,
            context->remote_port,
            context->local_port);

        if (modem_status != ESP01_MODEM_STATUS_OK)
        {
            return fail_request(
                context,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE);
        }

        context->state = ESP01_COAP_DOWNLOAD_SENDING;
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;

    case ESP01_COAP_DOWNLOAD_SENDING:
        return send_request(context, false);

    case ESP01_COAP_DOWNLOAD_WAITING:
        esp01_modem_poll();
        modem_status = esp01_modem_receive_udp(
            context->link_id,
            context->response_buffer,
            sizeof(context->response_buffer),
            &response_length);

        if (modem_status == ESP01_MODEM_STATUS_OK)
            return process_response(context, response_length);

        if (modem_status != ESP01_MODEM_STATUS_NO_PACKET)
        {
            return fail_request(
                context,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE);
        }

        if ((int32_t)(HAL_GetTick() -
                      context->response_deadline_ms) < 0)
        {
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
        }

        if (context->retry_count >= COAP_MAX_RETRY_COUNT)
        {
            return fail_request(
                context,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE);
        }

        context->retry_count++;

        if (context->retry_timeout_ms < COAP_MAX_RETRY_TIMEOUT_MS)
        {
            context->retry_timeout_ms *= 2U;

            if (context->retry_timeout_ms > COAP_MAX_RETRY_TIMEOUT_MS)
                context->retry_timeout_ms = COAP_MAX_RETRY_TIMEOUT_MS;
        }

        return send_request(context, true);

    default:
        return fail_request(
            context,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
            FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE);
    }
}
