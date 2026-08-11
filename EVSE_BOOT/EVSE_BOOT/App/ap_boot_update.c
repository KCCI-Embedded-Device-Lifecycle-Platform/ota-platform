#include "ap_boot_update.h"

#include <stddef.h>
#include <string.h>

#include "BSP/bsp_boot.h"
#include "BSP/bsp_storage.h"
#include "Middleware/boot_crc32.h"
#include "Common/boot_config.h"

// defines
#define AP_BOOT_UPDATE_PROGRAM_UNIT_SIZE    4U
#define AP_BOOT_UPDATE_NO_SECTOR_ERROR      0xFFFFFFFFUL

// inners
static void ApBootUpdate_ClearStorageError(ap_boot_update_context_t *context) {
    if (context == NULL) return;

    memset(&context->storage_error, 0, sizeof(context->storage_error));

    context->storage_error.sector = AP_BOOT_UPDATE_NO_SECTOR_ERROR;
}

static void ApBootUpdate_CopyStorageError(ap_boot_update_context_t *context, const bsp_storage_error_info_t *storage_error) {
    if ((context == NULL) || (storage_error == NULL)) return;

    context->storage_error.address = storage_error->address;
    context->storage_error.sector = storage_error->sector;
    context->storage_error.hal_error = storage_error->hal_error;
    context->storage_error.expected = storage_error->expected;
    context->storage_error.actual = storage_error->actual;
}

static bool ApBootUpdate_SetFailed(ap_boot_update_context_t *context, ap_boot_update_error_t error) {
    if (context == NULL) return false;

    context->state = AP_BOOT_UPDATE_STATE_FAILED;
    context->last_error = error;

    return false;
}

static void ApBootUpdate_ClearPendingVector(ap_boot_update_context_t *context) {
    if (context == NULL) return;

    // Erase된 Flash와 같은 값으로 초기화
    memset(context->pending_vector_table, 0xFF, sizeof(context->pending_vector_table));

    context->pending_vector_size = 0U;
}

static void ApBootUpdate_ClearTransferInfo(ap_boot_update_context_t *context) {
    if (context == NULL) return;

    context->image_size = 0U;
    context->expected_crc32 = 0U;
    context->received_size = 0U;
    context->next_offset = 0U;
    context->running_crc32 = BootCrc32_Begin();
    context->calculated_crc32 = 0U;

    ApBootUpdate_ClearPendingVector(context);
    ApBootUpdate_ClearStorageError(context);
}

static bool ApBootUpdate_IsIntermediateLengthValid(uint32_t length) {
    return (length & (AP_BOOT_UPDATE_PROGRAM_UNIT_SIZE - 1U)) == 0U;
}

static bool ApBootUpdate_IsInitialMspValid(uint32_t initial_msp) {
    bool main_sram_valid;
    bool ccmram_valid;

    // Stack Pointer 최소 4바이트 정렬
    if ((initial_msp & (AP_BOOT_UPDATE_PROGRAM_UNIT_SIZE - 1U)) != 0U) return false;

    main_sram_valid = (initial_msp >= BOOT_SRAM_START_ADDRESS) && (initial_msp <= BOOT_SRAM_STACK_TOP);
    ccmram_valid = (initial_msp >= BOOT_CCMRAM_START_ADDRESS) && (initial_msp <= BOOT_CCMRAM_STACK_TOP);

    return main_sram_valid || ccmram_valid;
}

static bool ApBootUpdate_IsPendingVectorValid(const ap_boot_update_context_t *context) {
    uint32_t initial_msp;
    uint32_t reset_handler;
    uint32_t reset_address;

    uint32_t application_start;
    uint32_t application_end;

    if (context == NULL) return false;
    if (context->pending_vector_size != AP_BOOT_UPDATE_VECTOR_TABLE_SIZE) return false;
    if (context->image_size < AP_BOOT_UPDATE_VECTOR_TABLE_SIZE) return false;
    if (context->image_size > BspStorage_GetApplicationCapacity()) return false;

    // uint8_t 배열 직접 uint32_t 포인터 변환 시 정렬 문제 >> memcpy 사용
    memcpy(&initial_msp, &context->pending_vector_table[0], sizeof(initial_msp));
    memcpy(&reset_handler, &context->pending_vector_table[4], sizeof(reset_handler));

    if (!ApBootUpdate_IsInitialMspValid(initial_msp)) return false;

    // Reset Handler Thumb 주소 >> bit 0 반드시 1
    if ((reset_handler & 0x1UL) == 0U) return false;

    reset_address = reset_handler & ~0x1UL;

    application_start = BspStorage_GetApplicationStartAddress();
    application_end = application_start + context->image_size;

    // Reset Handler가 실제 전송 이미지 범위 안에 있어야 합니다.
    if (reset_address < application_start) return false;
    if (reset_address >= application_end) return false;

    return true;
}

