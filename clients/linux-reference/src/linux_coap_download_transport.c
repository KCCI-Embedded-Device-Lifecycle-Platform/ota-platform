#include "linux_coap_download_transport.h"
#include "linux_coap_uri.h"
#include <coap3/coap.h>

#include <stdlib.h>
#include <string.h>

struct linux_coap_download_transport
{
    firmware_download_transport_t *public_transport;
    coap_context_t *coap_context;
    int socket_fd;
    int timeout_ms;

    char *request_uri;
    size_t request_uri_length;
    bool request_sent;
    linux_coap_uri_t parsed_uri;
    firmware_update_service_t *service;

    coap_session_t *session;

    bool response_complete;
    bool response_failed;
    
};

static coap_response_t prv_response_handler(
    coap_session_t *session,
    const coap_pdu_t *sent,
    const coap_pdu_t *received,
    const coap_mid_t message_id)
{
    linux_coap_download_transport_t *context;
    firmware_update_service_status_t service_status;
    const uint8_t *data;
    size_t length;
    size_t offset;
    size_t total;

    (void)sent;
    (void)message_id;

    context = (linux_coap_download_transport_t *) coap_session_get_app_data(session);

    if (context == NULL || received == NULL)
        return COAP_RESPONSE_FAIL;

    if (COAP_RESPONSE_CLASS(coap_pdu_get_code(received)) != 2)
    {
        firmware_update_service_fail_download(
            context->service,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INVALID_URI
        );
        context->response_failed = true;
        return COAP_RESPONSE_OK;
    }

    if (!coap_get_data_large(
            received,
            &length,
            &data,
            &offset,
            &total) ||
        length == 0 ||
        total == 0 ||
        offset > total ||
        length > total - offset)
    {
        firmware_update_service_fail_download(
            context->service,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL
        );
        context->response_failed = true;
        return COAP_RESPONSE_OK;
    }

    if (context->service->state == FIRMWARE_UPDATE_STATE_IDLE)
    {
        if (offset != 0)
        {
            firmware_update_service_fail_download(
                context->service,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL
            );
            context->response_failed = true;
            return COAP_RESPONSE_OK;
        }

        service_status =
            firmware_update_service_begin_download(
                context->service,
                total
            );

        if (service_status !=
            FIRMWARE_UPDATE_SERVICE_STATUS_OK)
        {
            context->response_failed = true;
            return COAP_RESPONSE_OK;
        }
    }

    if (context->service->state !=
            FIRMWARE_UPDATE_STATE_DOWNLOADING ||
        offset != context->service->download_offset)
    {
        firmware_update_service_fail_download(
            context->service,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL
        );
        context->response_failed = true;
        return COAP_RESPONSE_OK;
    }

    service_status =
        firmware_update_service_write_chunk(
            context->service,
            data,
            length
        );

    if (service_status !=
        FIRMWARE_UPDATE_SERVICE_STATUS_OK)
    {
        context->response_failed = true;
        return COAP_RESPONSE_OK;
    }

    if (offset + length == total)
    {
        service_status =
            firmware_update_service_finish_download(
                context->service
            );

        if (service_status !=
            FIRMWARE_UPDATE_SERVICE_STATUS_OK)
        {
            context->response_failed = true;
            return COAP_RESPONSE_OK;
        }

        context->response_complete = true;
    }

    return COAP_RESPONSE_OK;
}

static void prv_clear_request(linux_coap_download_transport_t *context)
{
    if (context->session != NULL)
    {
        coap_session_release(context->session);
        context->session = NULL;
    }

    context->socket_fd = -1;

    free(context->request_uri);

    context->request_uri = NULL;
    context->request_uri_length = 0;
    context->request_sent = false;
    memset(&context->parsed_uri, 0, sizeof(context->parsed_uri));
    context->service = NULL;
    context->timeout_ms = -1;
    context->response_complete = false;
    context->response_failed = false;
}

