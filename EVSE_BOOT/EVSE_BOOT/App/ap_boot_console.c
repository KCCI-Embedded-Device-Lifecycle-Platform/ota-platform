#include "ap_boot_console.h"

#include <stdint.h>
#include <string.h>

#include "BSP/bsp_transport.h"
#include "Common/boot_config.h"

// inners
static char s_rx_buffer[BOOT_UART_LINE_BUFFER_SIZE];
static size_t s_rx_index;

// functions
void ApBootConsole_Init(void) {
    // 텍스트 수신 버퍼 초기화
    s_rx_index = 0U;

    memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
}

bool ApBootConsole_SendString(const char *message) {
    // NULL 종료 문자열 BSP Transport로 전송
    size_t message_length;

    if (message == NULL) return false;

    message_length = strlen(message);

    if ((message_length == 0U) || (message_length > UINT16_MAX)) return false;

    return BspTransport_Send((const uint8_t *)message, (uint16_t)message_length) == BSP_STATUS_OK;
}

ap_boot_console_result_t ApBootConsole_PollLine(char *output_line, size_t output_line_size) {
    // CR or LF까지 수신한 텍스트 한 줄 반환
    uint8_t received_byte;
    bsp_status_t transport_status;

    if ((output_line == NULL) || (output_line_size == 0U)) return AP_BOOT_CONSOLE_RESULT_ERROR;

    transport_status = BspTransport_ReceiveByte(&received_byte);

    if ((transport_status == BSP_STATUS_TIMEOUT) || (transport_status == BSP_STATUS_BUSY))
        return AP_BOOT_CONSOLE_RESULT_NONE;

    if (transport_status != BSP_STATUS_OK) return AP_BOOT_CONSOLE_RESULT_ERROR;

    // CR or LF를 한 줄의 끝으로 처리
    // CRLF의 두 번째 문자는 빈 줄이므로 무시
    if ((received_byte == '\r') || (received_byte == '\n')) {
        if (s_rx_index == 0U) return AP_BOOT_CONSOLE_RESULT_NONE;

        s_rx_buffer[s_rx_index] = '\0';

        if ((s_rx_index + 1U) > output_line_size) {
            s_rx_index = 0U;
            memset(s_rx_buffer, 0, sizeof(s_rx_buffer));

            return AP_BOOT_CONSOLE_RESULT_OVERFLOW;
        }

        memcpy(output_line, s_rx_buffer, s_rx_index + 1U);
        s_rx_index = 0U;
        memset(s_rx_buffer, 0, sizeof(s_rx_buffer));

        return AP_BOOT_CONSOLE_RESULT_LINE_READY;
    }

    if (s_rx_index >= (sizeof(s_rx_buffer) - 1U)) {
        s_rx_index = 0U;
        memset(s_rx_buffer, 0, sizeof(s_rx_buffer));

        return AP_BOOT_CONSOLE_RESULT_OVERFLOW;
    }

    s_rx_buffer[s_rx_index] = (char)received_byte;
    s_rx_index++;

    return AP_BOOT_CONSOLE_RESULT_NONE;
}