#include "linux_coap_download_transport.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    const char malformed_uri[] = "firmware.bin";
    const char unsupported_uri[] = "https://127.0.0.1/firmware.bin";
    const char uri[] = "coap://127.0.0.1:5684/firmware.bin";
    firmware_download_transport_t transport = {0};
    firmware_update_service_t service = {0};
    linux_coap_download_transport_t *context;

    /* A public Transport function table is required. */
    assert(linux_coap_download_transport_create(NULL) == NULL);

    context = linux_coap_download_transport_create(&transport);
    assert(context != NULL);

    /* create() connects the generic interface to the Linux Context. */
    assert(transport.context == context);
    assert(transport.start != NULL);
    assert(transport.cancel != NULL);

    /* No network request exists immediately after creation. */
    assert(
        linux_coap_download_transport_get_socket_fd(context) == -1
    );
    assert(
        linux_coap_download_transport_get_timeout_ms(context) == -1
    );
    assert(
        linux_coap_download_transport_process(context, false) ==
        FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    /* Cancel is safe even when no download is active. */
    assert(
        transport.cancel(transport.context) ==
        FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    /* A URI without a scheme is malformed. */
    assert(
        transport.start(
            transport.context,
            malformed_uri,
            sizeof(malformed_uri) - 1,
            &service
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI
    );

    /* This transport supports CoAP only. */
    assert(
        transport.start(
            transport.context,
            unsupported_uri,
            sizeof(unsupported_uri) - 1,
            &service
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_UNSUPPORTED_PROTOCOL
    );

    /* start() accepts the request and schedules asynchronous work. */
    assert(
        transport.start(
            transport.context,
            uri,
            sizeof(uri) - 1,
            &service
        ) == FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    assert(
        linux_coap_download_transport_get_socket_fd(context) == -1
    );
    assert(
        linux_coap_download_transport_get_timeout_ms(context) == 0
    );

    /*
    * The first scheduled process() call resolves the host and creates
    * a libcoap client session. No response socket event is required yet.
    */
    assert(
        linux_coap_download_transport_process(context, false) ==
        FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    assert(
        linux_coap_download_transport_get_socket_fd(context) >= 0
    );
    assert(
        linux_coap_download_transport_get_timeout_ms(context) == 0
    );

    /*
    * The second scheduled process() call sends the initial CoAP GET.
    * Further processing waits for a response or retransmission timer.
    */
    assert(
        linux_coap_download_transport_process(context, false) ==
        FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );

    assert(
        linux_coap_download_transport_get_timeout_ms(context) > 0
    );

    /* cancel() releases the pending request. */
    assert(
        transport.cancel(transport.context) ==
        FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK
    );
    assert(
        linux_coap_download_transport_get_socket_fd(context) == -1
    );
    assert(
        linux_coap_download_transport_get_timeout_ms(context) == -1
    );

    linux_coap_download_transport_destroy(context);

    /* destroy() disconnects the public function table. */
    assert(transport.context == NULL);
    assert(transport.start == NULL);
    assert(transport.cancel == NULL);

    puts("linux CoAP download transport test passed");
    return 0;
}