#include "firmware_update_service.h"
#include "linux_firmware_update_backend.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *staging_path =
        "/tmp/ota-linux-reference-firmware-backend-test.bin";
    const uint8_t firmware[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t stored_firmware[sizeof(firmware)];
    linux_firmware_update_backend_context_t backend_context;
    firmware_update_backend_t backend;
    firmware_update_service_t service;
    FILE *staging_file;

    /* Remove a stale test artifact from a previous interrupted run. */
    remove(staging_path);

    assert(
        linux_firmware_update_backend_init(
            &backend_context,
            staging_path,
            &backend
        )
    );
    assert(firmware_update_service_init(&service, &backend));

    /* Download two chunks into the Linux staging file. */
    assert(
        firmware_update_service_begin_download(
            &service,
            sizeof(firmware)
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(
        firmware_update_service_write_chunk(
            &service,
            firmware,
            2
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(
        firmware_update_service_write_chunk(
            &service,
            firmware + 2,
            2
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(
        firmware_update_service_finish_download(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );

    assert(backend_context.package_ready);
    assert(service.state == FIRMWARE_UPDATE_STATE_DOWNLOADED);

    /* Verify that the Backend stored the exact bytes. */
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

    /* Simulate installation and successful boot confirmation. */
    assert(
        firmware_update_service_install(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(backend_context.install_requested);
    assert(service.state == FIRMWARE_UPDATE_STATE_UPDATING);

    assert(
        firmware_update_service_recover_after_boot(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(service.state == FIRMWARE_UPDATE_STATE_IDLE);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_SUCCESS);

    linux_firmware_update_backend_deinit(&backend_context);
    remove(staging_path);

    puts("linux firmware update backend test passed");
    return 0;
}