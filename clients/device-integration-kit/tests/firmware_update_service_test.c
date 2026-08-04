#include "firmware_update_service.h"

#include <assert.h>
#include <stdio.h>

static int prepare_call_count;
static size_t prepared_package_size;
static firmware_backend_status_t prepare_status = FIRMWARE_BACKEND_STATUS_OK;

static int write_chunk_call_count;
static size_t written_offset;
static size_t written_length;

static int finish_download_call_count;

static int install_call_count;

static int cancel_call_count;

static int recover_after_boot_call_count;
static firmware_backend_recovery_result_t next_recovery_result = FIRMWARE_BACKEND_RECOVERY_NONE;

static firmware_backend_status_t stub_prepare(
    void *context,
    size_t package_size)
{
    (void)context;
    prepare_call_count++;
    prepared_package_size = package_size;

    return prepare_status;
}

static firmware_backend_status_t stub_write_chunk(
    void *context,
    size_t offset,
    const uint8_t *data,
    size_t length)
{
    (void)context;
    (void)data;

    write_chunk_call_count++;
    written_offset = offset;
    written_length = length;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t stub_finish_download(void *context)
{
    (void)context;
    finish_download_call_count++;
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t stub_install(void *context)
{
    (void)context;
    install_call_count++;
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t stub_recover_after_boot(
    void *context,
    firmware_backend_recovery_result_t *recovery_result)
{
    (void)context;

    recover_after_boot_call_count++;
    *recovery_result = next_recovery_result;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t stub_cancel(void *context)
{
    (void)context;
    cancel_call_count++;
    return FIRMWARE_BACKEND_STATUS_OK;
}

int main(void)
{
    firmware_update_backend_t backend = {
        .context = NULL,
        .prepare = stub_prepare,
        .write_chunk = stub_write_chunk,
        .finish_download = stub_finish_download,
        .install = stub_install,
        .cancel = stub_cancel,
        .recover_after_boot = stub_recover_after_boot
    };
    firmware_update_service_t service;

    /* Service initialization */
    assert(firmware_update_service_init(&service, &backend));
    assert(service.state == FIRMWARE_UPDATE_STATE_IDLE);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_INITIAL);
    assert(service.download_offset == 0);
    assert(service.backend.prepare == stub_prepare);
    assert(service.severity == FIRMWARE_UPDATE_SEVERITY_MANDATORY);
    assert(service.maximum_defer_period_seconds == 0);

    /* Invalid arguments must not call the Backend. */
    assert(
        firmware_update_service_begin_download(NULL, 1024) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT
    );
    assert(
        firmware_update_service_begin_download(&service, 0) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT
    );
    assert(prepare_call_count == 0);

    /* A valid request starts downloading. */
    assert(
        firmware_update_service_begin_download(&service, 1024) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(prepare_call_count == 1);
    assert(prepared_package_size == 1024);
    assert(service.state == FIRMWARE_UPDATE_STATE_DOWNLOADING);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_INITIAL);
    assert(service.download_offset == 0);

    /* A received chunk is written at the current download offset. */
    {
        const uint8_t chunk[] = {0x10, 0x20, 0x30};

        assert(
            firmware_update_service_write_chunk(&service, chunk, sizeof(chunk))
            == FIRMWARE_UPDATE_SERVICE_STATUS_OK
        );
    }

    assert(write_chunk_call_count == 1);
    assert(written_offset == 0);
    assert(written_length == 3);
    assert(service.download_offset == 3);
    assert(service.state == FIRMWARE_UPDATE_STATE_DOWNLOADING);

    /* Download cannot start again while already downloading. */
    assert(
        firmware_update_service_begin_download(&service, 1024) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE
    );
    assert(prepare_call_count == 1);

    /* A completed download is verified by the Backend. */
    assert(
        firmware_update_service_finish_download(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(finish_download_call_count == 1);
    assert(service.state == FIRMWARE_UPDATE_STATE_DOWNLOADED);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_INITIAL);
    assert(service.download_offset == 3);

    /* Update Execute starts installation but does not mean success yet. */
    assert(
        firmware_update_service_install(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(install_call_count == 1);
    assert(service.state == FIRMWARE_UPDATE_STATE_UPDATING);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_INITIAL);
    assert(service.download_offset == 0);

    /* Backend storage failure must become LwM2M Update Result 2. */
    assert(firmware_update_service_init(&service, &backend));
    prepare_status = FIRMWARE_BACKEND_STATUS_NO_STORAGE;

    assert(
        firmware_update_service_begin_download(&service, 2048) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE
    );
    assert(prepare_call_count == 2);
    assert(service.state == FIRMWARE_UPDATE_STATE_IDLE);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_NO_STORAGE);

    /* A confirmed firmware boot completes the update successfully. */
    assert(firmware_update_service_init(&service, &backend));
    next_recovery_result = FIRMWARE_BACKEND_RECOVERY_SUCCESS;

    assert(
        firmware_update_service_recover_after_boot(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(recover_after_boot_call_count == 1);
    assert(service.state == FIRMWARE_UPDATE_STATE_IDLE);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_SUCCESS);
    assert(service.download_offset == 0);

    /* A normal boot has no firmware update result to recover. */
    assert(firmware_update_service_init(&service, &backend));
    next_recovery_result = FIRMWARE_BACKEND_RECOVERY_NONE;

    assert(
        firmware_update_service_recover_after_boot(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(recover_after_boot_call_count == 2);
    assert(service.state == FIRMWARE_UPDATE_STATE_IDLE);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_INITIAL);

    /* A rollback means that the firmware update failed. */
    assert(firmware_update_service_init(&service, &backend));
    next_recovery_result = FIRMWARE_BACKEND_RECOVERY_ROLLED_BACK;

    assert(
        firmware_update_service_recover_after_boot(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(recover_after_boot_call_count == 3);
    assert(service.state == FIRMWARE_UPDATE_STATE_IDLE);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_UPDATE_FAILURE);

    /* Cancel removes a downloaded package and resets the state machine. */
    assert(firmware_update_service_init(&service, &backend));
    service.state = FIRMWARE_UPDATE_STATE_DOWNLOADED;
    service.download_offset = 3;

    assert(
        firmware_update_service_cancel(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(cancel_call_count == 1);
    assert(service.state == FIRMWARE_UPDATE_STATE_IDLE);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_CANCELLED);
    assert(service.download_offset == 0);

    /* Firmware update policy can be configured by LwM2M resources. */
    assert(firmware_update_service_init(&service, &backend));

    assert(
        firmware_update_service_set_severity(
            &service,
            FIRMWARE_UPDATE_SEVERITY_OPTIONAL
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(service.severity == FIRMWARE_UPDATE_SEVERITY_OPTIONAL);

    assert(
        firmware_update_service_set_severity(
            &service,
            (firmware_update_severity_t)3
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT
    );
    assert(service.severity == FIRMWARE_UPDATE_SEVERITY_OPTIONAL);

    assert(
        firmware_update_service_set_maximum_defer_period(
            &service,
            3600
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(service.maximum_defer_period_seconds == 3600);

    /* An optional update may be deferred within the configured period. */
    assert(firmware_update_service_init(&service, &backend));
    service.state = FIRMWARE_UPDATE_STATE_DOWNLOADED;

    assert(
        firmware_update_service_set_severity(
            &service,
            FIRMWARE_UPDATE_SEVERITY_OPTIONAL
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(
        firmware_update_service_set_maximum_defer_period(
            &service,
            3600
        ) == FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );

    assert(
        firmware_update_service_defer(&service) ==
        FIRMWARE_UPDATE_SERVICE_STATUS_OK
    );
    assert(service.state == FIRMWARE_UPDATE_STATE_DOWNLOADED);
    assert(service.update_result == FIRMWARE_UPDATE_RESULT_DEFERRED);

    puts("firmware update service test passed");
    return 0;
}