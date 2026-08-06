#include "firmware_update_service.h"
#include "linux_coap_download_transport.h"
#include "linux_firmware_update_backend.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <stdio.h>

typedef struct
{
    uint8_t bytes[1152];
    size_t length;
    struct sockaddr_storage client_address;
    socklen_t client_address_length;
} loopback_request_t;

static int create_loopback_server(uint16_t *server_port)
{
    struct sockaddr_in address = {0};
    socklen_t address_length = sizeof(address);
    int socket_fd;

    if (server_port == NULL)
        return -1;

    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_fd < 0)
        return -1;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);

    if (bind(
            socket_fd,
            (const struct sockaddr *)&address,
            sizeof(address)) != 0)
    {
        close(socket_fd);
        return -1;
    }

    if (getsockname(
            socket_fd,
            (struct sockaddr *)&address,
            &address_length) != 0)
    {
        close(socket_fd);
        return -1;
    }

    *server_port = ntohs(address.sin_port);
    return socket_fd;
}

static bool receive_loopback_request(int socket_fd, loopback_request_t *request)
{
    struct timeval timeout = {
        .tv_sec = 1,
        .tv_usec = 0
    };
    ssize_t received_length;

    if (request == NULL)
        return false;

    if (setsockopt(
            socket_fd,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)) != 0)
    {
        return false;
    }

    memset(request, 0, sizeof(*request));
    request->client_address_length =
        sizeof(request->client_address);

    received_length = recvfrom(
        socket_fd,
        request->bytes,
        sizeof(request->bytes),
        0,
        (struct sockaddr *)&request->client_address,
        &request->client_address_length
    );

    if (received_length < 0)
        return false;

    request->length = (size_t)received_length;
    return true;
}

static bool send_loopback_content_response(
    int socket_fd,
    const loopback_request_t *request,
    const uint8_t *payload,
    size_t payload_length)
{
    uint8_t response[1152];
    size_t token_length;
    size_t response_length;
    ssize_t sent_length;

    if (request == NULL ||
        payload == NULL ||
        payload_length == 0 ||
        request->length < 4)
    {
        return false;
    }

    token_length = request->bytes[0] & 0x0f;

    if (token_length > 8 ||
        request->length < 4 + token_length ||
        payload_length >
            sizeof(response) - 5 - token_length)
    {
        return false;
    }

    /*
     * Version 1, ACK, same token length.
     * Response code 2.05 Content.
     */
    response[0] = (uint8_t)(0x60 | token_length);
    response[1] = 0x45;

    /* Echo Message ID and Token from the request. */
    response[2] = request->bytes[2];
    response[3] = request->bytes[3];

    memcpy(
        response + 4,
        request->bytes + 4,
        token_length
    );

    response[4 + token_length] = 0xff;

    memcpy(
        response + 5 + token_length,
        payload,
        payload_length
    );

    response_length =
        5 + token_length + payload_length;

    sent_length = sendto(
        socket_fd,
        response,
        response_length,
        0,
        (const struct sockaddr *)&request->client_address,
        request->client_address_length
    );

    return sent_length == (ssize_t)response_length;
}

static bool send_loopback_block2_response(
    int socket_fd,
    const loopback_request_t *request,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t block_number,
    bool more_blocks,
    size_t total_length)
{
    uint8_t response[1152];
    size_t token_length;
    size_t cursor;
    ssize_t sent_length;
    uint8_t block_value;

    /*
     * This test helper uses:
     * - Block size 16 bytes (SZX = 0)
     * - one-byte Block2 number
     * - one-byte Size2 value
     */
    if (request == NULL ||
        payload == NULL ||
        payload_length == 0 ||
        payload_length > 16 ||
        block_number > 15 ||
        total_length == 0 ||
        total_length > 255 ||
        request->length < 4)
    {
        return false;
    }

    token_length = request->bytes[0] & 0x0f;

    if (token_length > 8 ||
        request->length < 4 + token_length)
    {
        return false;
    }

    response[0] = (uint8_t)(0x60 | token_length);
    response[1] = 0x45;
    response[2] = request->bytes[2];
    response[3] = request->bytes[3];

    memcpy(
        response + 4,
        request->bytes + 4,
        token_length
    );

    cursor = 4 + token_length;

    /*
     * Block2 is option 23:
     * extended delta 13 + 10, one-byte value.
     */
    response[cursor++] = 0xd1;
    response[cursor++] = 10;

    block_value = (uint8_t)(block_number << 4);

    if (more_blocks)
        block_value |= 0x08;

    response[cursor++] = block_value;

    /*
     * Size2 is option 28:
     * delta 5 from Block2, one-byte value.
     */
    response[cursor++] = 0x51;
    response[cursor++] = (uint8_t)total_length;

    response[cursor++] = 0xff;

    memcpy(
        response + cursor,
        payload,
        payload_length
    );
    cursor += payload_length;

    sent_length = sendto(
        socket_fd,
        response,
        cursor,
        0,
        (const struct sockaddr *)&request->client_address,
        request->client_address_length
    );

    return sent_length == (ssize_t)cursor;
}

