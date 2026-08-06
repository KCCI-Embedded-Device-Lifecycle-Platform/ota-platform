#include "linux_coap_uri.h"

#include <stdbool.h>
#include <string.h>

static bool prv_find_scheme_length(
    const char *uri,
    size_t uri_length,
    size_t *scheme_length)
{
    size_t index;

    for (index = 0; index + 2 < uri_length; ++index)
    {
        if (uri[index] == ':' &&
            uri[index + 1] == '/' &&
            uri[index + 2] == '/')
        {
            *scheme_length = index;
            return true;
        }
    }

    return false;
}

static bool prv_parse_port(
    const char *text,
    size_t text_length,
    uint16_t *port)
{
    uint32_t value = 0;
    size_t index;

    if (text_length == 0)
        return false;

    for (index = 0; index < text_length; ++index)
    {
        if (text[index] < '0' || text[index] > '9')
            return false;

        value = value * 10u + (uint32_t)(text[index] - '0');

        if (value > 65535u)
            return false;
    }

    if (value == 0)
        return false;

    *port = (uint16_t)value;
    return true;
}

linux_coap_uri_status_t linux_coap_uri_parse(
    const char *uri,
    size_t uri_length,
    linux_coap_uri_t *parsed_uri)
{
    size_t scheme_length;
    size_t authority_start;
    size_t authority_end;
    size_t colon_index;
    size_t cursor;
    size_t index;

    if (uri == NULL || uri_length == 0 || parsed_uri == NULL)
        return LINUX_COAP_URI_STATUS_INVALID;

    memset(parsed_uri, 0, sizeof(*parsed_uri));

    if (!prv_find_scheme_length(uri, uri_length, &scheme_length) || scheme_length == 0)
    {
        return LINUX_COAP_URI_STATUS_INVALID;
    }

    if (scheme_length != sizeof("coap") - 1 ||
        memcmp(uri, "coap", sizeof("coap") - 1) != 0)
    {
        return LINUX_COAP_URI_STATUS_UNSUPPORTED_SCHEME;
    }

    authority_start = scheme_length + 3;
    authority_end = authority_start;

    while (authority_end < uri_length &&
           uri[authority_end] != '/' &&
           uri[authority_end] != '?' &&
           uri[authority_end] != '#')
    {
        ++authority_end;
    }

    if (authority_end == authority_start)
        return LINUX_COAP_URI_STATUS_INVALID;

    colon_index = authority_end;

    for (index = authority_start; index < authority_end; ++index)
    {
        if (uri[index] == ':')
        {
            if (colon_index != authority_end)
                return LINUX_COAP_URI_STATUS_INVALID;

            colon_index = index;
        }
    }

    if (colon_index == authority_start)
        return LINUX_COAP_URI_STATUS_INVALID;

    parsed_uri->host = uri + authority_start;
    parsed_uri->host_length = colon_index - authority_start;
    parsed_uri->port = 5683;

    if (colon_index < authority_end &&
        !prv_parse_port(
            uri + colon_index + 1,
            authority_end - colon_index - 1,
            &parsed_uri->port))
    {
        return LINUX_COAP_URI_STATUS_INVALID;
    }

    cursor = authority_end;

    if (cursor < uri_length && uri[cursor] == '/')
    {
        size_t path_start = ++cursor;

        while (cursor < uri_length &&
               uri[cursor] != '?' &&
               uri[cursor] != '#')
        {
            ++cursor;
        }

        parsed_uri->path = uri + path_start;
        parsed_uri->path_length = cursor - path_start;
    }

    if (cursor < uri_length && uri[cursor] == '?')
    {
        size_t query_start = ++cursor;

        while (cursor < uri_length && uri[cursor] != '#')
            ++cursor;

        parsed_uri->query = uri + query_start;
        parsed_uri->query_length = cursor - query_start;
    }

    if (cursor != uri_length)
        return LINUX_COAP_URI_STATUS_INVALID;

    return LINUX_COAP_URI_STATUS_OK;
}