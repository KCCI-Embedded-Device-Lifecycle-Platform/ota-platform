#ifndef EVSE_BOOT_HW__HW_COMMON_H
#define EVSE_BOOT_HW__HW_COMMON_H

// tables
typedef enum {
    HW_STATUS_OK = 0,
    HW_STATUS_ERROR,
    HW_STATUS_TIMEOUT,
    HW_STATUS_BUSY,
    HW_STATUS_INVALID_ARGUMENT,
    HW_STATUS_NOT_INITIALIZED
} hw_status_t;

#endif // EVSE_BOOT_HW__HW_COMMON_H