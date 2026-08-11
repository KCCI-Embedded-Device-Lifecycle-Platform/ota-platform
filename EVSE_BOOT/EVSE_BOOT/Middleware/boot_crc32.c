#include "boot_crc32.h"

#include <stddef.h>

// inners
static uint32_t BootCrc32_UpdateByte(uint32_t crc, uint8_t data) {
    uint8_t bit_index;

    crc ^= (uint32_t)data;

    for (bit_index = 0U; bit_index < 8U; bit_index++) {
        if ((crc & 0x00000001UL) != 0UL) crc = (crc >> 1U) ^ BOOT_CRC32_REFLECTED_POLYNOMIAL;
        else                             crc >>= 1U;
    }

    return crc;
}

// functions
uint32_t BootCrc32_Begin(void) {
    // 스트리밍 CRC32 계산 시작할 때 사용할 초기값 반환
    return BOOT_CRC32_INITIAL_VALUE;
}

mw_status_t BootCrc32_Update(uint32_t *crc, const uint8_t *data, uint32_t length) {
    // 기존 Running CRC에 새로운 데이터를 이어서 반영
    // 주의: 이 함수의 crc 값은 아직 Final XOR가 적용되지 않은 내부 계산 상태
    uint32_t index;

    if (crc == NULL) return MW_STATUS_INVALID_ARGUMENT;

    if ((data == NULL) && (length > 0U)) return MW_STATUS_INVALID_ARGUMENT;

    for (index = 0U; index < length; index++) *crc = BootCrc32_UpdateByte(*crc, data[index]);

    return MW_STATUS_OK;
}

uint32_t BootCrc32_End(uint32_t crc) {
    // 누적된 Running CRC에 Final XOR 적용
    return crc ^ BOOT_CRC32_FINAL_XOR_VALUE;
}

mw_status_t BootCrc32_Calculate(const uint8_t *data, uint32_t length, uint32_t *result) {
    // 하나의 연속된 데이터 전체에 대한 CRC32 계산
    uint32_t crc;
    mw_status_t status;

    if (result == NULL)                  return MW_STATUS_INVALID_ARGUMENT;
    if ((data == NULL) && (length > 0U)) return MW_STATUS_INVALID_ARGUMENT;

    crc = BootCrc32_Begin();

    status = BootCrc32_Update(&crc, data, length);

    if (status != MW_STATUS_OK) return status;

    *result = BootCrc32_End(crc);

    return MW_STATUS_OK;
}
