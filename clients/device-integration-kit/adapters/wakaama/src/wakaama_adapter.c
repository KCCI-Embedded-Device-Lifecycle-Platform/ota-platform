#include "wakaama_adapter.h"

#include <stddef.h>

bool wakaama_adapter_init(
    wakaama_adapter_t *adapter,
    const lwm2m_transport_t *transport)
{
    if (adapter == NULL ||
        transport == NULL ||
        transport->connect == NULL ||
        transport->close == NULL ||
        transport->send == NULL ||
        transport->receive == NULL ||
        transport->session_is_equal == NULL)
    {
        return false;
    }

    adapter->lwm2m_context = NULL;
    adapter->transport = *transport;

    return true;
}

void wakaama_adapter_set_context(
    wakaama_adapter_t *adapter,
    lwm2m_context_t *lwm2m_context)
{
    if (adapter != NULL)
        adapter->lwm2m_context = lwm2m_context;
}

void wakaama_adapter_poll(
    wakaama_adapter_t *adapter)
{
    datagram_status_t status;
    void *session;
    size_t length;

    if (adapter == NULL ||
        adapter->lwm2m_context == NULL)
    {
        return;
    }

    while (true)
    {
        session = NULL;
        length = 0U;

        status = adapter->transport.receive(
            adapter->transport.context,
            &session,
            adapter->receive_buffer,
            sizeof(adapter->receive_buffer),
            &length);

        if (status ==
            DATAGRAM_STATUS_NO_PACKET)
        {
            break;
        }

        if (status != DATAGRAM_STATUS_OK ||
            session == NULL ||
            length == 0U)
        {
            break;
        }

        lwm2m_handle_packet(
            adapter->lwm2m_context,
            adapter->receive_buffer,
            length,
            session);
    }
}

void *lwm2m_connect_server(
    uint16_t security_instance_id,
    void *user_data)
{
    wakaama_adapter_t *adapter =
        user_data;

    if (adapter == NULL)
        return NULL;

    return adapter->transport.connect(
        adapter->transport.context,
        security_instance_id);
}

void lwm2m_close_connection(
    void *session,
    void *user_data)
{
    wakaama_adapter_t *adapter =
        user_data;

    if (adapter == NULL || session == NULL)
        return;

    adapter->transport.close(
        adapter->transport.context,
        session);
}

uint8_t lwm2m_buffer_send(
    void *session,
    uint8_t *buffer,
    size_t length,
    void *user_data)
{
    wakaama_adapter_t *adapter =
        user_data;

    if (adapter == NULL ||
        session == NULL ||
        buffer == NULL ||
        length == 0U)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    if (adapter->transport.send(
            adapter->transport.context,
            session,
            buffer,
            length) !=
        DATAGRAM_STATUS_OK)
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    return COAP_NO_ERROR;
}

bool lwm2m_session_is_equal(
    void *session_1,
    void *session_2,
    void *user_data)
{
    wakaama_adapter_t *adapter =
        user_data;

    if (adapter == NULL)
        return false;

    return adapter->transport.session_is_equal(
        adapter->transport.context,
        session_1,
        session_2);
}