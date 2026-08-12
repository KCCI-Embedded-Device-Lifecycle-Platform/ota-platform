#include "esp01_transport.h"

#include "esp01_modem.h"

#include <stddef.h>
#include <string.h>

static void *transport_connect(
    void *context,
    uint16_t security_instance_id)
{
    esp01_transport_t *transport = context;

    if (transport == NULL || security_instance_id != 0U)
        return NULL;

    if (transport->session.open)
        return &transport->session;

    if (esp01_modem_open_udp(
            transport->session.link_id,
            transport->remote_host,
            transport->remote_port,
            transport->local_port) !=
        ESP01_MODEM_STATUS_OK)
    {
        return NULL;
    }

    transport->session.open = true;
    return &transport->session;
}

static void transport_close(
    void *context,
    void *session)
{
    esp01_transport_t *transport = context;

    if (transport == NULL ||
        session != &transport->session ||
        !transport->session.open)
    {
        return;
    }

    (void)esp01_modem_close(transport->session.link_id);
    transport->session.open = false;
}

static datagram_status_t transport_send(
    void *context,
    void *session,
    const uint8_t *data,
    size_t length)
{
    esp01_transport_t *transport = context;
    esp01_modem_status_t status;

    if (transport == NULL ||
        session != &transport->session ||
        !transport->session.open ||
        data == NULL ||
        length == 0U)
    {
        return DATAGRAM_STATUS_INVALID_ARGUMENT;
    }

    status = esp01_modem_send_udp(
        transport->session.link_id,
        data,
        length);

    switch (status)
    {
    case ESP01_MODEM_STATUS_OK:
        return DATAGRAM_STATUS_OK;

    case ESP01_MODEM_STATUS_INVALID_ARGUMENT:
        return DATAGRAM_STATUS_INVALID_ARGUMENT;

    case ESP01_MODEM_STATUS_PACKET_TOO_LARGE:
        return DATAGRAM_STATUS_PACKET_TOO_LARGE;

    default:
        return DATAGRAM_STATUS_SEND_FAILURE;
    }
}

static datagram_status_t transport_receive(
    void *context,
    void **session,
    uint8_t *buffer,
    size_t capacity,
    size_t *length)
{
    esp01_transport_t *transport = context;
    esp01_modem_status_t status;

    if (transport == NULL ||
        session == NULL ||
        buffer == NULL ||
        length == NULL)
    {
        return DATAGRAM_STATUS_INVALID_ARGUMENT;
    }

    *session = NULL;
    *length = 0U;

    if (!transport->session.open)
        return DATAGRAM_STATUS_CONNECTION_FAILURE;

    esp01_modem_poll();

    status = esp01_modem_receive_udp(
        transport->session.link_id,
        buffer,
        capacity,
        length);

    switch (status)
    {
    case ESP01_MODEM_STATUS_OK:
        *session = &transport->session;
        return DATAGRAM_STATUS_OK;

    case ESP01_MODEM_STATUS_NO_PACKET:
        return DATAGRAM_STATUS_NO_PACKET;

    case ESP01_MODEM_STATUS_PACKET_TOO_LARGE:
        return DATAGRAM_STATUS_PACKET_TOO_LARGE;

    case ESP01_MODEM_STATUS_INVALID_ARGUMENT:
        return DATAGRAM_STATUS_INVALID_ARGUMENT;

    default:
        return DATAGRAM_STATUS_CONNECTION_FAILURE;
    }
}

static bool transport_session_is_equal(
    void *context,
    void *session_1,
    void *session_2)
{
    esp01_transport_t *transport = context;

    if (transport == NULL)
        return false;

    return session_1 == &transport->session &&
           session_2 == &transport->session;
}

bool esp01_transport_init(
    esp01_transport_t *transport,
    const char *remote_host,
    uint16_t remote_port,
    uint16_t local_port,
    uint8_t link_id,
    lwm2m_transport_t *lwm2m_transport)
{
    if (transport == NULL ||
        remote_host == NULL ||
        remote_host[0] == '\0' ||
        remote_port == 0U ||
        local_port == 0U ||
        link_id >= ESP01_MODEM_MAX_LINKS ||
        lwm2m_transport == NULL)
    {
        return false;
    }

    memset(transport, 0, sizeof(*transport));
    memset(lwm2m_transport, 0, sizeof(*lwm2m_transport));

    transport->remote_host = remote_host;
    transport->remote_port = remote_port;
    transport->local_port = local_port;
    transport->session.link_id = link_id;

    lwm2m_transport->context = transport;
    lwm2m_transport->connect = transport_connect;
    lwm2m_transport->close = transport_close;
    lwm2m_transport->send = transport_send;
    lwm2m_transport->receive = transport_receive;
    lwm2m_transport->session_is_equal = transport_session_is_equal;

    return true;
}
