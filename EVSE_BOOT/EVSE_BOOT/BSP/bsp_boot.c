#include "bsp_boot.h"

#include <stddef.h>

#include "main.h"

#include "HW/hw_gpio.h"
#include "HW/hw_backup.h"
#include "HW/hw_system.h"
#include "Common/boot_config.h"

// defines
    // B1 USER 버튼: 누른 상태에서 GPIO_PIN_SET
    // LD1:         GPIO_PIN_SET에서 점등
#define BSP_BOOT_BUTTON_ACTIVE_STATE    GPIO_PIN_SET
#define BSP_BOOT_LED_ON_STATE           GPIO_PIN_SET
#define BSP_BOOT_LED_OFF_STATE          GPIO_PIN_RESET

// inners
static hw_gpio_t s_user_button;
static hw_gpio_t s_status_led;

static bool s_initialized;

static bsp_status_t BspBoot_ConvertHwStatus(hw_status_t status) {
    switch (status) {
        case HW_STATUS_OK:               return BSP_STATUS_OK;
        case HW_STATUS_TIMEOUT:          return BSP_STATUS_TIMEOUT;
        case HW_STATUS_BUSY:             return BSP_STATUS_BUSY;
        case HW_STATUS_INVALID_ARGUMENT: return BSP_STATUS_INVALID_ARGUMENT;
        case HW_STATUS_NOT_INITIALIZED:  return BSP_STATUS_NOT_INITIALIZED;
        case HW_STATUS_ERROR:
        default:                         return BSP_STATUS_ERROR;
    }
}

// functions
bsp_status_t BspBoot_Init(void) {
    // Bootloader에서 사용할 USER 버튼과 상태 LED를 초기화합니다.
    hw_status_t hw_status;

    if (s_initialized) return BSP_STATUS_OK;

    hw_status = HwBackup_Init();

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    hw_status = HwGpio_Attach(&s_user_button, B1_GPIO_Port, B1_Pin);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    hw_status = HwGpio_Attach(&s_status_led, BL_LED_GPIO_Port, BL_LED_Pin);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    hw_status = HwGpio_Write(&s_status_led, BSP_BOOT_LED_OFF_STATE);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    s_initialized = true;

    return BSP_STATUS_OK;
}

bsp_status_t BspBoot_GetButtonRequest(bool *requested) {
    // USER 버튼으로 Bootloader 진입을 요청했는지 확인합니다.
    // true  → Bootloader 모드 진입 요청
    // false → 정상 Application 부팅 요청
    GPIO_PinState first_state;
    GPIO_PinState second_state;

    hw_status_t hw_status;

    if (requested == NULL) return BSP_STATUS_INVALID_ARGUMENT;

    *requested = false;

    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;

    hw_status = HwGpio_Read(&s_user_button, &first_state);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    // 단순한 부트 모드 선택용 디바운싱입니다.
    // Bootloader는 RTOS를 사용하지 않으므로 내부적으로 HAL_Delay()를 사용합니다.
    HwSystem_Delay(BOOT_BUTTON_DEBOUNCE_MS);

    hw_status = HwGpio_Read(&s_user_button, &second_state);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    *requested = (first_state == BSP_BOOT_BUTTON_ACTIVE_STATE) && (second_state == BSP_BOOT_BUTTON_ACTIVE_STATE);

    return BSP_STATUS_OK;
}

bsp_status_t BspBoot_GetRemoteUpdateRequest(bool *requested) {
    uint32_t request_magic;
    uint32_t request_inverse;

    hw_status_t hw_status;

    if (requested == NULL) return BSP_STATUS_INVALID_ARGUMENT;

    *requested = false;

    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;

    hw_status = HwBackup_ReadWord(BOOT_UPDATE_REQUEST_MAGIC_OFFSET, &request_magic);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    hw_status = HwBackup_ReadWord(BOOT_UPDATE_REQUEST_INVERSE_OFFSET, &request_inverse);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    // 두 값이 모두 일치해야 유효한 원격 요청으로 인정
    // App에서는 다음 순서로 기록
    // 1. Magic 기록
    // 2. Inverse Magic 기록
    // 두 번째 값을 Commit 값처럼 사용
    *requested = (request_magic == BOOT_UPDATE_REQUEST_MAGIC) && (request_inverse == BOOT_UPDATE_REQUEST_MAGIC_INVERSE);

    return BSP_STATUS_OK;
}

bsp_status_t BspBoot_ClearRemoteUpdateRequest(void) {
    hw_status_t hw_status;

    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;

    // 요청 무효화 값인 Inverse Magic부터 삭제
    // 첫 번째 쓰기 후 Reset >> 두 Magic 일치 x >> 원격 요청 유효 판정 x
    hw_status = HwBackup_WriteWord(BOOT_UPDATE_REQUEST_INVERSE_OFFSET, BOOT_UPDATE_REQUEST_CLEAR_VALUE);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    hw_status = HwBackup_WriteWord(BOOT_UPDATE_REQUEST_MAGIC_OFFSET, BOOT_UPDATE_REQUEST_CLEAR_VALUE);

    if (hw_status != HW_STATUS_OK) return BspBoot_ConvertHwStatus(hw_status);

    return BSP_STATUS_OK;
}

bool BspBoot_IsApplicationValid(void) {
    // Application Vector Table이 유효한지 검사
    return HwSystem_IsVectorTableValid(BOOT_APP_START_ADDRESS, BOOT_FLASH_STACK_TOP);
}

bsp_status_t BspBoot_SetStatusLed(bool enabled) {
    hw_status_t hw_status;

    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;

    hw_status = HwGpio_Write(&s_status_led, enabled ? BSP_BOOT_LED_ON_STATE : BSP_BOOT_LED_OFF_STATE);

    return BspBoot_ConvertHwStatus(hw_status);
}

bsp_status_t BspBoot_ToggleStatusLed(void) {
    hw_status_t hw_status;

    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;

    hw_status = HwGpio_Toggle(&s_status_led);

    return BspBoot_ConvertHwStatus(hw_status);
}

uint32_t BspBoot_GetTick(void) {
    // HAL Tick 값 반환
    return HwSystem_GetTick();
}   

void BspBoot_Delay(uint32_t delay_ms) {
    // 짧은 블로킹 지연에 사용합니다.
    HwSystem_Delay(delay_ms);
}

void BspBoot_JumpToApplication(void) {
    // Application으로 점프 // 정상적으로 실행되면 반환하지 않습니다.
    HwSystem_JumpToVectorTable(BOOT_APP_START_ADDRESS);

    while (1) {}
}

void BspBoot_Reset(void) {
    // MCU 소프트웨어 Reset
    HwSystem_Reset();

    while (1) {}
}