int main(void)
{
    const char *staging_path =
        "/tmp/ota-linux-coap-download-transport-e2e.bin";

    const uint8_t firmware[] = {
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13
    };
    uint8_t stored_firmware[sizeof(firmware)];
    FILE *staging_file;

    linux_firmware_update_backend_context_t backend_context;
    firmware_update_backend_t backend;
    firmware_update_service_t service;
    firmware_download_transport_t transport = {0};
    linux_coap_download_transport_t *transport_context;

    char uri[128];
    int uri_length;
    loopback_request_t request;
    loopback_request_t second_request;

    uint16_t server_port;
    int server_socket;

    remove(staging_path);

    server_socket = create_loopback_server(&server_port);

    assert(server_socket >= 0);
    assert(server_port != 0);

    assert(
        linux_firmware_update_backend_init(
            &backend_context,
            staging_path,
            &backend
        )
    );

    assert(
        firmware_update_service_init(
            &service,
            &backend
        )
    );

    transport_context = linux_coap_download_transport_create(&transport);

    assert(transport_context != NULL);

    uri_length = snprintf(
        uri,
        sizeof(uri),
        "coap://127.0.0.1:%u/firmware.bin",
        server_port
    );

    assert(uri_length > 0);
    assert((size_t)uri_length < sizeof(uri));

    assert(
        transport.start(
            transport.context,
            uri,
            (size_t)uri_length,
            &service
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    /* First process creates the session. */
    assert(
        linux_coap_download_transport_process(
            transport_context,
            false
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    /* Second process sends the GET request. */
    assert(
        linux_coap_download_transport_process(
            transport_context,
            false
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    assert(
        receive_loopback_request(
            server_socket,
            &request
        )
    );

    assert(request.length >= 4);

    /* CoAP version 1, Confirmable message, GET code. */
    assert((request.bytes[0] >> 6) == 1);
    assert(((request.bytes[0] >> 4) & 0x03) == 0);
    assert(request.bytes[1] == 1);
    assert((request.bytes[0] & 0x0f) <= 8);

    /* Send Block2 number 0: 16 bytes, with more blocks. */
    assert(
        send_loopback_block2_response(
            server_socket,
            &request,
            firmware,
            16,
            0,
            true,
            sizeof(firmware)
        )
    );

    assert(
        linux_coap_download_transport_process(
            transport_context,
            true
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    assert(
        service.state ==
        FIRMWARE_UPDATE_STATE_DOWNLOADING
    );
    assert(service.download_offset == 16);
    assert(!backend_context.package_ready);

    /* Allow libcoap to send its automatic request for Block2 number 1. */
    assert(
        linux_coap_download_transport_process(
            transport_context,
            false
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    assert(
        receive_loopback_request(
            server_socket,
            &second_request
        )
    );

    assert(second_request.length >= 4);
    assert(second_request.bytes[1] == 1);

    /* Send the final Block2: remaining 4 bytes. */
    assert(
        send_loopback_block2_response(
            server_socket,
            &second_request,
            firmware + 16,
            sizeof(firmware) - 16,
            1,
            false,
            sizeof(firmware)
        )
    );

    assert(
        linux_coap_download_transport_process(
            transport_context,
            true
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    assert(
        service.state ==
        FIRMWARE_UPDATE_STATE_DOWNLOADED
    );
    assert(backend_context.package_ready);

    staging_file = fopen(staging_path, "rb");
    assert(staging_file != NULL);

    assert(
        fread(
            stored_firmware,
            1,
            sizeof(stored_firmware),
            staging_file
        ) == sizeof(stored_firmware)
    );

    assert(fclose(staging_file) == 0);

    assert(
        memcmp(
            stored_firmware,
            firmware,
            sizeof(firmware)
        ) == 0
    );

    assert(
        transport.cancel(transport.context) ==
        FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    /*
     * A loopback CoAP responder and firmware verification
     * will be added in the next steps.
     */

    linux_coap_download_transport_destroy(
        transport_context
    );
    linux_firmware_update_backend_deinit(
        &backend_context
    );

    assert(close(server_socket) == 0);
    remove(staging_path);

    puts("linux CoAP download transport E2E fixture passed");
    return 0;
}