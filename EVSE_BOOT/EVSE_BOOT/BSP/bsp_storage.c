#include "bsp_storage.h"

#include <stddef.h>
#include <string.h>

#include "HW/hw_flash.h"
#include "Common/boot_config.h"

// tables
typedef struct {
    hw_flash_sector_t sector;
    uint32_t start_address;
    uint32_t end_address;
} bsp_storage_sector_info_t;

// inners
static const bsp_storage_sector_info_t s_application_sectors[] = {
    // STM32F429ZI Application 영역
    {HW_FLASH_SECTOR_5,  0x08020000UL, 0x08040000UL},
    {HW_FLASH_SECTOR_6,  0x08040000UL, 0x08060000UL},
    {HW_FLASH_SECTOR_7,  0x08060000UL, 0x08080000UL},
    {HW_FLASH_SECTOR_8,  0x08080000UL, 0x080A0000UL},
    {HW_FLASH_SECTOR_9,  0x080A0000UL, 0x080C0000UL},
    {HW_FLASH_SECTOR_10, 0x080C0000UL, 0x080E0000UL},
    {HW_FLASH_SECTOR_11, 0x080E0000UL, 0x08100000UL}
};

#define BSP_STORAGE_APPLICATION_SECTOR_COUNT (sizeof(s_application_sectors) / sizeof(s_application_sectors[0]))

static bool s_initialized;

static void BspStorage_ResetErrorInfo(bsp_storage_error_info_t *error_info) {
    if (error_info == NULL) return;

    memset(error_info, 0, sizeof(*error_info));

    error_info->status = BSP_STORAGE_STATUS_OK;
    error_info->sector = BSP_STORAGE_NO_SECTOR_ERROR;
}

static bsp_storage_status_t BspStorage_SetError(bsp_storage_error_info_t *error_info, bsp_storage_status_t status, uint32_t address, uint32_t sector, uint32_t hal_error, uint32_t expected, uint32_t actual) {
    if (error_info != NULL) {
        error_info->status = status;
        error_info->address = address;
        error_info->sector = sector;
        error_info->hal_error = hal_error;
        error_info->expected = expected;
        error_info->actual = actual;
    }

    return status;
}

static bool \
BspStorage_GetSectorByAddress(uint32_t address, hw_flash_sector_t *sector, uint32_t *sector_end_address) {
    uint32_t index;

    if ((sector == NULL) || (sector_end_address == NULL)) return false;

    for (index = 0U; index < BSP_STORAGE_APPLICATION_SECTOR_COUNT; index++) {
        if ((address >= s_application_sectors[index].start_address) && (address < s_application_sectors[index].end_address)) {
            *sector = s_application_sectors[index].sector;
            *sector_end_address = s_application_sectors[index].end_address;

            return true;
        }
    }

    return false;
}

static uint32_t BspStorage_CreateProgramWord(const uint8_t *data, uint32_t length) {
    uint32_t index;
    uint32_t shift;
    uint32_t word;

    word = 0xFFFFFFFFUL;

    for (index = 0U; index < length; index++) {
        shift = index * 8U;

        word &= ~(0xFFUL << shift);
        word |= (uint32_t)data[index] << shift;
    }

    return word;
}

static bsp_storage_status_t \
BspStorage_LockAfterOperation(bsp_storage_status_t operation_status, bsp_storage_error_info_t *error_info, uint32_t operation_address) {
    hw_status_t lock_status;

    lock_status = HwFlash_Lock();

    if ((operation_status == BSP_STORAGE_STATUS_OK) && (lock_status != HW_STATUS_OK)) {
        return BspStorage_SetError(error_info,
                                   BSP_STORAGE_STATUS_FLASH_ERROR,
                                   operation_address,
                                   BSP_STORAGE_NO_SECTOR_ERROR,
                                   HwFlash_GetHalError(),
                                   0U,
                                   0U);
    }

    return operation_status;
}

