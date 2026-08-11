#ifndef EVSE_BOOT_BSP__BSP_STORAGE_H
#define EVSE_BOOT_BSP__BSP_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

// defines
#define BSP_STORAGE_PROGRAM_UNIT_SIZE    4U

#define BSP_STORAGE_NO_SECTOR_ERROR      0xFFFFFFFFUL

// tables
typedef enum {
    BSP_STORAGE_STATUS_OK = 0,

    BSP_STORAGE_STATUS_ERROR,
    BSP_STORAGE_STATUS_INVALID_ARGUMENT,
    BSP_STORAGE_STATUS_NOT_INITIALIZED,

    BSP_STORAGE_STATUS_OUT_OF_RANGE,
    BSP_STORAGE_STATUS_ALIGNMENT_ERROR,
    BSP_STORAGE_STATUS_NOT_ERASED,

    BSP_STORAGE_STATUS_FLASH_ERROR,
    BSP_STORAGE_STATUS_VERIFY_ERROR
} bsp_storage_status_t;

typedef struct {
    bsp_storage_status_t status;
    uint32_t address; // 오류가 발생한 Flash 절대 주소
    uint32_t sector;  // Erase 오류가 발생한 Sector
    uint32_t hal_error; // HAL_FLASH_GetError() 결과
    uint32_t expected; // Read-back 검증 실패 시 사용
    uint32_t actual;
} bsp_storage_error_info_t;

// functions
bsp_storage_status_t BspStorage_Init(void);

bool BspStorage_IsInitialized(void);

uint32_t BspStorage_GetApplicationStartAddress(void);
uint32_t BspStorage_GetApplicationCapacity(void);

bool BspStorage_IsImageSizeValid(uint32_t image_size);

bool BspStorage_IsRangeValid(uint32_t offset, uint32_t length);

bsp_storage_status_t BspStorage_EraseApplication(uint32_t image_size, bsp_storage_error_info_t *error_info);

bsp_storage_status_t BspStorage_Write(uint32_t offset, const uint8_t *data, uint32_t length, bool final_chunk,bsp_storage_error_info_t *error_info);
bsp_storage_status_t BspStorage_Verify(uint32_t offset, const uint8_t *data, uint32_t length,bsp_storage_error_info_t *error_info);

bool BspStorage_IsErased(uint32_t offset, uint32_t length);

#endif // EVSE_BOOT_BSP__BSP_STORAGE_H