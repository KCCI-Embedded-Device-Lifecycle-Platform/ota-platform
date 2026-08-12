#include "ap_bootloader.h"

#include <stddef.h>
#include <stdint.h>

#include "App/ap_boot_update.h"
#include "App/ap_boot_command.h"
#include "App/ap_boot_console.h"
#include "BSP/bsp_boot.h"
#include "BSP/bsp_transport.h"
#include "BSP/bsp_storage.h"
#include "Middleware/boot_protocol.h"
#include "Common/boot_config.h"

#if BOOT_ENABLE_STORAGE_TEST
#include "App/ap_boot_storage_test.h"
#endif
#if BOOT_ENABLE_CRC32_TEST
#include "App/ap_boot_crc32_test.h"
#endif
#if BOOT_ENABLE_UPDATE_TEST
#include "App/ap_boot_update_test.h"
#endif 

// tables
typedef enum {
    AP_BOOT_COMM_MODE_TEXT = 0,
    AP_BOOT_COMM_MODE_BINARY
} ap_boot_comm_mode_t;

// inners
static bool s_initialized;

static ap_boot_update_context_t s_update_context;
static boot_protocol_parser_t s_protocol_parser;

static uint8_t s_protocol_tx_frame[BOOT_PROTOCOL_MAX_FRAME_SIZE];

#define AP_BOOT_STAGING_COPY_CHUNK_SIZE 512U

static bool ApBootloader_ApplyStagedUpdate(
    uint32_t image_size,
    uint32_t image_crc32) {
    uint32_t offset;

    if ((image_size == 0U) ||
        (image_size > BOOT_STAGING_CAPACITY) ||
        (image_size > BOOT_APP_MAX_SIZE)) {
        return false;
    }

    if (!ApBootUpdate_Start(
            &s_update_context,
            image_size,
            image_crc32)) {
        return false;
    }

    offset = 0U;

    while (offset < image_size) {
        uint32_t chunk_size = image_size - offset;
        const uint8_t *source;

        if (chunk_size > AP_BOOT_STAGING_COPY_CHUNK_SIZE)
            chunk_size = AP_BOOT_STAGING_COPY_CHUNK_SIZE;

        source = (const uint8_t *)(uintptr_t)(
            BOOT_STAGING_START_ADDRESS + offset);

        if (!ApBootUpdate_WriteData(
                &s_update_context,
                offset,
                source,
                chunk_size)) {
            return false;
        }

        offset += chunk_size;
    }

    return ApBootUpdate_End(&s_update_context);
}

static bool ApBootloader_SendProtocolPacket(const boot_protocol_packet_t *packet) {
    uint16_t frame_length;
    mw_status_t middleware_status;
    bsp_status_t transport_status;

    if (packet == NULL) return false;

    middleware_status = BootProtocol_Encode(packet, s_protocol_tx_frame, sizeof(s_protocol_tx_frame), &frame_length);

    if (middleware_status != MW_STATUS_OK) return false;

    transport_status = BspTransport_Send(s_protocol_tx_frame, frame_length);

    return transport_status == BSP_STATUS_OK;
}

static void ApBootloader_SendProtocolError(boot_protocol_error_t error) {
    boot_protocol_packet_t response;

    ApBootCommand_BuildNack(BootProtocol_GetLastCommand(&s_protocol_parser), error, &response);

    (void)ApBootloader_SendProtocolPacket(&response);
}

static void ApBootloader_JumpToApplication(bool send_text_log) {
    if (!BspBoot_IsApplicationValid()) {
        if (send_text_log) { 
            (void)ApBootConsole_SendString("[BOOT] No valid application\r\n");
        }

        return;
    }

    if (send_text_log) (void)ApBootConsole_SendString("[BOOT] Jump to application\r\n");
    
    // Bootloader 상태 LED를 끈 뒤 Application으로 이동
    (void)BspBoot_SetStatusLed(false);

    // 현재 UART 송신은 Blocking 방식
    BspBoot_Delay(50U);

    BspBoot_JumpToApplication();

    // 정상의 경우 반환x
    while (1) {}
}

