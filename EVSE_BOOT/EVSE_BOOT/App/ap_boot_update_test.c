#include "App/ap_boot_update_test.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "App/ap_boot_console.h"
#include "App/ap_boot_update.h"
#include "BSP/bsp_storage.h"

// defines
#define AP_BOOT_UPDATE_TEST_IMAGE_SIZE              32U
#define AP_BOOT_UPDATE_TEST_CHUNK_SIZE              16U
#define AP_BOOT_UPDATE_TEST_EXPECTED_CRC32          0x4DD6EE6EUL
#define AP_BOOT_UPDATE_TEST_LOG_BUFFER_SIZE         256U

// 테스트 이미지 구조 >> 유효한 Vector Table 검사를 통과하도록 만든 최소 테스트 이미지
// 0x08020000:      Initial MSP = 0x20010000
// 0x08020004:      Reset Handler = 0x08020009
// 0x08020008:      0xE7FE = Thumb "B ." 무한 루프

// inners
static const uint8_t s_update_test_image[AP_BOOT_UPDATE_TEST_IMAGE_SIZE] = {
    /* Initial MSP: 0x20010000 */
    0x00U,
    0x00U,
    0x01U,
    0x20U,

    /* Reset Handler: 0x08020009 */
    0x09U,
    0x00U,
    0x02U,
    0x08U,

    /* 0xE7FE: B . */
    0xFEU,
    0xE7U,

    /* 0xBF00: NOP */
    0x00U,
    0xBFU,
    0x00U,
    0xBFU,
    0x00U,
    0xBFU,

    /* Test pattern */
    0x11U,
    0x22U,
    0x33U,
    0x44U,
    0x55U,
    0x66U,
    0x77U,
    0x88U,
    0xAAU,
    0xBBU,
    0xCCU,
    0xDDU,
    0x12U,
    0x34U,
    0x56U,
    0x78U
};

static bool ApBootUpdateTest_PrintResult(const char *test_name, bool passed) {
    char log_buffer[AP_BOOT_UPDATE_TEST_LOG_BUFFER_SIZE];

    int written_length;

    if (test_name == NULL) return false;

    written_length = snprintf(log_buffer, sizeof(log_buffer), "[UPDATE TEST] %-32s : %s\r\n", test_name, passed ? "PASS" : "FAIL");

    if ((written_length > 0) && ((size_t)written_length < sizeof(log_buffer)))
        (void)ApBootConsole_SendString(log_buffer);
    else (void)ApBootConsole_SendString("[UPDATE TEST] Log formatting error\r\n");

    return passed;
}

static void ApBootUpdateTest_PrintContext(const char *stage, const ap_boot_update_context_t *context) {
    char log_buffer[AP_BOOT_UPDATE_TEST_LOG_BUFFER_SIZE];

    int written_length;

    if ((stage == NULL) || (context == NULL)) return;

    written_length = snprintf(log_buffer,
                              sizeof(log_buffer),
                              "[UPDATE TEST] Failure context: %.32s\r\n"
                              "  state         : %u\r\n"
                              "  last_error    : %u\r\n"
                              "  image_size    : %lu\r\n"
                              "  received_size : %lu\r\n"
                              "  next_offset   : %lu\r\n",
                              stage,
                              (unsigned int)context->state,
                              (unsigned int)context->last_error,
                              (unsigned long)context->image_size,
                              (unsigned long)context->received_size,
                              (unsigned long)context->next_offset);

    if ((written_length > 0) && ((size_t)written_length < sizeof(log_buffer)))
        (void)ApBootConsole_SendString(log_buffer);

    written_length = snprintf(log_buffer,
                              sizeof(log_buffer),
                              "  expected_crc32 : 0x%08lX\r\n"
                              "  calculated_crc : 0x%08lX\r\n"
                              "  flash_address  : 0x%08lX\r\n"
                              "  flash_sector   : 0x%08lX\r\n"
                              "  flash_hal_error: 0x%08lX\r\n",
                              (unsigned long)context->expected_crc32,
                              (unsigned long)context->calculated_crc32,
                              (unsigned long)context->storage_error.address,
                              (unsigned long)context->storage_error.sector,
                              (unsigned long)context->storage_error.hal_error);

    if ((written_length > 0) && ((size_t)written_length < sizeof(log_buffer)))
        (void)ApBootConsole_SendString(log_buffer);
}

static void ApBootUpdateTest_PrintStorageError(const bsp_storage_error_info_t *error_info) {
    char log_buffer[AP_BOOT_UPDATE_TEST_LOG_BUFFER_SIZE];

    int written_length;

    if (error_info == NULL) return;

    written_length = snprintf(log_buffer,
                              sizeof(log_buffer),
                              "[UPDATE TEST] Storage verification error\r\n"
                              "  status   : %u\r\n"
                              "  address  : 0x%08lX\r\n"
                              "  sector   : 0x%08lX\r\n"
                              "  hal_error: 0x%08lX\r\n"
                              "  expected : 0x%08lX\r\n"
                              "  actual   : 0x%08lX\r\n",
                              (unsigned int)error_info->status,
                              (unsigned long)error_info->address,
                              (unsigned long)error_info->sector,
                              (unsigned long)error_info->hal_error,
                              (unsigned long)error_info->expected,
                              (unsigned long)error_info->actual);

    if ((written_length > 0) && ((size_t)written_length < sizeof(log_buffer)))
        (void)ApBootConsole_SendString(log_buffer);
}

