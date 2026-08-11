#ifndef EVSE_BOOT_AP__AP_BOOT_STORAGE_TEST_H
#define EVSE_BOOT_AP__AP_BOOT_STORAGE_TEST_H

#include <stdbool.h>

// Application 시작 영역 Erase, 테스트 패턴 기록, Read-back 검증
// 실행 후 기존 Application은 삭제됨
bool ApBootStorageTest_Run(void);

#endif // EVSE_BOOT_AP__AP_BOOT_STORAGE_TEST_H