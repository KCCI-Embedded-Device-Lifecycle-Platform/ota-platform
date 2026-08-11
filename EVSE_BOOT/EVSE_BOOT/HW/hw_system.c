#include "hw_system.h"

#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

// defines
/* STM32F429ZI Main SRAM */
// 0x20000000 ~ 0x2002FFFF
// 초기 MSP는 RAM의 끝 다음 주소인 0x20030000을 가리킬 수 있으므로 끝 주소를 포함해서 검사합니다.
#define HW_SYSTEM_MAIN_SRAM_START    0x20000000UL
#define HW_SYSTEM_MAIN_SRAM_END      0x20030000UL

/* STM32F429ZI CCM RAM */
// 0x10000000 ~ 0x1000FFFF
#define HW_SYSTEM_CCM_RAM_START      0x10000000UL
#define HW_SYSTEM_CCM_RAM_END        0x10010000UL

// STM32F429ZI Vector Table 크기를 고려한 정렬 조건입니다.
// 현재 Application 시작 주소 0x08020000은 이 조건을 만족합니다.
#define HW_SYSTEM_VECTOR_ALIGNMENT   0x200UL

// inners
typedef void (*hw_system_entry_t)(void);

static bool HwSystem_IsStackPointerValid(uint32_t stack_pointer) {
    bool main_sram_valid;
    bool ccm_ram_valid;
    bool alignment_valid;

    main_sram_valid = (stack_pointer > HW_SYSTEM_MAIN_SRAM_START) && (stack_pointer <= HW_SYSTEM_MAIN_SRAM_END);

    ccm_ram_valid = (stack_pointer > HW_SYSTEM_CCM_RAM_START) && (stack_pointer <= HW_SYSTEM_CCM_RAM_END);

    // ARM EABI 기준 Stack Pointer는 8바이트 정렬을 사용합니다.
    alignment_valid = (stack_pointer & 0x07UL) == 0UL;

    return (main_sram_valid || ccm_ram_valid) && alignment_valid;
}

// functions
bool HwSystem_IsVectorTableValid(uint32_t vector_table_address, uint32_t executable_end_address) {
    // Vector Table의 초기 MSP와 Reset Handler를 검사합니다.
    // vector_table_address:    Application Vector Table 시작 주소
    // executable_end_address:  실행 가능한 Flash 영역의 끝 다음 주소
    uint32_t initial_stack_pointer;
    uint32_t reset_vector;
    uint32_t reset_handler_address;

    if (vector_table_address >= executable_end_address) return false;

    // Vector Table 주소 정렬 검사
    if ((vector_table_address & (HW_SYSTEM_VECTOR_ALIGNMENT - 1UL)) != 0UL) return false;

    initial_stack_pointer = *(volatile uint32_t *)vector_table_address;

    reset_vector = *(volatile uint32_t *)(vector_table_address + 4UL);

    // Cortex-M 함수 주소의 bit 0은 Thumb 상태를 나타냅니다.
    if ((reset_vector & 0x01UL) == 0UL) return false;

    reset_handler_address = reset_vector & ~0x01UL;

    if (!HwSystem_IsStackPointerValid(initial_stack_pointer)) return false;

    if ((reset_handler_address < vector_table_address) || (reset_handler_address >= executable_end_address))
        return false;

    return true;
}

void HwSystem_JumpToVectorTable(uint32_t vector_table_address) {
    // 지정된 Vector Table을 사용하는 Application으로 점프합니다.
    // 정상적으로 실행되면 반환하지 않습니다.
    uint32_t initial_stack_pointer;
    uint32_t reset_vector;
    uint32_t nvic_index;

    hw_system_entry_t application_entry;

    initial_stack_pointer = *(volatile uint32_t *)vector_table_address;

    reset_vector = *(volatile uint32_t *)(vector_table_address + 4UL);

    application_entry = (hw_system_entry_t)(uintptr_t)reset_vector;

    // Bootloader 인터럽트가 Application 실행 중 발생하지 않도록
    // 전역 인터럽트를 먼저 비활성화합니다.
    __disable_irq();

    // Bootloader가 사용한 HAL 및 Clock 상태를 정리합니다.
    HAL_RCC_DeInit();
    HAL_DeInit();

    // SysTick 정지
    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL = 0UL;

    // PendSV와 SysTick Pending 상태 제거
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;

    // 모든 NVIC Interrupt Enable과 Pending 상태를 제거합니다.
    for (nvic_index = 0UL; nvic_index < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); nvic_index++) {
        NVIC->ICER[nvic_index] = 0xFFFFFFFFUL;
        NVIC->ICPR[nvic_index] = 0xFFFFFFFFUL;
    }

    // Application Vector Table 주소로 변경
    SCB->VTOR = vector_table_address;

    // Thread Mode가 MSP를 사용하도록 초기화
    __set_CONTROL(0UL);
    __set_PSP(0UL);
    __set_BASEPRI(0UL);
    __set_FAULTMASK(0UL);

    // Application의 초기 MSP 적용
    __set_MSP(initial_stack_pointer);

    __DSB();
    __ISB();

    // NVIC Enable/Pending 상태를 모두 제거했으므로 Application 실행 직전에 전역 인터럽트를 복구합니다
    __enable_irq();

    application_entry();

    // 정상적인 경우 Application에서 반환하지 않습니다.
    while (1) {}
}   

void HwSystem_Reset(void) {
    // MCU를 소프트웨어 Reset합니다.
    __disable_irq();

    NVIC_SystemReset();

    while (1) {}
}

uint32_t HwSystem_GetTick(void) {
    return HAL_GetTick();
}

void HwSystem_Delay(uint32_t delay_ms) {
    HAL_Delay(delay_ms);
}