// functions
bool ApBootUpdate_Init(ap_boot_update_context_t *context) {
    // Update Context를 IDLE 상태로 초기화
    // Flash 접근x
    if (context == NULL) return false;

    memset(context, 0, sizeof(*context));

    context->initialized = true;
    context->state = AP_BOOT_UPDATE_STATE_IDLE;
    context->last_error = AP_BOOT_UPDATE_ERROR_NONE;
    context->running_crc32 = BootCrc32_Begin();
    ApBootUpdate_ClearPendingVector(context);
    context->storage_error.sector = AP_BOOT_UPDATE_NO_SECTOR_ERROR;

    return true;
}

bool ApBootUpdate_Start(ap_boot_update_context_t *context, uint32_t image_size, uint32_t expected_crc32) {
    // 업데이트 시작
    // 수행 작업:
    // - 상태 검사
    // - 이미지 크기 검사
    // - Application Flash Erase
    // - CRC32 및 Offset 초기화
    // - RECEIVING 상태 진입
    bsp_storage_status_t storage_status;
    bsp_storage_error_info_t storage_error;

    if (context == NULL) return false;

    if (!context->initialized) {
        context->last_error = AP_BOOT_UPDATE_ERROR_NOT_INITIALIZED;

        return false;
    }

    // 현재 :: 한 번의 Bootloader 실행 >> 한 번의 OTA만 수행
    // COMPLETE 상태에서 다시 시작하려면 Reset 또는 ABORT 필요
    if (context->state != AP_BOOT_UPDATE_STATE_IDLE) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_STATE;

        return false;
    }

    if (!BspStorage_IsInitialized()) return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_STORAGE_NOT_READY);
    
    // 최소한 Initial MSP와 Reset Handler
    if (image_size < AP_BOOT_UPDATE_VECTOR_TABLE_SIZE) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_IMAGE_SIZE;

        return false;
    }

    if (!BspStorage_IsImageSizeValid(image_size))
        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_INVALID_IMAGE_SIZE);

    // Erase 실패 시에도 어떤 이미지를 업데이트하려 했는지 진단할 수 있도록 먼저 저장
    context->image_size = image_size;
    context->expected_crc32 = expected_crc32;
    context->received_size = 0U;
    context->next_offset = 0U;
    context->running_crc32 = BootCrc32_Begin();
    context->calculated_crc32 = 0U;
    context->last_error = AP_BOOT_UPDATE_ERROR_NONE;

    ApBootUpdate_ClearPendingVector(context);
    ApBootUpdate_ClearStorageError(context);

    context->state = AP_BOOT_UPDATE_STATE_ERASING;

    storage_status = BspStorage_EraseApplication(context->image_size, &storage_error);

    if (storage_status != BSP_STORAGE_STATUS_OK) {
        ApBootUpdate_CopyStorageError(context, &storage_error);

        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_ERASE_FAILED);
    }

    // Erase가 끝난 뒤 0x08020000의 첫 8바이트 : 0xFFFFFFFF
    // END_UPDATE 성공 전까지 이 상태 유지
    context->state = AP_BOOT_UPDATE_STATE_RECEIVING;

    return true;
}

