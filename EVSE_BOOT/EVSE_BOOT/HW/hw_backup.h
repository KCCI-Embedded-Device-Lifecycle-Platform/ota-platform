#ifndef EVSE_BOOT_HW__HW_BACKUP_H
#define EVSE_BOOT_HW__HW_BACKUP_H

#include <stdint.h>

#include "HW/hw_common.h"

// functions
hw_status_t HwBackup_Init(void);
hw_status_t HwBackup_ReadWord(uint32_t offset, uint32_t *value);
hw_status_t HwBackup_WriteWord(uint32_t offset, uint32_t value);

#endif // EVSE_BOOT_HW__HW_BACKUP_H