// functions
bsp_storage_status_t BspStorage_Init(void) {
    // Application Flash 영역 설정을 검사하고 Storage BSP를 초기화
    s_initialized = false;

    // BSP Sector Table, Bootloader 설정값 일치 확인
    if (BOOT_APP_START_ADDRESS != s_application_sectors[0].start_address)
        return BSP_STORAGE_STATUS_ERROR;

    if (BOOT_APP_END_ADDRESS != s_application_sectors[BSP_STORAGE_APPLICATION_SECTOR_COUNT - 1U].end_address)
        return BSP_STORAGE_STATUS_ERROR;

    if ((BOOT_APP_START_ADDRESS & (BSP_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U)
        return BSP_STORAGE_STATUS_ALIGNMENT_ERROR;

    s_initialized = true;

    return BSP_STORAGE_STATUS_OK;
}

bool BspStorage_IsInitialized(void) {
    // Storage 초기화 여부 반환
    return s_initialized;
}

uint32_t BspStorage_GetApplicationStartAddress(void) {
    // Application 저장 영역 시작 주소 반환
    return BOOT_APP_START_ADDRESS;
}

uint32_t BspStorage_GetApplicationCapacity(void) {
    // Application 저장 영역 최대 크기 반환
    return BOOT_APP_END_ADDRESS - BOOT_APP_START_ADDRESS;
}

bool BspStorage_IsImageSizeValid(uint32_t image_size) {
    // Firmware 이미지 크기가 저장 가능한 범위인지 확인
    if (image_size == 0U) return false;

    return image_size <= BspStorage_GetApplicationCapacity();
}

bool BspStorage_IsRangeValid(uint32_t offset, uint32_t length) {
    // Application 시작 주소 기준 Offset, Length 유효 저장 범위인지 확인
    uint32_t capacity;

    if (length == 0U) return false;

    capacity = BspStorage_GetApplicationCapacity();

    if (offset >= capacity) return false;

    // offset + length > Overflow 피하기 위해 뺄셈 형태로 검사
    if (length > (capacity - offset)) return false;

    return true;
}

bool BspStorage_IsErased(uint32_t offset, uint32_t length) {
    // 지정된 Application 영역이 Erase 상태인지 확인
    uint32_t address;
    uint32_t end_address;

    if ((!s_initialized) || (!BspStorage_IsRangeValid(offset, length))) return false;

    address = BOOT_APP_START_ADDRESS + offset;

    end_address = address + length;

    // 마지막 데이터 4바이트 미만이어도 해당 Word 전체 Erase 확인
    while (address < end_address) {
        if (!HwFlash_IsErasedWord(address)) return false;

        address += BSP_STORAGE_PROGRAM_UNIT_SIZE;
    }

    return true;
}

bsp_storage_status_t BspStorage_EraseApplication(uint32_t image_size, bsp_storage_error_info_t *error_info) {
    // Firmware 이미지 크기 기준 필요한 Sector만 Erase
    uint32_t last_image_address;
    hw_flash_sector_t last_sector;
    uint32_t last_sector_end_address;
    uint32_t sector_count;

    hw_status_t hw_status;
    hw_flash_error_info_t hw_error_info;

    bsp_storage_status_t operation_status;

    BspStorage_ResetErrorInfo(error_info);

    if (!s_initialized) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_NOT_INITIALIZED, 0U, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0U, 0U);
    }

    if (!BspStorage_IsImageSizeValid(image_size)) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_OUT_OF_RANGE, BOOT_APP_START_ADDRESS, BSP_STORAGE_NO_SECTOR_ERROR, 0U, image_size, BspStorage_GetApplicationCapacity());
    }

    last_image_address = BOOT_APP_START_ADDRESS + image_size - 1U;

    if (!BspStorage_GetSectorByAddress(last_image_address, &last_sector, &last_sector_end_address)) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_OUT_OF_RANGE, last_image_address, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0U, 0U);
    }

    (void)last_sector_end_address;

    // Application 첫 Sector 항상 Sector 5
    sector_count = (uint32_t)last_sector - (uint32_t)HW_FLASH_SECTOR_5 + 1U;

    hw_status = HwFlash_Unlock();

    if (hw_status != HW_STATUS_OK) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_FLASH_ERROR, BOOT_APP_START_ADDRESS, BSP_STORAGE_NO_SECTOR_ERROR, HwFlash_GetHalError(), 0U, 0U);
    }

    hw_status = HwFlash_EraseSectors(HW_FLASH_SECTOR_5, sector_count, &hw_error_info);

    if (hw_status != HW_STATUS_OK) {
        operation_status = BspStorage_SetError(error_info, BSP_STORAGE_STATUS_FLASH_ERROR, BOOT_APP_START_ADDRESS, hw_error_info.sector, hw_error_info.hal_error, 0U, 0U);

        return BspStorage_LockAfterOperation(operation_status, error_info, BOOT_APP_START_ADDRESS);
    }

    operation_status = BspStorage_LockAfterOperation(BSP_STORAGE_STATUS_OK, error_info, BOOT_APP_START_ADDRESS);

    if (operation_status != BSP_STORAGE_STATUS_OK) return operation_status;

    // 실제 기록할 이미지 범위 Erase 확인
    if (!BspStorage_IsErased(0U, image_size)) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_VERIFY_ERROR, BOOT_APP_START_ADDRESS, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0xFFFFFFFFUL, 0U);
    }

    return BSP_STORAGE_STATUS_OK;
}

