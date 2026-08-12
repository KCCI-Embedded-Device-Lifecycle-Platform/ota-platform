#ifndef ESP01_TRANSPORT_H
#define ESP01_TRANSPORT_H

#include "lwm2m_transport.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t link_id;
    bool open;
} esp01_transport_session_t;

typedef struct
{
    const char *remote_host;
    uint16_t remote_port;
    uint16_t local_port;

    esp01_transport_session_t session;
} esp01_transport_t;

bool esp01_transport_init(
    esp01_transport_t *transport,
    const char *remote_host,
    uint16_t remote_port,
    uint16_t local_port,
    uint8_t link_id,
    lwm2m_transport_t *lwm2m_transport);

#endif