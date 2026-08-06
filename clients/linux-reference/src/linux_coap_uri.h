#ifndef LINUX_COAP_URI_H
#define LINUX_COAP_URI_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    LINUX_COAP_URI_STATUS_OK = 0,
    LINUX_COAP_URI_STATUS_INVALID,
    LINUX_COAP_URI_STATUS_UNSUPPORTED_SCHEME
} linux_coap_uri_status_t;

typedef struct
{
    const char *host;
    size_t host_length;
    uint16_t port;

    const char *path;
    size_t path_length;

    const char *query;
    size_t query_length;
} linux_coap_uri_t;

/*
 * Parses a bounded URI without modifying or copying it.
 *
 * host, path, and query point into the caller-owned URI buffer.
 * They remain valid only while that buffer remains valid.
 */
linux_coap_uri_status_t linux_coap_uri_parse(
    const char *uri,
    size_t uri_length,
    linux_coap_uri_t *parsed_uri);

#endif