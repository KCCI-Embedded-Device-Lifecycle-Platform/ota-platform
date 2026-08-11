#include "hw_flash.h"

#include <stddef.h>

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"

// inners
static hw_status_t HwFlash_ConvertHalStatus(HAL_StatusTypeDef hal_status) {
    switch (hal_status) {
        case HAL_OK:        return HW_STATUS_OK;
        case HAL_TIMEOUT:   return HW_STATUS_TIMEOUT;
        case HAL_BUSY:      return HW_STATUS_BUSY;
        case HAL_ERROR:
        default:            return HW_STATUS_ERROR;
    }
}

// functions
hw_status_t HwFlash_Unlock(void) {
    // Flash 제어 레지스터 Lock 해제
    HAL_StatusTypeDef hal_status;

    hal_status = HAL_FLASH_Unlock();

    return HwFlash_ConvertHalStatus(hal_status);
}

hw_status_t HwFlash_Lock(void) {
    // Flash 제어 레지스터 다시 Lock
    HAL_StatusTypeDef hal_status;

    hal_status = HAL_FLASH_Lock();

    return HwFlash_ConvertHalStatus(hal_status);
}

hw_status_t HwFlash_EraseSectors(uint32_t first_sector, uint32_t sector_count, hw_flash_error_info_t *error_info) {
    // 지정된 Flash Sector Erase
    // bank:         FLASH_BANK_1 또는 FLASH_BANK_2
    // first_sector: FLASH_SECTOR_0 ~ FLASH_SECTOR_23
    // sector_count: 연속해서 지울 Sector 수 

    // Flash Unlock 상태에서 호출
    HAL_StatusTypeDef hal_status;
    FLASH_EraseInitTypeDef erase_config = {0};

    uint32_t sector_error;

    if ((sector_count == 0U) || (error_info == NULL)) return HW_STATUS_INVALID_ARGUMENT;

    sector_error = HW_FLASH_NO_SECTOR_ERROR;

    error_info->sector = HW_FLASH_NO_SECTOR_ERROR;
    error_info->hal_error = 0U;

    erase_config.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_config.Banks = FLASH_BANK_BOTH;   // Sector Erase에서는 Banks가 사용되지 않지만 구조체 필드를 명시적으로 초기화
    erase_config.Sector = (uint32_t)first_sector;
    erase_config.NbSectors = sector_count;
    erase_config.VoltageRange = FLASH_VOLTAGE_RANGE_3;  // F429ZI 3.3V 동작 환경에서 2.7V~3.6V용 Word Parallelism 사용

    hal_status = HAL_FLASHEx_Erase(&erase_config, &sector_error);

    error_info->sector = sector_error;
    error_info->hal_error = HAL_FLASH_GetError();

    return HwFlash_ConvertHalStatus(hal_status);
}

hw_status_t HwFlash_ProgramWord(uint32_t address, uint32_t data) {
    // 지정 주소에 32비트 Word 하나 기록
    // address 반드시 4바이트 정렬 Flash Unlock 상태에서 호출
    HAL_StatusTypeDef hal_status;

    if (!HwFlash_IsWordAligned(address)) return HW_STATUS_INVALID_ARGUMENT;

    hal_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, (uint64_t)data);

    return HwFlash_ConvertHalStatus(hal_status);
}

uint32_t HwFlash_ReadWord(uint32_t address) {
    // 지정 주소에서 32비트 Word 읽기
    return *(volatile const uint32_t *)address;
}

bool HwFlash_IsWordAligned(uint32_t address) {
    // 주소가 4바이트 정렬인지 확인
    return (address & (HW_FLASH_PROGRAM_WORD_SIZE - 1U)) == 0U;
}


bool HwFlash_IsErasedWord(uint32_t address) {
    // 지정 주소의 Word가 Erase 상태인지 확인
    // Erase 상태: 0xFFFFFFFF
    if (!HwFlash_IsWordAligned(address)) return false;

    return HwFlash_ReadWord(address) == 0xFFFFFFFFUL;
}


uint32_t HwFlash_GetHalError(void) {
    // 최근 HAL Flash 오류값 반환
    return HAL_FLASH_GetError();
}