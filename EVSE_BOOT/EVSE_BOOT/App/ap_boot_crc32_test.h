#ifndef EVSE_BOOT_APP__AP_BOOT_CRC32_TEST_H
#define EVSE_BOOT_APP__AP_BOOT_CRC32_TEST_H

#include <stdbool.h>

// CRC32 일괄 계산, 스트리밍 계산, 빈 데이터 및 잘못된 인자 시험 수행
// Flash 접근x
bool ApBootCrc32Test_Run(void);

#endif // EVSE_BOOT_APP__AP_BOOT_CRC32_TEST_H