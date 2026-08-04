#include "firmware_update_service.h"

static firmware_update_result_t prv_map_backend_status(
    firmware_backend_status_t backend_status)
{
    switch (backend_status)
    {
    case FIRMWARE_BACKEND_STATUS_OK:
        return FIRMWARE_UPDATE_RESULT_INITIAL;

    case FIRMWARE_BACKEND_STATUS_NO_STORAGE:
        return FIRMWARE_UPDATE_RESULT_NO_STORAGE;

    case FIRMWARE_BACKEND_STATUS_OUT_OF_MEMORY:
        return FIRMWARE_UPDATE_RESULT_OUT_OF_MEMORY;

    case FIRMWARE_BACKEND_STATUS_INTEGRITY_FAILURE:
        return FIRMWARE_UPDATE_RESULT_INTEGRITY_FAILURE;

    case FIRMWARE_BACKEND_STATUS_UNSUPPORTED_PACKAGE:
        return FIRMWARE_UPDATE_RESULT_UNSUPPORTED_PACKAGE;

    case FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE:
    case FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE:
    default:
        return FIRMWARE_UPDATE_RESULT_UPDATE_FAILURE;
    }
}


bool firmware_update_service_init(
    firmware_update_service_t *service,
    const firmware_update_backend_t *backend)
{
    if (service == NULL || backend == NULL)
        return false;

    if (backend->prepare == NULL ||
        backend->write_chunk == NULL ||
        backend->finish_download == NULL ||
        backend->install == NULL ||
        backend->cancel == NULL ||
        backend->recover_after_boot == NULL)
        return false;

    service->backend = *backend;
    service->state = FIRMWARE_UPDATE_STATE_IDLE;
    service->update_result = FIRMWARE_UPDATE_RESULT_INITIAL;
    service->download_offset = 0;
    service->severity = FIRMWARE_UPDATE_SEVERITY_MANDATORY;
    service->maximum_defer_period_seconds = 0;

    return true;
}

firmware_update_service_status_t firmware_update_service_begin_download(
    firmware_update_service_t *service,
    size_t package_size)
{
    firmware_backend_status_t backend_status;

    if (service == NULL || package_size == 0)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    if (service->state != FIRMWARE_UPDATE_STATE_IDLE)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE;

    service->update_result = FIRMWARE_UPDATE_RESULT_INITIAL;
    service->download_offset = 0;

    backend_status = service->backend.prepare(
        service->backend.context,
        package_size
    );

    if (backend_status != FIRMWARE_BACKEND_STATUS_OK)
    {
        service->update_result = prv_map_backend_status(backend_status);
        return FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE;
    }

    service->state = FIRMWARE_UPDATE_STATE_DOWNLOADING;
    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}

firmware_update_service_status_t firmware_update_service_write_chunk(
    firmware_update_service_t *service,
    const uint8_t *data,
    size_t length)
{
    firmware_backend_status_t backend_status;

    if (service == NULL || data == NULL || length == 0)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    if (service->state != FIRMWARE_UPDATE_STATE_DOWNLOADING)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE;

    backend_status = service->backend.write_chunk(
        service->backend.context,
        service->download_offset,
        data,
        length
    );

    if (backend_status != FIRMWARE_BACKEND_STATUS_OK)
    {
        service->state = FIRMWARE_UPDATE_STATE_IDLE;
        service->update_result = prv_map_backend_status(backend_status);
        service->download_offset = 0;
        return FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE;
    }

    service->download_offset += length;
    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}

firmware_update_service_status_t firmware_update_service_finish_download(
    firmware_update_service_t *service)
{
    firmware_backend_status_t backend_status;

    if (service == NULL)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    if (service->state != FIRMWARE_UPDATE_STATE_DOWNLOADING ||
        service->download_offset == 0)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE;

    backend_status = service->backend.finish_download(
        service->backend.context
    );

    if (backend_status != FIRMWARE_BACKEND_STATUS_OK)
    {
        service->state = FIRMWARE_UPDATE_STATE_IDLE;
        service->update_result = prv_map_backend_status(backend_status);
        service->download_offset = 0;
        return FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE;
    }

    service->state = FIRMWARE_UPDATE_STATE_DOWNLOADED;
    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}

