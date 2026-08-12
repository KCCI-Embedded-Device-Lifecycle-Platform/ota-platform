#ifndef LWM2M_TRANSPORT_H
#define LWM2M_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    DATAGRAM_STATUS_OK = 0,
    DATAGRAM_STATUS_INVALID_ARGUMENT,
    DATAGRAM_STATUS_CONNECTION_FAILURE,
    DATAGRAM_STATUS_SEND_FAILURE,
    DATAGRAM_STATUS_NO_PACKET,
    DATAGRAM_STATUS_PACKET_TOO_LARGE
} datagram_status_t;

typedef struct
{
    void *context;

    void *(*connect)(
        void *context,
        uint16_t security_instance_id);

    void (*close)(
        void *context,
        void *session);

    datagram_status_t (*send)(
        void *context,
        void *session,
        const uint8_t *data,
        size_t length);

    datagram_status_t (*receive)(
        void *context,
        void **session,
        uint8_t *buffer,
        size_t capacity,
        size_t *length);

    bool (*session_is_equal)(
        void *context,
        void *session_1,
        void *session_2);
} lwm2m_transport_t;

#endif