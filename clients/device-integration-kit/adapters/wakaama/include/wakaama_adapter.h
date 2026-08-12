#ifndef WAKAAMA_ADAPTER_H
#define WAKAAMA_ADAPTER_H

#include "liblwm2m.h"
#include "lwm2m_transport.h"

#include <stdbool.h>
#include <stdint.h>

#define  UDP_MAX_PACKET_SIZE 2048U

typedef struct
{
    lwm2m_context_t *lwm2m_context;
    lwm2m_transport_t transport;

    uint8_t receive_buffer[
        UDP_MAX_PACKET_SIZE];
} wakaama_adapter_t;

bool wakaama_adapter_init(
    wakaama_adapter_t *adapter,
    const lwm2m_transport_t *transport);

void wakaama_adapter_set_context(
    wakaama_adapter_t *adapter,
    lwm2m_context_t *lwm2m_context);

void wakaama_adapter_poll(
    wakaama_adapter_t *adapter);

#endif