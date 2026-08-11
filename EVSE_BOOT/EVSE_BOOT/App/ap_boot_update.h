#ifndef EVSE_BOOT_APP__AP_BOOT_UPDATE_H
#define EVSE_BOOT_APP__AP_BOOT_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#define AP_BOOT_UPDATE_VECTOR_TABLE_SIZE    8U

// tables
typedef enum {
    AP_BOOT_UPDATE_STATE_IDLE = 0,
    AP_BOOT_UPDATE_STATE_ERASING,
    AP_BOOT_UPDATE_STATE_RECEIVING,
    AP_BOOT_UPDATE_STATE_VERIFYING,
    AP_BOOT_UPDATE_STATE_COMPLETE,
    AP_BOOT_UPDATE_STATE_FAILED
} ap_boot_update_state_t;

typedef enum {
    AP_BOOT_UPDATE_ERROR_NONE = 0,

    AP_BOOT_UPDATE_ERROR_NULL_CONTEXT,
    AP_BOOT_UPDATE_ERROR_NOT_INITIALIZED,
    AP_BOOT_UPDATE_ERROR_INVALID_STATE,

    AP_BOOT_UPDATE_ERROR_STORAGE_NOT_READY,
    AP_BOOT_UPDATE_ERROR_INVALID_IMAGE_SIZE,
    AP_BOOT_UPDATE_ERROR_INVALID_DATA,
    AP_BOOT_UPDATE_ERROR_INVALID_OFFSET,
    AP_BOOT_UPDATE_ERROR_INVALID_LENGTH,

    AP_BOOT_UPDATE_ERROR_ERASE_FAILED,
    AP_BOOT_UPDATE_ERROR_WRITE_FAILED,
    AP_BOOT_UPDATE_ERROR_CRC_UPDATE_FAILED,

    AP_BOOT_UPDATE_ERROR_SIZE_MISMATCH,
    AP_BOOT_UPDATE_ERROR_CRC_MISMATCH,
    AP_BOOT_UPDATE_ERROR_APPLICATION_INVALID,

    AP_BOOT_UPDATE_ERROR_ABORTED
} ap_boot_update_error_t;

typedef struct {
    uint32_t address;
    uint32_t sector;
    uint32_t hal_error;
    uint32_t expected;
    uint32_t actual;
} ap_boot_update_storage_error_t;

typedef struct {
    bool initialized;

    ap_boot_update_state_t state;
    ap_boot_update_error_t last_error;

    uint32_t image_size;
    uint32_t expected_crc32;

    uint32_t received_size;
    uint32_t next_offset;

    // Final XOR 적용 전의 CRC32 누적 상태
    uint32_t running_crc32;
    // END_UPDATE에서 확정된 최종 CRC32
    uint32_t calculated_crc32;
    // Application Vector Table의 첫 8바이트
    uint8_t pending_vector_table[AP_BOOT_UPDATE_VECTOR_TABLE_SIZE];
    uint32_t pending_vector_size;

    ap_boot_update_storage_error_t storage_error;
} ap_boot_update_context_t;

// functions
bool ApBootUpdate_Init(ap_boot_update_context_t *context);
bool ApBootUpdate_Start(ap_boot_update_context_t *context, uint32_t image_size, uint32_t expected_crc32);
bool ApBootUpdate_WriteData(ap_boot_update_context_t *context, uint32_t offset, const uint8_t *data, uint32_t length);
bool ApBootUpdate_End(ap_boot_update_context_t *context);
void ApBootUpdate_Abort(ap_boot_update_context_t *context);

bool ApBootUpdate_IsInitialized(const ap_boot_update_context_t *context);

ap_boot_update_state_t ApBootUpdate_GetState(const ap_boot_update_context_t *context);
ap_boot_update_error_t ApBootUpdate_GetLastError(const ap_boot_update_context_t *context);

uint32_t ApBootUpdate_GetImageSize(const ap_boot_update_context_t *context);
uint32_t ApBootUpdate_GetReceivedSize(const ap_boot_update_context_t *context);
uint32_t ApBootUpdate_GetNextOffset(const ap_boot_update_context_t *context);
uint32_t ApBootUpdate_GetCalculatedCrc32(const ap_boot_update_context_t *context);
uint32_t ApBootUpdate_GetProgressPercent(const ap_boot_update_context_t *context);

#endif // EVSE_BOOT_APP__AP_BOOT_UPDATE_H