bool ApBootUpdate_WriteData(ap_boot_update_context_t *context, uint32_t offset, const uint8_t *data, uint32_t length) {
    // Firmware 데이터 한 블록을 Flash에 기록
    // offset은 Application 시작 주소 기준
    // offset 0~7 :  Flash에 쓰지 않고 pending_vector_table에 저장
    // offset 8 이상 : 기존 방식대로 Flash에 기록
    uint32_t remaining_size;
    uint32_t next_received_size;

    uint32_t data_index;
    uint32_t write_offset;
    uint32_t write_length;

    bool final_chunk;

    mw_status_t crc_status;

    bsp_storage_status_t storage_status;
    bsp_storage_error_info_t storage_error;

    if (context == NULL) return false;

    if (!context->initialized) {
        context->last_error = AP_BOOT_UPDATE_ERROR_NOT_INITIALIZED;

        return false;
    }

    if (context->state != AP_BOOT_UPDATE_STATE_RECEIVING) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_STATE;

        return false;
    }

    if ((data == NULL) || (length == 0U)) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_DATA;

        return false;
    }

    // Offset이 예상값과 다르면 패킷 누락, 중복, 순서 변경 중 하나로 판단
    if (offset != context->next_offset) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_OFFSET;

        return false;
    }

    if (context->received_size > context->image_size)
        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_INVALID_LENGTH);

    remaining_size = context->image_size - context->received_size;

    // 덧셈 Overflow를 피하기 위해 남은 크기와 비교
    if (length > remaining_size) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_LENGTH;

        return false;
    }

    next_received_size = context->received_size + length;

    final_chunk = next_received_size == context->image_size;

    // 마지막 블록이 아니라면 다음 Offset도 Word 정렬 유지 >> Length 4바이트 배수
    if ((!final_chunk) && (!ApBootUpdate_IsIntermediateLengthValid(length))) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_LENGTH;

        return false;
    }

    ApBootUpdate_ClearStorageError(context);

    data_index = 0U;
    write_offset = offset;

    // Application 첫 8바이트 Flash 기록 x
    // 첫 DATA가 8바이트보다 작아 여러 패킷에 걸쳐 Vector Table 수신되는 경우도 처리
    if (write_offset < AP_BOOT_UPDATE_VECTOR_TABLE_SIZE) {
        while ((data_index < length) && (write_offset < AP_BOOT_UPDATE_VECTOR_TABLE_SIZE)) {
            context->pending_vector_table[write_offset] = data[data_index];

            data_index++;
            write_offset++;
        }

        if (context->pending_vector_size < write_offset) context->pending_vector_size = write_offset;
    }

    // Vector Table 제외, 실제 Flash에 쓸 데이터 길이
    write_length = length - data_index;

    if (write_length > 0U) {
        storage_status = BspStorage_Write(write_offset, &data[data_index], write_length, final_chunk, &storage_error);

        if (storage_status != BSP_STORAGE_STATUS_OK) {
            ApBootUpdate_CopyStorageError(context, &storage_error);

            return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_WRITE_FAILED);
        }
    }

    // Flash Write와 Read-back 검증이 성공한 데이터만 CRC32 누적값에 포함
    crc_status = BootCrc32_Update(&context->running_crc32, data, length);

    if (crc_status != MW_STATUS_OK) return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_CRC_UPDATE_FAILED);

    // ACK의 next_offset 원본 BIN 기준
    // Vector Table을 Flash에 쓰지 않았더라도 Sender에 기존과 동일한 next_offset 반환
    context->received_size = next_received_size;
    context->next_offset = next_received_size;
    context->last_error = AP_BOOT_UPDATE_ERROR_NONE;

    return true;
}

