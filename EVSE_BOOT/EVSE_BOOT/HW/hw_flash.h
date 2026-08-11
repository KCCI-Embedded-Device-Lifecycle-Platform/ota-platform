#ifndef EVSE_BOOT_HW__HW_FLASH_H
#define EVSE_BOOT_HW__HW_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#include "HW/hw_common.h"

// defines
#define HW_FLASH_PROGRAM_WORD_SIZE 4U  // FLASH_VOLTAGE_RANGE_3, FLASH_TYPEPROGRAM_WORD 사용 >> 4바이트
#define HW_FLASH_NO_SECTOR_ERROR   0xFFFFFFFFUL
// HAL_FLASHEx_Erase()에서 모든 Sector가 정상적으로 지워졌을 때 사용하는 Sector Error 초기값

// tables
typedef enum {
    HW_FLASH_SECTOR_0 = 0U,
    HW_FLASH_SECTOR_1,
    HW_FLASH_SECTOR_2,
    HW_FLASH_SECTOR_3,
    HW_FLASH_SECTOR_4,
    HW_FLASH_SECTOR_5,
    HW_FLASH_SECTOR_6,
    HW_FLASH_SECTOR_7,
    HW_FLASH_SECTOR_8,
    HW_FLASH_SECTOR_9,
    HW_FLASH_SECTOR_10,
    HW_FLASH_SECTOR_11,
    HW_FLASH_SECTOR_12,
    HW_FLASH_SECTOR_13,
    HW_FLASH_SECTOR_14,
    HW_FLASH_SECTOR_15,
    HW_FLASH_SECTOR_16,
    HW_FLASH_SECTOR_17,
    HW_FLASH_SECTOR_18,
    HW_FLASH_SECTOR_19,
    HW_FLASH_SECTOR_20,
    HW_FLASH_SECTOR_21,
    HW_FLASH_SECTOR_22,
    HW_FLASH_SECTOR_23
} hw_flash_sector_t;

typedef struct {
    // Sector Erase 실패 시 문제 발생 Sector
    // HW_FLASH_NO_SECTOR_ERROR: 특정 Sector 오류가 없거나 모든 Sector 정상 처리
    uint32_t sector;

    // HAL_FLASH_GetError() 결과
    uint32_t hal_error;
} hw_flash_error_info_t;

// functions
hw_status_t HwFlash_Unlock(void);
hw_status_t HwFlash_Lock(void);

hw_status_t HwFlash_EraseSectors(uint32_t first_sector, uint32_t sector_count, hw_flash_error_info_t *error_info);

hw_status_t HwFlash_ProgramWord(uint32_t address, uint32_t data);

uint32_t HwFlash_ReadWord(uint32_t address);

bool HwFlash_IsWordAligned(uint32_t address);
bool HwFlash_IsErasedWord(uint32_t address);

uint32_t HwFlash_GetHalError(void);

#endif // EVSE_BOOT_HW__HW_FLASH_H