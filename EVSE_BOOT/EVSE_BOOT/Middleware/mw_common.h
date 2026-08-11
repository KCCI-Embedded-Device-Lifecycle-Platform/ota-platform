#ifndef EVSE_BOOT_MW__MW_COMMON_H
#define EVSE_BOOT_MW__MW_COMMON_H

// tables
typedef enum {
    MW_STATUS_OK = 0,
    MW_STATUS_ERROR,
    MW_STATUS_INVALID_ARGUMENT,
    MW_STATUS_BUFFER_TOO_SMALL
} mw_status_t;

#endif // EVSE_BOOT_MW__MW_COMMON_H