static void ApBootloader_ProcessTextMode(ap_boot_comm_mode_t *communication_mode, ap_boot_action_t *action) {
    char command_line[BOOT_UART_LINE_BUFFER_SIZE];

    ap_boot_console_result_t console_result;
    ap_boot_text_command_result_t command_result;

    if ((communication_mode == NULL) || (action == NULL)) return;

    console_result = ApBootConsole_PollLine(command_line, sizeof(command_line));

    if (console_result == AP_BOOT_CONSOLE_RESULT_LINE_READY) {
        if (!ApBootCommand_ProcessText(command_line, &command_result)) return;

        if (command_result.response != NULL) (void)ApBootConsole_SendString(command_result.response);

        if (command_result.action == AP_BOOT_ACTION_ENTER_BINARY_PROTOCOL) {
            BootProtocol_Init(&s_protocol_parser);
            *communication_mode = AP_BOOT_COMM_MODE_BINARY;
        }
        *action = command_result.action;
    }
    else if (console_result == AP_BOOT_CONSOLE_RESULT_OVERFLOW) {
        (void)ApBootConsole_SendString("[BOOT] RX OVERFLOW\r\n");
    }
}

static void ApBootloader_ProcessBinaryMode(ap_boot_action_t *action) {
    uint8_t received_byte;
    bool response_sent;

    bsp_status_t transport_status;
    boot_protocol_result_t protocol_result;

    boot_protocol_packet_t received_packet;
    ap_boot_binary_command_result_t command_result;

    if (action == NULL) return;

    transport_status = BspTransport_ReceiveByte(&received_byte);

    if ((transport_status == BSP_STATUS_TIMEOUT) || (transport_status == BSP_STATUS_BUSY)) return;
    if (transport_status != BSP_STATUS_OK) return;

    protocol_result = BootProtocol_ProcessByte(&s_protocol_parser, received_byte, &received_packet);

    if (protocol_result == BOOT_PROTOCOL_RESULT_PACKET_READY) {
        if (!ApBootCommand_ProcessPacket(&received_packet, &s_update_context, &command_result)) return;

        if (command_result.response_ready) response_sent = ApBootloader_SendProtocolPacket(&command_result.response);
        else response_sent = true;
        
        // RUN_APP의 경우 ACK가 정상적으로 전송된 뒤에만 Application 실행 Action 반영
        if (response_sent) *action = command_result.action;

        return;
    }

    if (protocol_result == BOOT_PROTOCOL_RESULT_CRC_ERROR) {
        ApBootloader_SendProtocolError(BOOT_PROTOCOL_ERROR_CRC);

        return;
    }

    if (protocol_result == BOOT_PROTOCOL_RESULT_LENGTH_ERROR) {
        ApBootloader_SendProtocolError(BOOT_PROTOCOL_ERROR_LENGTH);

        return;
    }

    if (protocol_result == BOOT_PROTOCOL_RESULT_FORMAT_ERROR) {
        ApBootloader_SendProtocolError(BOOT_PROTOCOL_ERROR_FORMAT);
    }
}

static void ApBootloader_CommandLoop(void) {
    uint32_t led_previous_tick;

    ap_boot_action_t action;
    ap_boot_comm_mode_t communication_mode;

    communication_mode = AP_BOOT_COMM_MODE_TEXT;

    BootProtocol_Init(&s_protocol_parser);

    led_previous_tick = BspBoot_GetTick();

    (void)ApBootConsole_SendString("[BOOT] Waiting for UART command\r\n");

    while (1) {
        action = AP_BOOT_ACTION_NONE;

        if (communication_mode == AP_BOOT_COMM_MODE_TEXT) ApBootloader_ProcessTextMode(&communication_mode, &action);
        else ApBootloader_ProcessBinaryMode(&action);

        if (action == AP_BOOT_ACTION_RUN_APPLICATION)
            ApBootloader_JumpToApplication(communication_mode == AP_BOOT_COMM_MODE_TEXT);
#if BOOT_ENABLE_STORAGE_TEST
        else if (action == AP_BOOT_ACTION_RUN_STORAGE_TEST) (void)ApBootStorageTest_Run();
#endif
#if BOOT_ENABLE_CRC32_TEST
        else if (action == AP_BOOT_ACTION_RUN_CRC32_TEST) (void)ApBootCrc32Test_Run();
#endif
#if BOOT_ENABLE_UPDATE_TEST
        else if (action == AP_BOOT_ACTION_RUN_UPDATE_TEST) (void)ApBootUpdateTest_Run();
#endif
        if ((BspBoot_GetTick() - led_previous_tick) >= BOOT_LED_TOGGLE_INTERVAL_MS) {
            led_previous_tick = BspBoot_GetTick();
            (void)BspBoot_ToggleStatusLed();
        }
    }
}

// functions
bool ApBootloader_Init(void) {
    // BSP, Transport, Console, Protocol Parser 초기화
    if (s_initialized) return true;
    if (BspBoot_Init() != BSP_STATUS_OK) return false;
    if (BspTransport_Init() != BSP_STATUS_OK) return false;
    if (BspStorage_Init() != BSP_STORAGE_STATUS_OK) return false;
    if (!ApBootUpdate_Init(&s_update_context)) return false;
    ApBootConsole_Init();

    BootProtocol_Init(&s_protocol_parser);

    s_initialized = true;

    return true;
}

