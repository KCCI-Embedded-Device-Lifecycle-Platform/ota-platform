#ifndef EVSE_BOOT_APP__AP_BOOT_CONSOLE_H
#define EVSE_BOOT_APP__AP_BOOT_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>

// tables
typedef enum {
    AP_BOOT_CONSOLE_RESULT_NONE = 0,
    AP_BOOT_CONSOLE_RESULT_LINE_READY,
    AP_BOOT_CONSOLE_RESULT_OVERFLOW,
    AP_BOOT_CONSOLE_RESULT_ERROR
} ap_boot_console_result_t;

// functions
void ApBootConsole_Init(void);
bool ApBootConsole_SendString(const char *message);
ap_boot_console_result_t ApBootConsole_PollLine(char *output_line, size_t output_line_size);

#endif // EVSE_BOOT_APP__AP_BOOT_CONSOLE_H