#ifndef ESP01_COAP_DOWNLOAD_TRANSPORT_H
#define ESP01_COAP_DOWNLOAD_TRANSPORT_H

#include "firmware_download_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ESP01_COAP_DOWNLOAD_URI_CAPACITY      256U
#define ESP01_COAP_DOWNLOAD_REQUEST_CAPACITY  384U
#define ESP01_COAP_DOWNLOAD_RESPONSE_CAPACITY 2048U

typedef enum
{
    ESP01_COAP_DOWNLOAD_IDLE = 0,
    ESP01_COAP_DOWNLOAD_OPENING,
    ESP01_COAP_DOWNLOAD_SENDING,
    ESP01_COAP_DOWNLOAD_WAITING
} esp01_coap_download_state_t;

typedef struct
{
    firmware_download_transport_t *public_transport;
    firmware_update_service_t *service;

    uint8_t link_id;
    uint16_t local_port;
    uint16_t remote_port;

    char uri[ESP01_COAP_DOWNLOAD_URI_CAPACITY];
    char *host;
    char *path;
    char *query;

    esp01_coap_download_state_t state;
    uint32_t block_number;
    uint16_t block_size;
    uint32_t total_size;
    uint16_t message_id;
    uint8_t token[2];

    uint8_t retry_count;
    uint32_t retry_timeout_ms;
    uint32_t response_deadline_ms;
    uint8_t last_progress_percent;

    uint8_t request_buffer[ESP01_COAP_DOWNLOAD_REQUEST_CAPACITY];
    size_t request_length;
    uint8_t response_buffer[ESP01_COAP_DOWNLOAD_RESPONSE_CAPACITY];
} esp01_coap_download_transport_t;

bool esp01_coap_download_transport_init(
    esp01_coap_download_transport_t *context,
    uint8_t link_id,
    uint16_t local_port,
    firmware_download_transport_t *transport);

firmware_download_transport_status_t
esp01_coap_download_transport_process(
    esp01_coap_download_transport_t *context);

#endif