firmware_update_service_status_t firmware_update_service_install(
    firmware_update_service_t *service)
{
    firmware_backend_status_t backend_status;

    if (service == NULL)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    if (service->state != FIRMWARE_UPDATE_STATE_DOWNLOADED)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE;

    service->state = FIRMWARE_UPDATE_STATE_UPDATING;
    service->update_result = FIRMWARE_UPDATE_RESULT_INITIAL;
    service->download_offset = 0;

    backend_status = service->backend.install(
        service->backend.context
    );

    if (backend_status != FIRMWARE_BACKEND_STATUS_OK)
    {
        service->state = FIRMWARE_UPDATE_STATE_IDLE;
        service->update_result = prv_map_backend_status(backend_status);
        return FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE;
    }

    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}

firmware_update_service_status_t firmware_update_service_recover_after_boot(
    firmware_update_service_t *service)
{
    firmware_backend_status_t backend_status;
    firmware_backend_recovery_result_t recovery_result = FIRMWARE_BACKEND_RECOVERY_NONE;

    if (service == NULL)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    backend_status = service->backend.recover_after_boot(
        service->backend.context,
        &recovery_result
    );

    service->state = FIRMWARE_UPDATE_STATE_IDLE;
    service->download_offset = 0;

    if (backend_status != FIRMWARE_BACKEND_STATUS_OK)
    {
        service->update_result = prv_map_backend_status(backend_status);
        return FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE;
    }

    switch (recovery_result)
    {
    case FIRMWARE_BACKEND_RECOVERY_NONE:
        service->update_result = FIRMWARE_UPDATE_RESULT_INITIAL;
        break;

    case FIRMWARE_BACKEND_RECOVERY_SUCCESS:
        service->update_result = FIRMWARE_UPDATE_RESULT_SUCCESS;
        break;

    case FIRMWARE_BACKEND_RECOVERY_ROLLED_BACK:
        service->update_result = FIRMWARE_UPDATE_RESULT_UPDATE_FAILURE;
        break;

    default:
        service->update_result = FIRMWARE_UPDATE_RESULT_UPDATE_FAILURE;
        return FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE;
    }

    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}

firmware_update_service_status_t firmware_update_service_cancel(
    firmware_update_service_t *service)
{
    firmware_backend_status_t backend_status;

    if (service == NULL)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    if (service->state != FIRMWARE_UPDATE_STATE_DOWNLOADING &&
        service->state != FIRMWARE_UPDATE_STATE_DOWNLOADED)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE;

    backend_status = service->backend.cancel(
        service->backend.context
    );

    if (backend_status != FIRMWARE_BACKEND_STATUS_OK)
    {
        service->update_result = prv_map_backend_status(backend_status);
        return FIRMWARE_UPDATE_SERVICE_STATUS_BACKEND_FAILURE;
    }

    service->state = FIRMWARE_UPDATE_STATE_IDLE;
    service->update_result = FIRMWARE_UPDATE_RESULT_CANCELLED;
    service->download_offset = 0;

    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}

firmware_update_service_status_t firmware_update_service_set_severity(
    firmware_update_service_t *service,
    firmware_update_severity_t severity)
{
    if (service == NULL)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    switch (severity)
    {
    case FIRMWARE_UPDATE_SEVERITY_CRITICAL:
    case FIRMWARE_UPDATE_SEVERITY_MANDATORY:
    case FIRMWARE_UPDATE_SEVERITY_OPTIONAL:
        service->severity = severity;
        return FIRMWARE_UPDATE_SERVICE_STATUS_OK;

    default:
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;
    }
}

firmware_update_service_status_t firmware_update_service_set_maximum_defer_period(
    firmware_update_service_t *service,
    uint64_t seconds)
{
    if (service == NULL)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    service->maximum_defer_period_seconds = seconds;
    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}

firmware_update_service_status_t firmware_update_service_defer(
    firmware_update_service_t *service)
{
    if (service == NULL)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_ARGUMENT;

    if (service->state != FIRMWARE_UPDATE_STATE_DOWNLOADED)
        return FIRMWARE_UPDATE_SERVICE_STATUS_INVALID_STATE;

    if (service->severity == FIRMWARE_UPDATE_SEVERITY_CRITICAL ||
        service->maximum_defer_period_seconds == 0)
        return FIRMWARE_UPDATE_SERVICE_STATUS_NOT_ALLOWED;

    service->update_result = FIRMWARE_UPDATE_RESULT_DEFERRED;
    return FIRMWARE_UPDATE_SERVICE_STATUS_OK;
}