bool ApBootUpdateTest_Run(void) {
    ap_boot_update_context_t context;

    bsp_storage_status_t storage_status;
    bsp_storage_error_info_t storage_error;

    bool operation_result;

    (void)ApBootConsole_SendString("\r\n"
                                   "[UPDATE TEST] Start\r\n"
                                   "[UPDATE TEST] WARNING: Application Sector 5 "
                                   "will be erased\r\n"
                                   "[UPDATE TEST] Image size : 32 bytes\r\n"
                                   "[UPDATE TEST] CRC32      : 0x4DD6EE6E\r\n"
                                   "\r\n");

    // 1. Context 초기화
    operation_result = ApBootUpdate_Init(&context);

    if (!ApBootUpdateTest_PrintResult("Context initialization", operation_result)) return false;

    // 2. 초기 상태 검사
    operation_result = (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_IDLE) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_NONE) &&
                       (ApBootUpdate_GetImageSize(&context) == 0U) && (ApBootUpdate_GetReceivedSize(&context) == 0U) &&
                       (ApBootUpdate_GetNextOffset(&context) == 0U);

    if (!ApBootUpdateTest_PrintResult("Initial IDLE state", operation_result)) {
        ApBootUpdateTest_PrintContext("INITIAL_STATE", &context);

        return false;
    }

    // 3. START_UPDATE 전에 DATA 기록 시도
    // INVALID_STATE로 거부, 상태는 IDLE 유지해
    operation_result = !ApBootUpdate_WriteData(&context, 0U, &s_update_test_image[0], 4U);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_IDLE) && (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_INVALID_STATE);

    if (!ApBootUpdateTest_PrintResult("DATA before START rejected", operation_result)) {
        ApBootUpdateTest_PrintContext("DATA_BEFORE_START", &context);

        return false;
    }

    // 4. 업데이트 시작
    // 이 호출에서 실제 Sector 5 Erase 발생
    operation_result = ApBootUpdate_Start(&context, AP_BOOT_UPDATE_TEST_IMAGE_SIZE, AP_BOOT_UPDATE_TEST_EXPECTED_CRC32);

    if (!ApBootUpdateTest_PrintResult("START_UPDATE", operation_result)) {
        ApBootUpdateTest_PrintContext("START_UPDATE", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 5. Erase 후 RECEIVING 상태 검사
    operation_result = (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetImageSize(&context) == AP_BOOT_UPDATE_TEST_IMAGE_SIZE) &&
                       (ApBootUpdate_GetReceivedSize(&context) == 0U) && (ApBootUpdate_GetNextOffset(&context) == 0U) &&
                       (ApBootUpdate_GetProgressPercent(&context) == 0U);

    if (!ApBootUpdateTest_PrintResult("RECEIVING state after erase", operation_result)) {
        ApBootUpdateTest_PrintContext("AFTER_ERASE", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 6. 업데이트 진행 중 START_UPDATE 재호출
    // INVALID_STATE로 거부, 기존 업데이트는 RECEIVING 상태 유지
    operation_result = !ApBootUpdate_Start(&context, AP_BOOT_UPDATE_TEST_IMAGE_SIZE, AP_BOOT_UPDATE_TEST_EXPECTED_CRC32);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_INVALID_STATE);

    if (!ApBootUpdateTest_PrintResult("Duplicate START rejected", operation_result)) {
        ApBootUpdateTest_PrintContext("DUPLICATE_START", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 7. 잘못된 Offset
    // 첫 DATA는 Offset 0이어야 하지만 4 전달
    operation_result = !ApBootUpdate_WriteData(&context, 4U, &s_update_test_image[0], AP_BOOT_UPDATE_TEST_CHUNK_SIZE);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_INVALID_OFFSET) &&
                       (ApBootUpdate_GetReceivedSize(&context) == 0U) && (ApBootUpdate_GetNextOffset(&context) == 0U);

    if (!ApBootUpdateTest_PrintResult("Invalid offset rejected", operation_result)) {
        ApBootUpdateTest_PrintContext("INVALID_OFFSET", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 8. 중간 DATA의 잘못된 길이
    // 마지막 DATA가 아닌데 3바이트 >> 4바이트 Word 정렬 유지x
    operation_result = !ApBootUpdate_WriteData(&context, 0U, &s_update_test_image[0], 3U);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_INVALID_LENGTH) &&
                       (ApBootUpdate_GetReceivedSize(&context) == 0U);

    if (!ApBootUpdateTest_PrintResult("Unaligned length rejected", operation_result)) {
        ApBootUpdateTest_PrintContext("INVALID_LENGTH", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 9. 첫 번째 16바이트 기록
    operation_result = ApBootUpdate_WriteData(&context, 0U, &s_update_test_image[0], AP_BOOT_UPDATE_TEST_CHUNK_SIZE);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetReceivedSize(&context) == 16U) &&
                       (ApBootUpdate_GetNextOffset(&context) == 16U) &&
                       (ApBootUpdate_GetProgressPercent(&context) == 50U);

    if (!ApBootUpdateTest_PrintResult("First DATA chunk", operation_result)) {
        ApBootUpdateTest_PrintContext("FIRST_DATA", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 10. 동일 패킷 재전송
    // 다음 Offset 16 >> Offset 0 재전송 시 중복 패킷으로 판단하여 거부
    operation_result = !ApBootUpdate_WriteData(&context, 0U, &s_update_test_image[0], AP_BOOT_UPDATE_TEST_CHUNK_SIZE);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_INVALID_OFFSET) &&
                       (ApBootUpdate_GetReceivedSize(&context) == 16U) && (ApBootUpdate_GetNextOffset(&context) == 16U);

    if (!ApBootUpdateTest_PrintResult("Duplicate DATA rejected", operation_result)) {
        ApBootUpdateTest_PrintContext("DUPLICATE_DATA", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 11. 두 번째 16바이트 기록
    operation_result = ApBootUpdate_WriteData(&context, 16U, &s_update_test_image[16], AP_BOOT_UPDATE_TEST_CHUNK_SIZE);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetReceivedSize(&context) == AP_BOOT_UPDATE_TEST_IMAGE_SIZE) &&
                       (ApBootUpdate_GetNextOffset(&context) == AP_BOOT_UPDATE_TEST_IMAGE_SIZE) &&
                       (ApBootUpdate_GetProgressPercent(&context) == 100U);

    if (!ApBootUpdateTest_PrintResult("Final DATA chunk", operation_result)) {
        ApBootUpdateTest_PrintContext("FINAL_DATA", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 12. 이미지 크기를 초과하는 추가 DATA
    operation_result = !ApBootUpdate_WriteData(&context, AP_BOOT_UPDATE_TEST_IMAGE_SIZE, &s_update_test_image[0], 4U);
    operation_result = operation_result && (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_RECEIVING) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_INVALID_LENGTH) &&
                       (ApBootUpdate_GetReceivedSize(&context) == AP_BOOT_UPDATE_TEST_IMAGE_SIZE);

    if (!ApBootUpdateTest_PrintResult("Overflow DATA rejected", operation_result)) {
        ApBootUpdateTest_PrintContext("OVERFLOW_DATA", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 13. END_UPDATE
    // 크기, 누적 CRC32, Application Vector 검사
    operation_result = ApBootUpdate_End(&context);

    if (!ApBootUpdateTest_PrintResult("END_UPDATE", operation_result)) {
        ApBootUpdateTest_PrintContext("END_UPDATE", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 14. 최종 COMPLETE 상태와 CRC32 검사
    operation_result = (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_COMPLETE) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_NONE) &&
                       (ApBootUpdate_GetCalculatedCrc32(&context) == AP_BOOT_UPDATE_TEST_EXPECTED_CRC32);

    if (!ApBootUpdateTest_PrintResult("COMPLETE state and CRC32", operation_result)) {
        ApBootUpdateTest_PrintContext("COMPLETE_CHECK", &context);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 15. Flash 전체 이미지 최종 비교
    // ApBootUpdate_WriteData()에서도 Word 단위 Read-back 수행, 여기서는 전체 32바이트 다시 비교
    storage_status = BspStorage_Verify(0U, s_update_test_image, AP_BOOT_UPDATE_TEST_IMAGE_SIZE, &storage_error);

    operation_result = storage_status == BSP_STORAGE_STATUS_OK;

    if (!ApBootUpdateTest_PrintResult("Final Flash verification", operation_result)) {
        ApBootUpdateTest_PrintStorageError(&storage_error);

        ApBootUpdate_Abort(&context);

        return false;
    }

    // 16. Context 초기화 복귀 시험
    // Flash 내용 그대로 유지 Context만 IDLE로 회귀
    ApBootUpdate_Abort(&context);

    operation_result = (ApBootUpdate_GetState(&context) == AP_BOOT_UPDATE_STATE_IDLE) &&
                       (ApBootUpdate_GetLastError(&context) == AP_BOOT_UPDATE_ERROR_ABORTED) &&
                       (ApBootUpdate_GetImageSize(&context) == 0U) && (ApBootUpdate_GetReceivedSize(&context) == 0U) &&
                       (ApBootUpdate_GetNextOffset(&context) == 0U);

    if (!ApBootUpdateTest_PrintResult("ABORT reset to IDLE", operation_result)) {
        ApBootUpdateTest_PrintContext("ABORT", &context);

        return false;
    }

    (void)ApBootConsole_SendString("\r\n"
                                   "[UPDATE TEST] ALL PASS\r\n"
                                   "[UPDATE TEST] Test image is now stored at "
                                   "0x08020000\r\n"
                                   "[UPDATE TEST] Restore Application.bin before "
                                   "normal reset\r\n"
                                   "\r\n");

    return true;
}