bsp_storage_status_t BspStorage_Write(uint32_t offset, const uint8_t *data, uint32_t length, bool final_chunk, bsp_storage_error_info_t *error_info) {
    // Application 시작 주소 기준 Offset에 데이터 기록
    // offset:      BOOT_APP_START_ADDRESS 기준 Offset
    // final_chunk: true >> 마지막 4바이트 미만 데이터 0xFF로 Padding 기록
    
    // final_chunk false >> length 반드시 4바이트 배수
    uint32_t absolute_address;
    uint32_t source_index;
    uint32_t bytes_to_program;
    uint32_t program_word;
    uint32_t readback_word;

    hw_status_t hw_status;

    bsp_storage_status_t operation_status;

    BspStorage_ResetErrorInfo(error_info);

    if (!s_initialized) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_NOT_INITIALIZED, 0U, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0U, 0U);
    }

    if ((data == NULL) || (length == 0U)) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_INVALID_ARGUMENT, 0U, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0U, 0U);
    }

    if (!BspStorage_IsRangeValid(offset, length)) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_OUT_OF_RANGE, BOOT_APP_START_ADDRESS + offset, BSP_STORAGE_NO_SECTOR_ERROR, 0U, length, BspStorage_GetApplicationCapacity());
    }

    if ((offset & (BSP_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_ALIGNMENT_ERROR, BOOT_APP_START_ADDRESS + offset, BSP_STORAGE_NO_SECTOR_ERROR, 0U, BSP_STORAGE_PROGRAM_UNIT_SIZE, offset);
    }

    if ((!final_chunk) && ((length & (BSP_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U)) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_ALIGNMENT_ERROR, BOOT_APP_START_ADDRESS + offset, BSP_STORAGE_NO_SECTOR_ERROR, 0U, BSP_STORAGE_PROGRAM_UNIT_SIZE, length);
    }

    if (!BspStorage_IsErased(offset, length)) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_NOT_ERASED, BOOT_APP_START_ADDRESS + offset, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0xFFFFFFFFUL, 0U);
    }

    absolute_address = BOOT_APP_START_ADDRESS + offset;

    source_index = 0U;

    hw_status = HwFlash_Unlock();

    if (hw_status != HW_STATUS_OK) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_FLASH_ERROR, absolute_address, BSP_STORAGE_NO_SECTOR_ERROR, HwFlash_GetHalError(), 0U, 0U);
    }

    operation_status = BSP_STORAGE_STATUS_OK;

    while (source_index < length) {
        bytes_to_program = length - source_index;

        if (bytes_to_program > BSP_STORAGE_PROGRAM_UNIT_SIZE) bytes_to_program = BSP_STORAGE_PROGRAM_UNIT_SIZE;

        program_word = BspStorage_CreateProgramWord(&data[source_index], bytes_to_program);

        hw_status = HwFlash_ProgramWord(absolute_address, program_word);

        if (hw_status != HW_STATUS_OK) {
            operation_status = BspStorage_SetError(error_info, BSP_STORAGE_STATUS_FLASH_ERROR, absolute_address, BSP_STORAGE_NO_SECTOR_ERROR, HwFlash_GetHalError(), program_word, 0U);

            break;
        }

        readback_word = HwFlash_ReadWord(absolute_address);

        if (readback_word != program_word) {
            operation_status = BspStorage_SetError(error_info, BSP_STORAGE_STATUS_VERIFY_ERROR, absolute_address, BSP_STORAGE_NO_SECTOR_ERROR, HwFlash_GetHalError(), program_word, readback_word);

            break;
        }

        absolute_address += BSP_STORAGE_PROGRAM_UNIT_SIZE;

        source_index += bytes_to_program;
    }

    return BspStorage_LockAfterOperation(operation_status, error_info, absolute_address);
}

