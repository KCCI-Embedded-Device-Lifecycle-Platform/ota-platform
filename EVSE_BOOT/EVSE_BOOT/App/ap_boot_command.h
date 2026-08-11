#ifndef EVSE_BOOT_APP__AP_BOOT_COMMAND_H
#define EVSE_BOOT_APP__AP_BOOT_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "App/ap_boot_update.h"
#include "Middleware/boot_protocol.h"
#include "Common/boot_config.h"

// tables
typedef enum {
    AP_BOOT_ACTION_NONE = 0,
    AP_BOOT_ACTION_ENTER_BINARY_PROTOCOL,
    AP_BOOT_ACTION_RUN_APPLICATION

#if BOOT_ENABLE_STORAGE_TEST
    ,AP_BOOT_ACTION_RUN_STORAGE_TEST
#endif
#if BOOT_ENABLE_CRC32_TEST
    ,AP_BOOT_ACTION_RUN_CRC32_TEST
#endif
#if BOOT_ENABLE_UPDATE_TEST
    ,AP_BOOT_ACTION_RUN_UPDATE_TEST
#endif
} ap_boot_action_t;

typedef struct {
    ap_boot_action_t action;
    // 정적 문자열 반환 // 응답 없으면 NULL
    const char *response;
} ap_boot_text_command_result_t;

typedef struct {
    ap_boot_action_t action;

    bool response_ready;

    boot_protocol_packet_t response;
} ap_boot_binary_command_result_t;

// functions
bool ApBootCommand_ProcessText(const char *command, ap_boot_text_command_result_t *result);
bool ApBootCommand_ProcessPacket(const boot_protocol_packet_t *request, ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result);
void ApBootCommand_BuildNack(uint8_t request_command, boot_protocol_error_t error, boot_protocol_packet_t *response);

#endif // EVSE_BOOT_APP__AP_BOOT_COMMAND_H