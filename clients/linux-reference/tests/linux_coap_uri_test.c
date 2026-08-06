#include "linux_coap_uri.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void assert_slice_equals(
    const char *actual,
    size_t actual_length,
    const char *expected)
{
    size_t expected_length = strlen(expected);

    assert(actual != NULL);
    assert(actual_length == expected_length);
    assert(memcmp(actual, expected, expected_length) == 0);
}

int main(void)
{
    const char uri[] =
        "coap://127.0.0.1:5684/releases/device.bin?token=abc";
    linux_coap_uri_t parsed_uri = {0};

    assert(
        linux_coap_uri_parse(
            uri,
            sizeof(uri) - 1,
            &parsed_uri
        ) == LINUX_COAP_URI_STATUS_OK
    );

    assert_slice_equals(
        parsed_uri.host,
        parsed_uri.host_length,
        "127.0.0.1"
    );
    assert(parsed_uri.port == 5684);

    /* Separators '/' and '?' are not part of the parsed values. */
    assert_slice_equals(
        parsed_uri.path,
        parsed_uri.path_length,
        "releases/device.bin"
    );
    assert_slice_equals(
        parsed_uri.query,
        parsed_uri.query_length,
        "token=abc"
    );
{
    const char default_port_uri[] =
        "coap://firmware.local/image.bin";
    linux_coap_uri_t default_port = {0};

    assert(
        linux_coap_uri_parse(
            default_port_uri,
            sizeof(default_port_uri) - 1,
            &default_port
        ) == LINUX_COAP_URI_STATUS_OK
    );

    assert_slice_equals(
        default_port.host,
        default_port.host_length,
        "firmware.local"
    );
    assert(default_port.port == 5683);
    assert_slice_equals(
        default_port.path,
        default_port.path_length,
        "image.bin"
    );
    assert(default_port.query == NULL);
    assert(default_port.query_length == 0);
}

{
    const char malformed_uri[] = "firmware.bin";
    const char unsupported_uri[] =
        "https://firmware.local/image.bin";
    const char empty_host_uri[] =
        "coap:///image.bin";
    linux_coap_uri_t parsed = {0};

    assert(
        linux_coap_uri_parse(
            malformed_uri,
            sizeof(malformed_uri) - 1,
            &parsed
        ) == LINUX_COAP_URI_STATUS_INVALID
    );

    assert(
        linux_coap_uri_parse(
            unsupported_uri,
            sizeof(unsupported_uri) - 1,
            &parsed
        ) == LINUX_COAP_URI_STATUS_UNSUPPORTED_SCHEME
    );

    assert(
        linux_coap_uri_parse(
            empty_host_uri,
            sizeof(empty_host_uri) - 1,
            &parsed
        ) == LINUX_COAP_URI_STATUS_INVALID
    );
}

    puts("linux CoAP URI test passed");
    return 0;
}