bool ApBootUpdate_End(ap_boot_update_context_t *context) {
    // 전체 이미지 검증 및 Vector Table 최종 Commit
    // 1. 전체 수신 크기 확인
    // 2. CRC32 확인
    // 3. RAM에 보관한 Vector Table 확인
    // 4. Vector Table을 Flash에 마지막으로 기록
    // 5. 실제 Flash Application Vector 재검사
    // 6. COMPLETE 상태 진입
    bsp_storage_status_t storage_status;
    bsp_storage_error_info_t storage_error;

    if (context == NULL) return false;

    if (!context->initialized) {
        context->last_error = AP_BOOT_UPDATE_ERROR_NOT_INITIALIZED;

        return false;
    }

    if (context->state != AP_BOOT_UPDATE_STATE_RECEIVING) {
        context->last_error = AP_BOOT_UPDATE_ERROR_INVALID_STATE;

        return false;
    }

    context->state = AP_BOOT_UPDATE_STATE_VERIFYING;

    if (context->received_size != context->image_size)
        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_SIZE_MISMATCH);

    if (context->next_offset != context->image_size)
        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_SIZE_MISMATCH);

    // Vector Table 첫 8바이트가 모두 수신됐는지 확인
    if (context->pending_vector_size != AP_BOOT_UPDATE_VECTOR_TABLE_SIZE)
        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_APPLICATION_INVALID);

    context->calculated_crc32 = BootCrc32_End(context->running_crc32);

    if (context->calculated_crc32 != context->expected_crc32)
        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_CRC_MISMATCH);

    // 아직 Flash에 기록하지 않은 RAM Vector Table 검사
    if (!ApBootUpdate_IsPendingVectorValid(context))
        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_APPLICATION_INVALID);

    ApBootUpdate_ClearStorageError(context);

    // 크기, CRC32, Vector 검사가 모두 성공한 뒤 Application 첫 8바이트를 마지막으로 Flash에 기록
    // 이 시점 전까지 Application은 유효하지 않음
    storage_status = BspStorage_Write(0U, context->pending_vector_table, AP_BOOT_UPDATE_VECTOR_TABLE_SIZE, true, &storage_error);

    if (storage_status != BSP_STORAGE_STATUS_OK) {
        ApBootUpdate_CopyStorageError(context, &storage_error);

        return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_WRITE_FAILED);
    }

    // 실제 Flash에 최종 기록된 Vector Table을 기존 BSP 검사 함수로 다시 확인
    if (!BspBoot_IsApplicationValid()) return ApBootUpdate_SetFailed(context, AP_BOOT_UPDATE_ERROR_APPLICATION_INVALID);

    context->state = AP_BOOT_UPDATE_STATE_COMPLETE;
    context->last_error = AP_BOOT_UPDATE_ERROR_NONE;

    return true;
}

void ApBootUpdate_Abort(ap_boot_update_context_t *context) {
    // 진행 중인 업데이트 취소 >> IDLE로 돌아감
    // 이미 Erase된 Application 복구x
    if ((context == NULL) || (!context->initialized)) return;

    ApBootUpdate_ClearTransferInfo(context);

    context->state = AP_BOOT_UPDATE_STATE_IDLE;
    context->last_error = AP_BOOT_UPDATE_ERROR_ABORTED;
}

bool ApBootUpdate_IsInitialized(const ap_boot_update_context_t *context) {
    if (context == NULL) return false;

    return context->initialized;
}

ap_boot_update_state_t ApBootUpdate_GetState(const ap_boot_update_context_t *context) {
    if (context == NULL) return AP_BOOT_UPDATE_STATE_FAILED;

    return context->state;
}

ap_boot_update_error_t ApBootUpdate_GetLastError(const ap_boot_update_context_t *context) {
    if (context == NULL) return AP_BOOT_UPDATE_ERROR_NULL_CONTEXT;

    return context->last_error;
}

uint32_t ApBootUpdate_GetImageSize(const ap_boot_update_context_t *context) {
    if (context == NULL) return 0U;

    return context->image_size;
}

uint32_t ApBootUpdate_GetReceivedSize(const ap_boot_update_context_t *context) {
    if (context == NULL) return 0U;

    return context->received_size;
}

uint32_t ApBootUpdate_GetNextOffset(const ap_boot_update_context_t *context) {
    if (context == NULL) return 0U;

    return context->next_offset;
}

uint32_t ApBootUpdate_GetCalculatedCrc32(const ap_boot_update_context_t *context) {
    if (context == NULL) return 0U;

    return context->calculated_crc32;
}

uint32_t ApBootUpdate_GetProgressPercent(const ap_boot_update_context_t *context) {
    if ((context == NULL) || (context->image_size == 0U)) return 0U;

    if (context->received_size >= context->image_size) return 100U;

    // image_size가 약 2MB이므로 received_size * 100 연산은 uint32_t 범위 안에서 안전
    return (context->received_size * 100U) / context->image_size;
}