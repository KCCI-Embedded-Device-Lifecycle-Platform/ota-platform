#include "App/ap_boot_storage_test.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "App/ap_boot_console.h"
#include "BSP/bsp_storage.h"

// defines
#define AP_BOOT_STORAGE_TEST_LOG_SIZE    192U

// inners

// 첫 번째 Word: 0x44332211
// 유효한 SRAM Stack Pointer가 아니므로 시험 후 Bootloader가 이 영역 Application으로 실행x
static const uint8_t s_test_data[] = {
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

static void ApBootStorageTest_PrintError(const char *stage, const bsp_storage_error_info_t *error_info) {
    char log_buffer[AP_BOOT_STORAGE_TEST_LOG_SIZE];

    int written_length;

    if ((stage == NULL) || (error_info == NULL)) return;

    written_length = snprintf(log_buffer,
                              sizeof(log_buffer),
                              "[FLASH TEST] %s FAIL\r\n"
                              "  status   : %u\r\n"
                              "  address  : 0x%08" PRIX32 "\r\n"
                              "  sector   : 0x%08" PRIX32 "\r\n"
                              "  hal_error: 0x%08" PRIX32 "\r\n"
                              "  expected : 0x%08" PRIX32 "\r\n"
                              "  actual   : 0x%08" PRIX32 "\r\n",
                              stage,
                              (unsigned int)error_info->status,
                              error_info->address,
                              error_info->sector,
                              error_info->hal_error,
                              error_info->expected,
                              error_info->actual);

    if ((written_length > 0) && ((size_t)written_length < sizeof(log_buffer)))
        (void)ApBootConsole_SendString(log_buffer);
    else (void)ApBootConsole_SendString("[FLASH TEST] Error log overflow\r\n");
}

// functions
bool ApBootStorageTest_Run(void) {
    bsp_storage_status_t storage_status;
    bsp_storage_error_info_t error_info;

    (void)ApBootConsole_SendString("\r\n"
                                   "[FLASH TEST] Start\r\n"
                                   "[FLASH TEST] WARNING: "
                                   "Application Sector will be erased\r\n");

    if (!BspStorage_IsInitialized()) {
        (void)ApBootConsole_SendString("[FLASH TEST] Storage not initialized\r\n");

        return false;
    }

    if (!BspStorage_IsImageSizeValid(sizeof(s_test_data))) {
        (void)ApBootConsole_SendString("[FLASH TEST] Invalid test size\r\n");

        return false;
    }

    // sizeof(s_test_data) 16바이트 >> but Flash는 Sector 단위로 지워지므로 Sector 5 전체 삭제
    storage_status = BspStorage_EraseApplication(sizeof(s_test_data), &error_info);

    if (storage_status != BSP_STORAGE_STATUS_OK) {
        ApBootStorageTest_PrintError("ERASE", &error_info);

        return false;
    }

    (void)ApBootConsole_SendString("[FLASH TEST] Erase OK\r\n");

    if (!BspStorage_IsErased(0U, sizeof(s_test_data))) {
        (void)ApBootConsole_SendString("[FLASH TEST] Erase verification FAIL\r\n");

        return false;
    }

    (void)ApBootConsole_SendString("[FLASH TEST] Erase verification OK\r\n");

    storage_status = BspStorage_Write(0U, s_test_data, sizeof(s_test_data), true, &error_info);

    if (storage_status != BSP_STORAGE_STATUS_OK) {
        ApBootStorageTest_PrintError("WRITE", &error_info);

        return false;
    }

    (void)ApBootConsole_SendString("[FLASH TEST] Write OK\r\n");

    storage_status = BspStorage_Verify(0U, s_test_data, sizeof(s_test_data), &error_info);

    if (storage_status != BSP_STORAGE_STATUS_OK) {
        ApBootStorageTest_PrintError("VERIFY", &error_info);

        return false;
    }

    (void)ApBootConsole_SendString("[FLASH TEST] Verify OK\r\n"
                                   "[FLASH TEST] PASS\r\n"
                                   "[FLASH TEST] Application is no longer valid\r\n"
                                   "[FLASH TEST] Reflash Application.bin "
                                   "at 0x08020000\r\n"
                                   "\r\n");

    return true;
}