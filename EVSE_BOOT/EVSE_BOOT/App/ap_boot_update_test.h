#ifndef EVSE_BOOT_APP__AP_BOOT_UPDATE_TEST_H
#define EVSE_BOOT_APP__AP_BOOT_UPDATE_TEST_H

#include <stdbool.h>

// OTA Update 상태머신의 고정 데이터 시험 수행
// 주의: Application 시작 Sector 삭제, 테스트 이미지 기록
bool ApBootUpdateTest_Run(void);

#endif // EVSE_BOOT_APP__AP_BOOT_UPDATE_TEST_H