void ApBootloader_Run(void) {
    // Bootloader 전체 실행 흐름 시작
    bool enter_bootloader_mode;
    bool button_requested;
    bool remote_update_requested;
    bool staged_update_available;
    uint32_t staged_image_size;
    uint32_t staged_image_crc32;

    bsp_status_t boot_status;

    if (!s_initialized) return;

    enter_bootloader_mode = false;
    button_requested = false;
    remote_update_requested = false;
    staged_update_available = false;
    staged_image_size = 0U;
    staged_image_crc32 = 0U;

    (void)ApBootConsole_SendString("[BOOT] Bootloader start\r\n");

    // 1. USER 버튼 요청 확인
    boot_status = BspBoot_GetButtonRequest(&button_requested);

    if (boot_status != BSP_STATUS_OK) {
        // 버튼 입력 오류 발생 시 안전상 App으로 바로 이동하지 않고 Boot 머뭄
        enter_bootloader_mode = true;

        (void)ApBootConsole_SendString("[BOOT] Button read error\r\n");
    } 
    else if (button_requested) {
        enter_bootloader_mode = true;

        (void)ApBootConsole_SendString("[BOOT] USER button pressed\r\n");
    }

    // 2. App이 Backup SRAM에 기록한, 원격 Boot 진입 요청 확인
    // USER 버튼 눌린 경우에도 원격 요청 확인
    // 두 요청 동시 존재 시 Remote Magic 삭제
    boot_status = BspBoot_GetRemoteUpdateRequest(&remote_update_requested);

    if (boot_status != BSP_STATUS_OK) {
        // Backup SRAM 읽기 x >> 원격 요청 유무 확정 x >> 안전상 Boot 머뭄
        enter_bootloader_mode = true;

        (void)ApBootConsole_SendString("[BOOT] Remote request read error\r\n");
    }
    else if (remote_update_requested) {
        enter_bootloader_mode = true;

        (void)ApBootConsole_SendString("[BOOT] Remote update request detected\r\n");

        // Remote Request는 일회성
        // Boot가 감지한 즉시 삭제

        boot_status = BspBoot_ClearRemoteUpdateRequest();

        if (boot_status != BSP_STATUS_OK) {
            // 삭제 실패 시에도 App 이동 x
            // 다음 Reset에서도 다시 Bootloader로 진입
            (void)ApBootConsole_SendString("[BOOT] Remote request clear error\r\n");
        }
        else {
            boot_status = BspBoot_GetStagedUpdate(
                &staged_update_available,
                &staged_image_size,
                &staged_image_crc32);

            if (boot_status != BSP_STATUS_OK) {
                (void)BspBoot_SetUpdateResult(false);
                (void)ApBootConsole_SendString(
                    "[BOOT] Staged metadata invalid\r\n");
            }
            else if (staged_update_available) {
                (void)ApBootConsole_SendString(
                    "[BOOT] Applying staged firmware\r\n");

                if (ApBootloader_ApplyStagedUpdate(
                        staged_image_size,
                        staged_image_crc32)) {
                    boot_status = BspBoot_ClearStagedUpdate();

                    if (boot_status == BSP_STATUS_OK)
                        boot_status = BspBoot_SetUpdateResult(true);

                    if (boot_status == BSP_STATUS_OK) {
                        (void)ApBootConsole_SendString(
                            "[BOOT] Staged firmware applied\r\n");
                        ApBootloader_JumpToApplication(true);
                    }
                }

                (void)BspBoot_SetUpdateResult(false);
                (void)BspBoot_ClearStagedUpdate();
                (void)ApBootConsole_SendString(
                    "[BOOT] Staged firmware apply failed\r\n");
            }
        }
    }

    // 3. 앞선 진입 조건 없을 때 App 검사
    if ((!enter_bootloader_mode) && (!BspBoot_IsApplicationValid())) {
        enter_bootloader_mode = true;

        (void)ApBootConsole_SendString("[BOOT] No valid application\r\n");
    }

    if (enter_bootloader_mode) {
        (void)ApBootConsole_SendString("[BOOT] Stay in bootloader mode\r\n");

        ApBootloader_CommandLoop();
    }

    // 버튼 요청, 원격 요청, App 오류 없으면 기존과 동일하게 App으로 이동
    ApBootloader_JumpToApplication(true);

    // Jump 실패 or 예상과 달리 반환된 경우 Boot 명령 모드 전환
    (void)ApBootConsole_SendString("[BOOT] Stay in bootloader mode\r\n");

    ApBootloader_CommandLoop();
}