bsp_storage_status_t BspStorage_Verify(uint32_t offset, const uint8_t *data, uint32_t length, bsp_storage_error_info_t *error_info) {
    // Flash에 기록된 데이터와 입력 Buffer 비교
    uint32_t absolute_address;
    uint32_t source_index;
    uint32_t bytes_to_compare;
    uint32_t expected_word;
    uint32_t actual_word;
    uint32_t byte_index;
    uint32_t byte_mask;

    BspStorage_ResetErrorInfo(error_info);

    if (!s_initialized) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_NOT_INITIALIZED, 0U, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0U, 0U);
    }

    if ((data == NULL) || (!BspStorage_IsRangeValid(offset, length))) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_INVALID_ARGUMENT, BOOT_APP_START_ADDRESS + offset, BSP_STORAGE_NO_SECTOR_ERROR, 0U, 0U, 0U);
    }

    if ((offset & (BSP_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U) {
        return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_ALIGNMENT_ERROR, BOOT_APP_START_ADDRESS + offset, BSP_STORAGE_NO_SECTOR_ERROR, 0U, BSP_STORAGE_PROGRAM_UNIT_SIZE, offset);
    }

    absolute_address = BOOT_APP_START_ADDRESS + offset;

    source_index = 0U;

    while (source_index < length) {
        bytes_to_compare = length - source_index;

        if (bytes_to_compare > BSP_STORAGE_PROGRAM_UNIT_SIZE) bytes_to_compare = BSP_STORAGE_PROGRAM_UNIT_SIZE;

        expected_word = BspStorage_CreateProgramWord(&data[source_index], bytes_to_compare);

        actual_word = HwFlash_ReadWord(absolute_address);

        // 마지막 Word의 Padding 영역 비교하지 않음
        for (byte_index = 0U; byte_index < bytes_to_compare; byte_index++) {
            byte_mask = 0xFFUL << (byte_index * 8U);

            if ((expected_word & byte_mask) != (actual_word & byte_mask)) {
                return BspStorage_SetError(error_info, BSP_STORAGE_STATUS_VERIFY_ERROR, absolute_address + byte_index, BSP_STORAGE_NO_SECTOR_ERROR, 0U, expected_word, actual_word);
            }
        }

        absolute_address += BSP_STORAGE_PROGRAM_UNIT_SIZE;

        source_index += bytes_to_compare;
    }

    return BSP_STORAGE_STATUS_OK;
}
