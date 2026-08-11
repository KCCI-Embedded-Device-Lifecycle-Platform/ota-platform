#include "ap_boot_command.h"

#include <stddef.h>
#include <string.h>

#include "BSP/bsp_boot.h"
#include "Common/boot_config.h"

// defines
#define AP_BOOT_COMMAND_UINT32_SIZE              4U

#define AP_BOOT_COMMAND_START_UPDATE_LENGTH      8U
#define AP_BOOT_COMMAND_DATA_HEADER_LENGTH       4U

#define AP_BOOT_COMMAND_UPDATE_ACK_LENGTH        6U
#define AP_BOOT_COMMAND_UPDATE_NACK_LENGTH       7U

// inners
static void ApBootCommand_BuildAck(uint8_t request_command, boot_protocol_packet_t *response) {
    if (response == NULL) return;

    memset(response, 0, sizeof(*response));

    response->command = BOOT_PROTOCOL_CMD_ACK;
    response->length = 2U;
    response->data[0] = request_command;
    response->data[1] = BOOT_PROTOCOL_ERROR_NONE;
}

static uint32_t ApBootCommand_ReadUint32Le(const uint8_t *data) {
    return ((uint32_t)data[0]) | ((uint32_t)data[1] << 8U) | ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static void ApBootCommand_WriteUint32Le(uint8_t *data, uint32_t value) {
    // little endian 변환 함수
    data[0] = (uint8_t)(value & 0xFFUL);
    data[1] = (uint8_t)((value >> 8U) & 0xFFUL);
    data[2] = (uint8_t)((value >> 16U) & 0xFFUL);
    data[3] = (uint8_t)((value >> 24U) & 0xFFUL);
}

static void ApBootCommand_ResetBinaryResult(ap_boot_binary_command_result_t *result) {
    if (result == NULL) return;

    memset(result, 0, sizeof(*result));

    result->action = AP_BOOT_ACTION_NONE;
    result->response_ready = false;
}

static void ApBootCommand_PrepareUpdateAck(uint8_t request_command, const ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result) {
    // ACK 생성 함수
    boot_protocol_packet_t *response;

    if ((update_context == NULL) || (result == NULL)) return;

    response = &result->response;

    memset(response, 0, sizeof(*response));

    response->command = BOOT_PROTOCOL_CMD_ACK;
    response->length = AP_BOOT_COMMAND_UPDATE_ACK_LENGTH; 
    response->data[0] = request_command;                                // DATA[0]: 처리한 요청 Command
    response->data[1] = (uint8_t)ApBootUpdate_GetState(update_context); // DATA[1]: 현재 OTA 상태
    ApBootCommand_WriteUint32Le(&response->data[2], ApBootUpdate_GetNextOffset(update_context)); // DATA[2..5]: 다음에 수신할 Offset

    result->response_ready = true;
}

static void ApBootCommand_PrepareUpdateNack(uint8_t request_command, ap_boot_update_error_t update_error, const ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result) {
    // NACK 생성 함수
    boot_protocol_packet_t *response;

    if ((update_context == NULL) || (result == NULL)) return;

    response = &result->response;

    memset(response, 0, sizeof(*response));

    response->command = BOOT_PROTOCOL_CMD_NACK;
    response->length = AP_BOOT_COMMAND_UPDATE_NACK_LENGTH;
    response->data[0] = request_command;    // DATA[0]: 실패한 요청 Command
    response->data[1] = (uint8_t)update_error;  // DATA[1]: OTA 상태머신 오류
    response->data[2] = (uint8_t)ApBootUpdate_GetState(update_context); // DATA[2]: 현재 OTA 상태
    ApBootCommand_WriteUint32Le(&response->data[3], ApBootUpdate_GetNextOffset(update_context)); // DATA[3..6]: 다시 전송해야 할 Offset

    result->response_ready = true;
}

static void ApBootCommand_HandleHello(const boot_protocol_packet_t *request, ap_boot_binary_command_result_t *result) {
    if ((request == NULL) || (result == NULL)) return;

    if (request->length != 0U) ApBootCommand_BuildNack(request->command, BOOT_PROTOCOL_ERROR_LENGTH, &result->response);
    else ApBootCommand_BuildAck(request->command, &result->response);

    result->response_ready = true;
}

static void ApBootCommand_HandleGetVersion(const boot_protocol_packet_t *request, ap_boot_binary_command_result_t *result) {
    if ((request == NULL) || (result == NULL)) return;

    if (request->length != 0U) {
        ApBootCommand_BuildNack(request->command, BOOT_PROTOCOL_ERROR_LENGTH, &result->response);

        result->response_ready = true;

        return;
    }

    memset(&result->response, 0, sizeof(result->response));

    result->response.command = BOOT_PROTOCOL_CMD_VERSION_RESPONSE;
    result->response.length = 3U;
    result->response.data[0] = BOOTLOADER_VERSION_MAJOR;
    result->response.data[1] = BOOTLOADER_VERSION_MINOR;
    result->response.data[2] = BOOTLOADER_VERSION_PATCH;

    result->response_ready = true;
}

static void ApBootCommand_HandleStartUpdate(const boot_protocol_packet_t *request, ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result) {
    // START_UPDATE Handler
    uint32_t image_size;
    uint32_t expected_crc32;

    if ((request == NULL) || (update_context == NULL) || (result == NULL)) return;

    if (request->length != AP_BOOT_COMMAND_START_UPDATE_LENGTH) {
        ApBootCommand_PrepareUpdateNack(request->command, AP_BOOT_UPDATE_ERROR_INVALID_LENGTH, update_context, result);

        return;
    }

    image_size = ApBootCommand_ReadUint32Le(&request->data[0]);

    expected_crc32 = ApBootCommand_ReadUint32Le(&request->data[4]);

    if (!ApBootUpdate_Start(update_context, image_size, expected_crc32)) {
        ApBootCommand_PrepareUpdateNack(request->command, ApBootUpdate_GetLastError(update_context), update_context, result);

        return;
    }

    ApBootCommand_PrepareUpdateAck(request->command, update_context, result);
}

static void ApBootCommand_HandleData(const boot_protocol_packet_t *request, ap_boot_update_context_t *update_context,  ap_boot_binary_command_result_t *result) {
    // DATA Handler
    uint32_t offset;
    uint32_t firmware_length;

    if ((request == NULL) || (update_context == NULL) || (result == NULL)) return;

    // Offset 4바이트, Firmware 데이터 최소 1바이트 필요
    if (request->length <= AP_BOOT_COMMAND_DATA_HEADER_LENGTH) {
        ApBootCommand_PrepareUpdateNack(request->command, AP_BOOT_UPDATE_ERROR_INVALID_LENGTH, update_context, result);

        return;
    }

    offset = ApBootCommand_ReadUint32Le(&request->data[0]);

    firmware_length = (uint32_t)request->length - AP_BOOT_COMMAND_DATA_HEADER_LENGTH;

    if (!ApBootUpdate_WriteData(update_context, offset, &request->data[AP_BOOT_COMMAND_DATA_HEADER_LENGTH], firmware_length)) {
        ApBootCommand_PrepareUpdateNack(request->command, ApBootUpdate_GetLastError(update_context), update_context, result);

        return;
    }

    ApBootCommand_PrepareUpdateAck(request->command, update_context, result);
}

static void ApBootCommand_HandleEndUpdate(const boot_protocol_packet_t *request, ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result) {
    // END_UPDATE Handler
    if ((request == NULL) || (update_context == NULL) || (result == NULL)) return;

    if (request->length != 0U) {
        ApBootCommand_PrepareUpdateNack(request->command, AP_BOOT_UPDATE_ERROR_INVALID_LENGTH, update_context, result);

        return;
    }

    if (!ApBootUpdate_End(update_context)) {
        ApBootCommand_PrepareUpdateNack(request->command, ApBootUpdate_GetLastError(update_context), update_context, result);

        return;
    }

    ApBootCommand_PrepareUpdateAck(request->command, update_context, result);
}

static void ApBootCommand_HandleAbort(const boot_protocol_packet_t *request, ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result) {
    // ABORT Handler
    if ((request == NULL) || (update_context == NULL) || (result == NULL)) return;

    if (request->length != 0U) {
        ApBootCommand_PrepareUpdateNack(request->command, AP_BOOT_UPDATE_ERROR_INVALID_LENGTH, update_context, result);

        return;
    }

    ApBootUpdate_Abort(update_context);

    ApBootCommand_PrepareUpdateAck(request->command, update_context, result);
}

static void ApBootCommand_HandleRunApplication(const boot_protocol_packet_t *request, ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result) {
    // RUN_APP Handler
    ap_boot_update_state_t update_state;

    if ((request == NULL) || (update_context == NULL) || (result == NULL)) return;

    if (request->length != 0U) {
        ApBootCommand_PrepareUpdateNack(request->command, AP_BOOT_UPDATE_ERROR_INVALID_LENGTH, update_context, result);

        return;
    }

    update_state = ApBootUpdate_GetState(update_context);

    if ((update_state == AP_BOOT_UPDATE_STATE_ERASING) || (update_state == AP_BOOT_UPDATE_STATE_RECEIVING) ||
        (update_state == AP_BOOT_UPDATE_STATE_VERIFYING) || (update_state == AP_BOOT_UPDATE_STATE_FAILED)) {
        ApBootCommand_PrepareUpdateNack(request->command, AP_BOOT_UPDATE_ERROR_INVALID_STATE, update_context, result);

        return;
    }

    if (!BspBoot_IsApplicationValid()) {
        ApBootCommand_PrepareUpdateNack(request->command, AP_BOOT_UPDATE_ERROR_APPLICATION_INVALID, update_context, result);

        return;
    }

    ApBootCommand_PrepareUpdateAck(request->command, update_context, result);

    // Bootloader가 ACK 송신 후 실행
    result->action = AP_BOOT_ACTION_RUN_APPLICATION;
}

// functions
void ApBootCommand_BuildNack(uint8_t request_command, boot_protocol_error_t error, boot_protocol_packet_t *response) {
    // Protocol Parser 오류에 대한 NACK 패킷 생성
    if (response == NULL) return;

    memset(response, 0, sizeof(*response));

    response->command = BOOT_PROTOCOL_CMD_NACK;
    response->length = 2U;
    response->data[0] = request_command;
    response->data[1] = (uint8_t)error;
}

bool ApBootCommand_ProcessText(const char *command, ap_boot_text_command_result_t *result) {
    // HELLO, VERSION, PROTO, RUN 문자열 명령 처리
    if ((command == NULL) || (result == NULL)) return false;

    result->action = AP_BOOT_ACTION_NONE;
    result->response = NULL;

    if (strcmp(command, "HELLO") == 0) {
        result->response = "[BOOT] ACK\r\n";

        return true;
    }

    if (strcmp(command, "VERSION") == 0) {
        result->response = "[BOOT] VERSION " BOOTLOADER_VERSION_STRING "\r\n";

        return true;
    }

    if (strcmp(command, "PROTO") == 0) {
        result->response = "[BOOT] Enter binary protocol mode\r\n";

        result->action = AP_BOOT_ACTION_ENTER_BINARY_PROTOCOL;

        return true;
    }

    if (strcmp(command, "RUN") == 0) {
        result->action = AP_BOOT_ACTION_RUN_APPLICATION;

        return true;
    }
#if BOOT_ENABLE_STORAGE_RESET_TEST
    if (strcmp(command, "FLASH_TEST") == 0) {
        result->response = 
            "[FLASH TEST] Destructive test\r\n"
            "[FLASH TEST] Application will be erased\r\n"
            "[FLASH TEST] Enter FLASH_TEST_CONFIRM "
            "to continue\r\n";

        return true;
    }

    if (strcmp(command, "FLASH_TEST_CONFIRM") == 0) {
        result->response = "[FLASH TEST] Confirmed\r\n";
        result->action = AP_BOOT_ACTION_RUN_STORAGE_TEST;

        return true;
    }
#endif
#if BOOT_ENABLE_CRC32_TEST
    if (strcmp(command, "CRC32_TEST") == 0) {
        result->response = "[CRC32 TEST] Command received\r\n";
        result->action = AP_BOOT_ACTION_RUN_CRC32_TEST;

        return true;
    }
#endif
#if BOOT_ENABLE_UPDATE_TEST
    if (strcmp(command, "UPDATE_TEST") == 0) {
        result->response = "[UPDATE TEST] Destructive OTA state-machine test\r\n"
                           "[UPDATE TEST] Application Sector 5 will be erased\r\n"
                           "[UPDATE TEST] Enter UPDATE_TEST_CONFIRM to continue\r\n";

        return true;
    }

    if (strcmp(command, "UPDATE_TEST_CONFIRM") == 0) {
        result->response = "[UPDATE TEST] Confirmed\r\n";

        result->action = AP_BOOT_ACTION_RUN_UPDATE_TEST;

        return true;
    }
#endif
    result->response = "[BOOT] NACK\r\n";

    return true;
}

bool ApBootCommand_ProcessPacket(const boot_protocol_packet_t *request, ap_boot_update_context_t *update_context, ap_boot_binary_command_result_t *result) {
    // binary Protocol Packet 명령 처리
    if ((request == NULL) || (update_context == NULL) || (result == NULL)) return false;

    ApBootCommand_ResetBinaryResult(result);

    switch (request->command) {
        case BOOT_PROTOCOL_CMD_HELLO:
            ApBootCommand_HandleHello(request, result);
            break;

        case BOOT_PROTOCOL_CMD_GET_VERSION:
            ApBootCommand_HandleGetVersion(request, result);
            break;

        case BOOT_PROTOCOL_CMD_START_UPDATE:
            ApBootCommand_HandleStartUpdate(request, update_context, result);
            break;

        case BOOT_PROTOCOL_CMD_DATA:
            ApBootCommand_HandleData(request, update_context, result);
            break;

        case BOOT_PROTOCOL_CMD_END_UPDATE:
            ApBootCommand_HandleEndUpdate(request, update_context, result);
            break;

        case BOOT_PROTOCOL_CMD_ABORT:
            ApBootCommand_HandleAbort(request, update_context, result);
            break;

        case BOOT_PROTOCOL_CMD_RUN_APP:
            ApBootCommand_HandleRunApplication(request, update_context, result);
            break;

        default: {
            ApBootCommand_BuildNack(request->command, BOOT_PROTOCOL_ERROR_UNSUPPORTED_COMMAND, &result->response);
            result->response_ready = true;
            break;
        }
    }
    return true;
}

