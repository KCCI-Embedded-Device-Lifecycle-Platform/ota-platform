#ifndef OTA_LINUX_REFERENCE_COAP_DOWNLOAD_TRANSPORT_H
#define OTA_LINUX_REFERENCE_COAP_DOWNLOAD_TRANSPORT_H

#include "firmware_download_transport.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct linux_coap_download_transport
    linux_coap_download_transport_t;

linux_coap_download_transport_t * linux_coap_download_transport_create(
    firmware_download_transport_t *transport);

void linux_coap_download_transport_destroy(
    linux_coap_download_transport_t *context);

int linux_coap_download_transport_get_socket_fd(
    const linux_coap_download_transport_t *context);

int linux_coap_download_transport_get_timeout_ms(
    const linux_coap_download_transport_t *context);

firmware_download_transport_status_t linux_coap_download_transport_process(
    linux_coap_download_transport_t *context,
    bool socket_readable);

#ifdef __cplusplus
}
#endif

#endif