static firmware_download_transport_status_t prv_start(
    void *raw_context,
    const char *uri,
    size_t uri_length,
    firmware_update_service_t *service)
{
    linux_coap_download_transport_t *context =
        (linux_coap_download_transport_t *)raw_context;
    linux_coap_uri_status_t uri_status;
    char *uri_copy;

    if (context == NULL || service == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    if (context->request_uri != NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    uri_status = linux_coap_uri_parse(
        uri,
        uri_length,
        &context->parsed_uri
    );

    switch (uri_status)
    {
    case LINUX_COAP_URI_STATUS_OK:
        break;

    case LINUX_COAP_URI_STATUS_INVALID:
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;

    case LINUX_COAP_URI_STATUS_UNSUPPORTED_SCHEME:
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_UNSUPPORTED_PROTOCOL;

    default:
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
    }

    uri_copy = (char *)malloc(uri_length + 1);

    if (uri_copy == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    memcpy(uri_copy, uri, uri_length);
    uri_copy[uri_length] = '\0';

    /*
     * Parse again so every slice points into the Transport-owned copy,
     * not into the caller-owned URI buffer.
     */
    if (linux_coap_uri_parse(
            uri_copy,
            uri_length,
            &context->parsed_uri) != LINUX_COAP_URI_STATUS_OK)
    {
        free(uri_copy);
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
    }

    context->request_uri = uri_copy;
    context->request_uri_length = uri_length;
    context->service = service;
    context->timeout_ms = 0;

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

static firmware_download_transport_status_t prv_send_get(
    linux_coap_download_transport_t *context)
{
    coap_uri_t uri = {0};
    coap_optlist_t *options = NULL;
    coap_pdu_t *pdu = NULL;
    uint8_t *option_buffer = NULL;
    uint8_t token[COAP_TOKEN_DEFAULT_MAX];
    size_t token_length = 0;
    coap_mid_t message_id;

    uri.scheme = COAP_URI_SCHEME_COAP;
    uri.host.s =
        (const uint8_t *)context->parsed_uri.host;
    uri.host.length =
        context->parsed_uri.host_length;
    uri.port = context->parsed_uri.port;
    uri.path.s =
        (const uint8_t *)context->parsed_uri.path;
    uri.path.length =
        context->parsed_uri.path_length;
    uri.query.s =
        (const uint8_t *)context->parsed_uri.query;
    uri.query.length =
        context->parsed_uri.query_length;

    option_buffer = (uint8_t *)malloc(
        context->request_uri_length + 1
    );

    if (option_buffer == NULL)
        goto failure;

    /*
     * Port, Path, and Query options are created here.
     * Uri-Host is added directly to the PDU below.
     */
    if (coap_uri_into_options(
            &uri,
            NULL,
            &options,
            1,
            option_buffer,
            context->request_uri_length + 1) < 0)
    {
        goto failure;
    }

    pdu = coap_new_pdu(
        COAP_MESSAGE_CON,
        COAP_REQUEST_GET,
        context->session
    );

    if (pdu == NULL)
        goto failure;

    coap_session_new_token(
        context->session,
        &token_length,
        token
    );

    if (!coap_add_token(pdu, token_length, token))
        goto failure;

    if (!coap_add_option(
            pdu,
            COAP_OPTION_URI_HOST,
            uri.host.length,
            uri.host.s))
    {
        goto failure;
    }

    if (options != NULL &&
        !coap_add_optlist_pdu(pdu, &options))
    {
        goto failure;
    }

    coap_delete_optlist(options);
    options = NULL;

    free(option_buffer);
    option_buffer = NULL;

    /*
     * coap_send() takes ownership of pdu, including failure cases.
     */
    message_id = coap_send(context->session, pdu);
    pdu = NULL;

    if (message_id == COAP_INVALID_MID)
        goto failure;

    context->request_sent = true;
    context->timeout_ms = 100;

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;

failure:
    if (pdu != NULL)
        coap_delete_pdu(pdu);

    coap_delete_optlist(options);
    free(option_buffer);

    firmware_update_service_fail_download(
        context->service,
        FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL
    );
    prv_clear_request(context);

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
}

static firmware_download_transport_status_t prv_cancel(
    void *raw_context)
{
    linux_coap_download_transport_t *context =
        (linux_coap_download_transport_t *)raw_context;

    if (context == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
    
    prv_clear_request(context);

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

linux_coap_download_transport_t * linux_coap_download_transport_create(
    firmware_download_transport_t *transport)
{
    linux_coap_download_transport_t *context;

    if (transport == NULL)
        return NULL;

    coap_startup();

    context = (linux_coap_download_transport_t *)calloc(1, sizeof(*context));

    if (context == NULL)
    {
        coap_cleanup();
        return NULL;
    }

    context->coap_context = coap_new_context(NULL);

    if (context->coap_context == NULL)
    {
        free(context);
        coap_cleanup();
        return NULL;
    }

    coap_context_set_block_mode(context->coap_context, COAP_BLOCK_USE_LIBCOAP);

    coap_register_response_handler(context->coap_context, prv_response_handler);

    context->public_transport = transport;
    context->socket_fd = -1;
    context->timeout_ms = -1;

    transport->context = context;
    transport->start = prv_start;
    transport->cancel = prv_cancel;

    return context;
}

void linux_coap_download_transport_destroy(
    linux_coap_download_transport_t *context)
{
    if (context == NULL)
        return;

    if (context->public_transport != NULL &&
        context->public_transport->context == context)
    {
        context->public_transport->context = NULL;
        context->public_transport->start = NULL;
        context->public_transport->cancel = NULL;
    }

    prv_clear_request(context);
    coap_free_context(context->coap_context);
    free(context);
    coap_cleanup();
}

int linux_coap_download_transport_get_socket_fd(
    const linux_coap_download_transport_t *context)
{
    return context == NULL ? -1 : context->socket_fd;
}

int linux_coap_download_transport_get_timeout_ms(
    const linux_coap_download_transport_t *context)
{
    return context == NULL ? -1 : context->timeout_ms;
}

firmware_download_transport_status_t linux_coap_download_transport_process(
    linux_coap_download_transport_t *context,
    bool socket_readable)
{
    coap_str_const_t host;
    coap_addr_info_t *address_info;
    coap_address_t destination;
    coap_proto_t protocol;

    (void)socket_readable;

    if (context == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    if (context->request_uri == NULL)
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;

    if (context->session != NULL)
    {
        if (!context->request_sent)
            return prv_send_get(context);

        if (coap_io_process(
                context->coap_context,
                COAP_IO_NO_WAIT) < 0)
        {
            firmware_update_service_fail_download(
                context->service,
                FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST
            );
            prv_clear_request(context);
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE;
        }

        if (context->response_failed)
        {
            prv_clear_request(context);
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
        }

        if (context->response_complete)
        {
            /*
            * Session cleanup is done after coap_io_process() returns,
            * never from inside the response callback.
            */
            prv_clear_request(context);
            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
        }

        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
    }

    host.s = (const uint8_t *)context->parsed_uri.host;
    host.length = context->parsed_uri.host_length;

    address_info = coap_resolve_address_info(
        &host,
        context->parsed_uri.port,
        context->parsed_uri.port,
        context->parsed_uri.port,
        context->parsed_uri.port,
        0,
        COAP_URI_SCHEME_COAP_BIT,
        COAP_RESOLVE_TYPE_REMOTE
    );

    if (address_info == NULL)
    {
        firmware_update_service_fail_download(
            context->service,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST
        );
        prv_clear_request(context);
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE;
    }

    protocol = address_info->proto;
    memcpy(
        &destination,
        &address_info->addr,
        sizeof(destination)
    );
    coap_free_address_info(address_info);

    context->session = coap_new_client_session(
        context->coap_context,
        NULL,
        &destination,
        protocol
    );

    if (context->session == NULL)
    {
        firmware_update_service_fail_download(
            context->service,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST
        );
        prv_clear_request(context);
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_CONNECTION_FAILURE;
    }

    coap_session_set_app_data(
        context->session,
        context
    );

    context->socket_fd = coap_context_get_coap_fd(context->coap_context);

    if (context->socket_fd < 0)
    {
        firmware_update_service_fail_download(
            context->service,
            FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL
        );
        prv_clear_request(context);
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
    }

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}