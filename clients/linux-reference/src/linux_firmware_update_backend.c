#include "linux_firmware_update_backend.h"
#include <errno.h>
#include <string.h>

static firmware_backend_status_t prv_prepare(
    void *backend_context,
    size_t package_size)
{
    linux_firmware_update_backend_context_t *context =
        (linux_firmware_update_backend_context_t *)backend_context;

    if (context == NULL ||
        context->staging_path == NULL ||
        package_size == 0)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (context->staging_file != NULL)
    {
        FILE *staging_file = context->staging_file;
        context->staging_file = NULL;

        if (fclose(staging_file) != 0)
            return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    }

    context->expected_size = 0;
    context->received_size = 0;
    context->package_ready = false;
    context->install_requested = false;

    context->staging_file = fopen(context->staging_path, "wb");

    if (context->staging_file == NULL)
        return FIRMWARE_BACKEND_STATUS_NO_STORAGE;

    context->expected_size = package_size;
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prv_write_chunk(
    void *backend_context,
    size_t offset,
    const uint8_t *data,
    size_t length)
{
    linux_firmware_update_backend_context_t *context =
        (linux_firmware_update_backend_context_t *)backend_context;
    size_t written_size;

    if (context == NULL ||
        context->staging_file == NULL ||
        data == NULL ||
        length == 0)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (offset != context->received_size)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (context->received_size > context->expected_size ||
        length > context->expected_size - context->received_size)
        return FIRMWARE_BACKEND_STATUS_INTEGRITY_FAILURE;

    written_size = fwrite(
        data,
        1,
        length,
        context->staging_file
    );

    context->received_size += written_size;

    if (written_size != length)
        return FIRMWARE_BACKEND_STATUS_NO_STORAGE;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prv_finish_download(void *backend_context)
{
    linux_firmware_update_backend_context_t *context =
        (linux_firmware_update_backend_context_t *)backend_context;

    if (context == NULL || context->staging_file == NULL)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->package_ready = false;

    if (fflush(context->staging_file) != 0)
    {
        fclose(context->staging_file);
        context->staging_file = NULL;
        return FIRMWARE_BACKEND_STATUS_NO_STORAGE;
    }

    if (fclose(context->staging_file) != 0)
    {
        context->staging_file = NULL;
        return FIRMWARE_BACKEND_STATUS_NO_STORAGE;
    }

    context->staging_file = NULL;

    if (context->received_size != context->expected_size)
        return FIRMWARE_BACKEND_STATUS_INTEGRITY_FAILURE;

    /*
     * The Linux reference currently verifies transfer size only.
     * A production Backend must also verify manifest compatibility,
     * image hash, signature, and anti-rollback policy.
     */
    context->package_ready = true;
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prv_install(void *backend_context)
{
    linux_firmware_update_backend_context_t *context =
        (linux_firmware_update_backend_context_t *)backend_context;

    FILE *marker_file;

    if (context == NULL ||
        context->staging_file != NULL ||
        !context->package_ready)
        return FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE;
    
    marker_file = fopen(context->install_marker_path, "wb");

    if (marker_file == NULL)
        return FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE;

    if (fclose(marker_file) != 0)
    {
        remove(context->install_marker_path);
        return FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE;
    }

    context->install_requested = true;
    /*
     * Linux reference simulates boot target selection and reboot request.
     * A device Backend performs Bootloader metadata update and reboot here.
     */
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prv_recover_after_boot(
    void *backend_context,
    firmware_backend_recovery_result_t *recovery_result)
{
    linux_firmware_update_backend_context_t *context =
        (linux_firmware_update_backend_context_t *)backend_context;

    if (context == NULL || recovery_result == NULL)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    
    FILE *marker_file;

    marker_file = fopen(context->install_marker_path, "rb");

    if (marker_file == NULL)
    {
        if (errno == ENOENT)
        {
            *recovery_result = FIRMWARE_BACKEND_RECOVERY_NONE;
            return FIRMWARE_BACKEND_STATUS_OK;
        }

        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    }

    if (fclose(marker_file) != 0)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    /*
    * 성공한 package를 먼저 정리한다.
    * 이미 Bootloader가 이동/삭제했을 수 있으므로 ENOENT는 허용한다.
    */
    if (remove(context->staging_path) != 0 && errno != ENOENT)
    {
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    }

    /*
    * 모든 정리가 끝난 뒤 마커를 제거한다.
    * 중간 실패 시 다음 부팅에서 recovery를 재시도할 수 있다.
    */
    if (remove(context->install_marker_path) != 0)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    /*
     * The Linux reference simulates successful boot confirmation.
     * A device Backend reads Bootloader confirmation or rollback metadata.
     */
    *recovery_result = FIRMWARE_BACKEND_RECOVERY_SUCCESS;

    context->install_requested = false;
    context->package_ready = false;
    context->expected_size = 0;
    context->received_size = 0;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prv_cancel(void *backend_context)
{
    linux_firmware_update_backend_context_t *context =
        (linux_firmware_update_backend_context_t *)backend_context;

    if (context == NULL || context->staging_path == NULL)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (context->staging_file != NULL)
    {
        fclose(context->staging_file);
        context->staging_file = NULL;
    }

    if (remove(context->staging_path) != 0 && errno != ENOENT)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->expected_size = 0;
    context->received_size = 0;
    context->package_ready = false;
    context->install_requested = false;

    return FIRMWARE_BACKEND_STATUS_OK;
}

bool linux_firmware_update_backend_init(
    linux_firmware_update_backend_context_t *context,
    const char *staging_path,
    const char *install_marker_path,
    firmware_update_backend_t *backend)
{
    if (context == NULL ||
        staging_path == NULL ||
        staging_path[0] == '\0' ||
        install_marker_path == NULL ||
        install_marker_path[0] == '\0' ||
        backend == NULL)
        return false;

    memset(context, 0, sizeof(*context));
    context->staging_path = staging_path;
    context->install_marker_path = install_marker_path;

    backend->context = context;
    backend->prepare = prv_prepare;
    backend->write_chunk = prv_write_chunk;
    backend->finish_download = prv_finish_download;
    backend->install = prv_install;
    backend->cancel = prv_cancel;
    backend->recover_after_boot = prv_recover_after_boot;

    return true;
}

void linux_firmware_update_backend_deinit(
    linux_firmware_update_backend_context_t *context)
{
    if (context == NULL)
        return;

    if (context->staging_file != NULL)
        fclose(context->staging_file);

    /*
     * Deinit releases process resources but does not delete the package.
     * Package deletion belongs to cancel or successful installation cleanup.
     */
    memset(context, 0, sizeof(*context));
}