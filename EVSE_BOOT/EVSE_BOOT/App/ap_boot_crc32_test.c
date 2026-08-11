#include "App/ap_boot_crc32_test.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "App/ap_boot_console.h"
#include "Middleware/boot_crc32.h"

#define AP_BOOT_CRC32_TEST_EXPECTED_VALUE       0xCBF43926UL
#define AP_BOOT_CRC32_TEST_EMPTY_EXPECTED_VALUE 0x00000000UL

#define AP_BOOT_CRC32_TEST_LOG_BUFFER_SIZE      128U

static const uint8_t s_crc32_test_data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

static void ApBootCrc32Test_PrintValue(const char *test_name, uint32_t expected, uint32_t actual, bool passed) {
    char log_buffer[AP_BOOT_CRC32_TEST_LOG_BUFFER_SIZE];

    int written_length;

    if (test_name == NULL) return;

    written_length = snprintf(log_buffer,
                              sizeof(log_buffer),
                              "[CRC32 TEST] %s\r\n"
                              "  expected: 0x%08lX\r\n"
                              "  actual  : 0x%08lX\r\n"
                              "  result  : %s\r\n",
                              test_name,
                              (unsigned long)expected,
                              (unsigned long)actual,
                              passed ? "PASS" : "FAIL");

    if ((written_length > 0) && ((size_t)written_length < sizeof(log_buffer)))
        (void)ApBootConsole_SendString(log_buffer);
    else (void)ApBootConsole_SendString("[CRC32 TEST] Log buffer overflow\r\n");
}

static bool ApBootCrc32Test_RunOneShot(void) {
    uint32_t calculated_crc;
    mw_status_t middleware_status;
    bool passed;

    calculated_crc = 0U;

    middleware_status = BootCrc32_Calculate(s_crc32_test_data, sizeof(s_crc32_test_data), &calculated_crc);

    passed = (middleware_status == MW_STATUS_OK) && (calculated_crc == AP_BOOT_CRC32_TEST_EXPECTED_VALUE);

    ApBootCrc32Test_PrintValue("One-shot calculation", AP_BOOT_CRC32_TEST_EXPECTED_VALUE, calculated_crc, passed);

    return passed;
}

static bool ApBootCrc32Test_RunStreaming(void) {
    uint32_t running_crc;
    uint32_t calculated_crc;

    mw_status_t middleware_status;

    bool passed;

    running_crc = BootCrc32_Begin();

    // "123"
    middleware_status = BootCrc32_Update(&running_crc, &s_crc32_test_data[0], 3U);

    if (middleware_status != MW_STATUS_OK) {
        (void)ApBootConsole_SendString("[CRC32 TEST] Streaming block 1 FAIL\r\n");

        return false;
    }

    // "45"
    middleware_status = BootCrc32_Update(&running_crc, &s_crc32_test_data[3], 2U);

    if (middleware_status != MW_STATUS_OK) {
        (void)ApBootConsole_SendString("[CRC32 TEST] Streaming block 2 FAIL\r\n");

        return false;
    }

    // "6789"
    middleware_status = BootCrc32_Update(&running_crc, &s_crc32_test_data[5], 4U);

    if (middleware_status != MW_STATUS_OK) {
        (void)ApBootConsole_SendString("[CRC32 TEST] Streaming block 3 FAIL\r\n");

        return false;
    }

    calculated_crc = BootCrc32_End(running_crc);

    passed = calculated_crc == AP_BOOT_CRC32_TEST_EXPECTED_VALUE;

    ApBootCrc32Test_PrintValue("Streaming calculation", AP_BOOT_CRC32_TEST_EXPECTED_VALUE, calculated_crc, passed);

    return passed;
}

static bool ApBootCrc32Test_RunEmptyData(void) {
    uint32_t calculated_crc;
    mw_status_t middleware_status;
    bool passed;

    calculated_crc = 0xFFFFFFFFUL;

    // length가 0이면 data는 NULL이어도 정상 입력으로 처리합니다.
    middleware_status = BootCrc32_Calculate(NULL, 0U, &calculated_crc);

    passed = (middleware_status == MW_STATUS_OK) && (calculated_crc == AP_BOOT_CRC32_TEST_EMPTY_EXPECTED_VALUE);

    ApBootCrc32Test_PrintValue("Empty data calculation",
                               AP_BOOT_CRC32_TEST_EMPTY_EXPECTED_VALUE,
                               calculated_crc,
                               passed);

    return passed;
}

static bool ApBootCrc32Test_RunInvalidArguments(void) {
    uint32_t calculated_crc;

    mw_status_t null_data_status;
    mw_status_t null_result_status;
    mw_status_t null_crc_status;

    bool passed;

    calculated_crc = 0U;

    // NULL data와 1바이트 길이는 잘못된 입력
    null_data_status = BootCrc32_Calculate(NULL, 1U, &calculated_crc);
    // 결과 저장 포인터가 NULL이면 잘못된 입력
    null_result_status = BootCrc32_Calculate(s_crc32_test_data, sizeof(s_crc32_test_data), NULL);
    // Running CRC 포인터가 NULL이면 잘못된 입력
    null_crc_status = BootCrc32_Update(NULL, s_crc32_test_data, sizeof(s_crc32_test_data));

    passed = (null_data_status == MW_STATUS_INVALID_ARGUMENT) && (null_result_status == MW_STATUS_INVALID_ARGUMENT) && (null_crc_status == MW_STATUS_INVALID_ARGUMENT);

    if (passed) {
        (void)ApBootConsole_SendString("[CRC32 TEST] Invalid arguments: PASS\r\n");
    }
    else {
        char log_buffer[AP_BOOT_CRC32_TEST_LOG_BUFFER_SIZE];

        int written_length;

        written_length = snprintf(log_buffer,
                                  sizeof(log_buffer),
                                  "[CRC32 TEST] Invalid arguments: FAIL\r\n"
                                  "  null data  : %u\r\n"
                                  "  null result: %u\r\n"
                                  "  null crc   : %u\r\n",
                                  (unsigned int)null_data_status,
                                  (unsigned int)null_result_status,
                                  (unsigned int)null_crc_status);

        if ((written_length > 0) && ((size_t)written_length < sizeof(log_buffer)))
            (void)ApBootConsole_SendString(log_buffer);
    }

    return passed;
}

bool ApBootCrc32Test_Run(void) {
    bool one_shot_passed;
    bool streaming_passed;
    bool empty_data_passed;
    bool invalid_arguments_passed;

    bool all_passed;

    (void)ApBootConsole_SendString("\r\n"
                                   "[CRC32 TEST] Start\r\n"
                                   "[CRC32 TEST] Input: 123456789\r\n"
                                   "[CRC32 TEST] Expected: 0xCBF43926\r\n");

    one_shot_passed = ApBootCrc32Test_RunOneShot();
    streaming_passed = ApBootCrc32Test_RunStreaming();
    empty_data_passed = ApBootCrc32Test_RunEmptyData();
    invalid_arguments_passed = ApBootCrc32Test_RunInvalidArguments();

    all_passed = one_shot_passed && streaming_passed && empty_data_passed && invalid_arguments_passed;

    if (all_passed) {
        (void)ApBootConsole_SendString("[CRC32 TEST] ALL PASS\r\n" "\r\n");
    }
    else {
        (void)ApBootConsole_SendString("[CRC32 TEST] FAILED\r\n" "\r\n");
    }

    return all_passed;
}