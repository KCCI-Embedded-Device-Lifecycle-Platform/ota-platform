#ifndef EVSE_BOOT_BSP__BSP_BOOT_H
#define EVSE_BOOT_BSP__BSP_BOOT_H

#include <stdbool.h>
#include <stdint.h>

#include "BSP/bsp_common.h"

// functions
bsp_status_t BspBoot_Init(void);

bsp_status_t BspBoot_GetButtonRequest(bool *requested);

bsp_status_t BspBoot_GetRemoteUpdateRequest(bool *requested);
bsp_status_t BspBoot_ClearRemoteUpdateRequest(void);

bsp_status_t BspBoot_GetStagedUpdate(
    bool *available,
    uint32_t *image_size,
    uint32_t *image_crc32);
bsp_status_t BspBoot_ClearStagedUpdate(void);
bsp_status_t BspBoot_SetUpdateResult(bool success);

bool BspBoot_IsApplicationValid(void);

bsp_status_t BspBoot_SetStatusLed(bool enabled);
bsp_status_t BspBoot_ToggleStatusLed(void);

uint32_t BspBoot_GetTick(void);
void BspBoot_Delay(uint32_t delay_ms);

void BspBoot_JumpToApplication(void);
void BspBoot_Reset(void);

#endif // EVSE_BOOT_BSP__BSP_BOOT_H
