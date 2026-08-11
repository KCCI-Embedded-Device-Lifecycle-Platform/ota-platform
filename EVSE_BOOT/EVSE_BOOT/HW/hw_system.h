#ifndef EVSE_BOOT_HW__HW_SYSTEM_H
#define EVSE_BOOT_HW__HW_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

// functions
bool HwSystem_IsVectorTableValid(uint32_t vector_table_address, uint32_t executable_end_address);
void HwSystem_JumpToVectorTable(uint32_t vector_table_address);

void HwSystem_Reset(void);

uint32_t HwSystem_GetTick(void);
void HwSystem_Delay(uint32_t delay_ms);

#endif // EVSE_BOOT_HW__HW_SYSTEM_H