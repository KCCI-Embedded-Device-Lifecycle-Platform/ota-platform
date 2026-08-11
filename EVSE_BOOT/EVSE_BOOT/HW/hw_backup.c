#include "hw_backup.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "main.h"

#include "Common/boot_config.h"

// inners
static bool s_initialized;

static bool HwBackup_IsOffsetValid(uint32_t offset) {
    // Backup SRAM에는 32비트 Word 단위로 접근
    if ((offset & (sizeof(uint32_t) - 1U)) != 0U) return false;

    // offset부터 4바이트를 읽거나 쓸 수 있어야 
    if (offset > (BOOT_BACKUP_SRAM_SIZE - sizeof(uint32_t))) return false;

    return true;
}

static volatile uint32_t *HwBackup_GetWordAddress(uint32_t offset) {
    uintptr_t absolute_address;

    absolute_address = (uintptr_t)BOOT_BACKUP_SRAM_START_ADDRESS + (uintptr_t)offset;

    return (volatile uint32_t *)absolute_address;
}

// functions
hw_status_t HwBackup_Init(void) {
    // Backup SRAM 접근에 필요한 순서:
    // 1. PWR Peripheral Clock 활성화
    // 2. Backup Domain 쓰기 접근 허용
    // 3. Backup SRAM Clock 활성화
    if (s_initialized) return HW_STATUS_OK;

    __HAL_RCC_PWR_CLK_ENABLE();

    HAL_PWR_EnableBkUpAccess();

    // DBP가 실제로 설정됐는지 확인
    if ((PWR->CR & PWR_CR_DBP) == 0U) return HW_STATUS_ERROR;

    __HAL_RCC_BKPSRAM_CLK_ENABLE();

    // Clock 및 접근 권한 설정이 이후 메모리 접근보다 먼저 완료되도록 Barrier 적용
    __DSB();
    __ISB();

    s_initialized = true;

    return HW_STATUS_OK;
}

hw_status_t HwBackup_ReadWord(uint32_t offset, uint32_t *value) {
    volatile uint32_t *word_address;

    if (value == NULL) return HW_STATUS_INVALID_ARGUMENT;

    *value = 0U;

    if (!s_initialized) return HW_STATUS_NOT_INITIALIZED;

    if (!HwBackup_IsOffsetValid(offset)) return HW_STATUS_INVALID_ARGUMENT;

    word_address = HwBackup_GetWordAddress(offset);

    __DMB();

    *value = *word_address;

    __DMB();

    return HW_STATUS_OK;
}

hw_status_t HwBackup_WriteWord(uint32_t offset, uint32_t value) {
    volatile uint32_t *word_address;

    if (!s_initialized) return HW_STATUS_NOT_INITIALIZED;

    if (!HwBackup_IsOffsetValid(offset)) return HW_STATUS_INVALID_ARGUMENT;

    word_address = HwBackup_GetWordAddress(offset);

    *word_address = value;

    // Reset 직전에도 Backup SRAM 쓰기가 완료되도록 Store 완료 보장
    __DSB();

    // 쓰기 후 Read-back으로 실제 저장 여부 확인
    if (*word_address != value) return HW_STATUS_ERROR;

    return HW_STATUS_OK;
}