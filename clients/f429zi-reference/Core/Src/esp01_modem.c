#include "esp01_modem.h"
#include "esp01_uart.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define IPD_PREFIX_LENGTH        5U
#define CONTROL_BUFFER_SIZE      512U
#define PACKET_QUEUE_DEPTH       2U

typedef enum
{
    PARSER_STATE_TEXT = 0,
    PARSER_STATE_LINK_ID,
    PARSER_STATE_LENGTH,
    PARSER_STATE_PAYLOAD,
    PARSER_STATE_DROP_PAYLOAD
} parser_state_t;

typedef struct
{
    size_t length;
    uint8_t data[ESP01_MODEM_MAX_UDP_PAYLOAD];
} udp_packet_t;

static const uint8_t ipd_prefix[IPD_PREFIX_LENGTH] = {
    '+', 'I', 'P', 'D', ','
};

static parser_state_t parser_state;

static uint8_t prefix_match_length;
static uint8_t current_link_id;
static size_t expected_payload_length;
static size_t received_payload_length;

static char control_buffer[CONTROL_BUFFER_SIZE];
static size_t control_length;

static udp_packet_t packet_queue[ESP01_MODEM_MAX_LINKS][PACKET_QUEUE_DEPTH];

static uint8_t queue_head[ESP01_MODEM_MAX_LINKS];

static uint8_t queue_tail[ESP01_MODEM_MAX_LINKS];

static uint8_t queue_count[ESP01_MODEM_MAX_LINKS];

static uint32_t dropped_packet_count;

static void control_reset(void)
{
    control_length = 0U;
    control_buffer[0] = '\0';
}

static esp01_modem_status_t wait_for_response(
    const char *expected,
    uint32_t timeout_ms)
{
    uint32_t started_at = HAL_GetTick();

    while (HAL_GetTick() - started_at < timeout_ms)
    {
        esp01_modem_poll();

        if (strstr(control_buffer, expected) != NULL)
            return ESP01_MODEM_STATUS_OK;

        if (strstr(control_buffer, "ERROR") != NULL ||
            strstr(control_buffer, "FAIL") != NULL)
        {
            return ESP01_MODEM_STATUS_AT_ERROR;
        }

        HAL_Delay(1U);
    }

    return ESP01_MODEM_STATUS_TIMEOUT;
}

static esp01_modem_status_t execute_command(
    const char *command,
    const char *expected,
    uint32_t timeout_ms)
{
    size_t command_length;

    if (command == NULL || expected == NULL)
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;

    command_length = strlen(command);

    if (command_length == 0U)
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;

    /*
     * UART에 이미 도착한 +IPD는 packet queue로 보낸 뒤
     * 이전 ASCII 응답만 제거한다.
     */
    esp01_modem_poll();
    control_reset();

    if (!esp01_uart_write(
            (const uint8_t *)command,
            command_length,
            1000U))
    {
        return ESP01_MODEM_STATUS_UART_FAILURE;
    }

    return wait_for_response(
        expected,
        timeout_ms);
}


static void control_append(uint8_t byte)
{
    if (control_length >= sizeof(control_buffer) - 1U)
    {
        size_t retained = sizeof(control_buffer) / 2U;

        memmove(
            control_buffer,
            &control_buffer[control_length - retained],
            retained);

        control_length = retained;
    }

    control_buffer[control_length++] = (char)byte;
    control_buffer[control_length] = '\0';
}

static void reset_packet_parser(void)
{
    parser_state = PARSER_STATE_TEXT;
    current_link_id = 0U;
    expected_payload_length = 0U;
    received_payload_length = 0U;
}

static void parse_text_byte(uint8_t byte)
{
    if (byte == ipd_prefix[prefix_match_length])
    {
        prefix_match_length++;

        if (prefix_match_length == IPD_PREFIX_LENGTH)
        {
            prefix_match_length = 0U;
            parser_state = PARSER_STATE_LINK_ID;
            current_link_id = 0U;
        }

        return;
    }

    for (uint8_t index = 0U;
         index < prefix_match_length;
         index++)
    {
        control_append(ipd_prefix[index]);
    }

    prefix_match_length = 0U;

    if (byte == ipd_prefix[0])
        prefix_match_length = 1U;
    else
        control_append(byte);
}

static void begin_payload(void)
{
    if (expected_payload_length == 0U)
    {
        dropped_packet_count++;
        reset_packet_parser();
        return;
    }

    uint8_t link_id = current_link_id;

    if (expected_payload_length >
            ESP01_MODEM_MAX_UDP_PAYLOAD ||
        queue_count[link_id] >= PACKET_QUEUE_DEPTH)
    {
        dropped_packet_count++;
        parser_state = PARSER_STATE_DROP_PAYLOAD;
        return;
    }

    packet_queue[link_id]
                [queue_tail[link_id]]
                    .length = expected_payload_length;

    parser_state = PARSER_STATE_PAYLOAD;

    
}

static void complete_payload(void)
{
    uint8_t link_id = current_link_id;

    queue_tail[link_id] =
        (uint8_t)((queue_tail[link_id] + 1U) %
                  PACKET_QUEUE_DEPTH);

    queue_count[link_id]++;

    reset_packet_parser();
}

static void parse_byte(uint8_t byte)
{
    switch (parser_state)
    {
    case PARSER_STATE_TEXT:
        parse_text_byte(byte);
        break;

    case PARSER_STATE_LINK_ID:
        if (byte >= '0' && byte <= '9')
        {
            current_link_id =
                (uint8_t)(byte - '0');
        }
        else if (byte == ',' &&
                 current_link_id <
                     ESP01_MODEM_MAX_LINKS)
        {
            expected_payload_length = 0U;
            parser_state = PARSER_STATE_LENGTH;
        }
        else
        {
            dropped_packet_count++;
            reset_packet_parser();
        }
        break;

    case PARSER_STATE_LENGTH:
        if (byte >= '0' && byte <= '9')
        {
            expected_payload_length =
                expected_payload_length * 10U +
                (size_t)(byte - '0');
        }
        else if (byte == ':')
        {
            received_payload_length = 0U;
            begin_payload();
        }
        else
        {
            dropped_packet_count++;
            reset_packet_parser();
        }
        break;

    case PARSER_STATE_PAYLOAD:
        packet_queue[current_link_id]
            [queue_tail[current_link_id]]
                .data[received_payload_length++] = byte;

        if (received_payload_length ==
            expected_payload_length)
        {
            complete_payload();
        }
        break;

    case PARSER_STATE_DROP_PAYLOAD:
        received_payload_length++;

        if (received_payload_length ==
            expected_payload_length)
        {
            reset_packet_parser();
        }
        break;

    default:
        dropped_packet_count++;
        reset_packet_parser();
        break;
    }
}

void esp01_modem_init(void)
{
    parser_state = PARSER_STATE_TEXT;
    prefix_match_length = 0U;
    control_length = 0U;
    control_buffer[0] = '\0';

    memset(packet_queue, 0, sizeof(packet_queue));
    memset(queue_head, 0, sizeof(queue_head));
    memset(queue_tail, 0, sizeof(queue_tail));
    memset(queue_count, 0, sizeof(queue_count));

    dropped_packet_count = 0U;

    reset_packet_parser();
}

void esp01_modem_poll(void)
{
    uint8_t byte;

    while (esp01_uart_read(&byte))
        parse_byte(byte);
}

esp01_modem_status_t esp01_modem_open_udp(
    uint8_t link_id,
    const char *remote_host,
    uint16_t remote_port,
    uint16_t local_port)
{
    char command[192];
    int command_length;

    if (link_id >= ESP01_MODEM_MAX_LINKS ||
        remote_host == NULL ||
        remote_host[0] == '\0' ||
        remote_port == 0U ||
        local_port == 0U)
    {
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;
    }

    command_length = snprintf(
        command,
        sizeof(command),
        "AT+CIPSTART=%u,\"UDP\",\"%s\",%u,%u,0\r\n",
        (unsigned)link_id,
        remote_host,
        (unsigned)remote_port,
        (unsigned)local_port);

    if (command_length <= 0 ||
        (size_t)command_length >= sizeof(command))
    {
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;
    }

    return execute_command(
        command,
        "OK",
        5000U);
}

esp01_modem_status_t esp01_modem_close(
    uint8_t link_id)
{
    char command[32];
    int command_length;

    if (link_id >= ESP01_MODEM_MAX_LINKS)
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;

    command_length = snprintf(
        command,
        sizeof(command),
        "AT+CIPCLOSE=%u\r\n",
        (unsigned)link_id);

    if (command_length <= 0 ||
        (size_t)command_length >= sizeof(command))
    {
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;
    }

    return execute_command(
        command,
        "OK",
        2000U);
}

esp01_modem_status_t esp01_modem_send_udp(
    uint8_t link_id,
    const uint8_t *data,
    size_t length)
{
    char command[40];
    int command_length;
    esp01_modem_status_t status;

    if (link_id >= ESP01_MODEM_MAX_LINKS ||
        data == NULL ||
        length == 0U)
    {
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;
    }

    if (length > ESP01_MODEM_MAX_UDP_PAYLOAD)
        return ESP01_MODEM_STATUS_PACKET_TOO_LARGE;

    command_length = snprintf(
        command,
        sizeof(command),
        "AT+CIPSEND=%u,%u\r\n",
        (unsigned)link_id,
        (unsigned)length);

    if (command_length <= 0 ||
        (size_t)command_length >= sizeof(command))
    {
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;
    }

    status = execute_command(
        command,
        ">",
        2000U);

    if (status != ESP01_MODEM_STATUS_OK)
        return status;

    /*
     * CIPSEND echo와 prompt를 제거한다.
     * UDP packet queue는 제거하지 않는다.
     */
    control_reset();

    if (!esp01_uart_write(
            data,
            length,
            3000U))
    {
        return ESP01_MODEM_STATUS_UART_FAILURE;
    }

    /*
     * 대기 중 들어오는 +IPD는 parser가 packet queue에 저장한다.
     */
    return wait_for_response(
        "SEND OK",
        5000U);
}

esp01_modem_status_t esp01_modem_receive_udp(
    uint8_t link_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *length)
{
    udp_packet_t *packet;

    if (link_id >= ESP01_MODEM_MAX_LINKS ||
        buffer == NULL ||
        length == NULL)
    {
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;
    }

    if (queue_count[link_id] == 0U)
        return ESP01_MODEM_STATUS_NO_PACKET;

    packet = &packet_queue[link_id]
                         [queue_head[link_id]];

    if (capacity < packet->length)
        return ESP01_MODEM_STATUS_PACKET_TOO_LARGE;

    *length = packet->length;

    memcpy(
        buffer,
        packet->data,
        packet->length);

    queue_head[link_id] =
        (uint8_t)((queue_head[link_id] + 1U) %
                  PACKET_QUEUE_DEPTH);

    queue_count[link_id]--;

    return ESP01_MODEM_STATUS_OK;
}

esp01_modem_status_t esp01_modem_join_wifi(
    const char *ssid,
    const char *password)
{
    char command[192];
    int command_length;
    esp01_modem_status_t status;

    if (ssid == NULL || password == NULL)
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;

    status = execute_command(
        "AT+CWMODE_CUR=1\r\n",
        "OK",
        1000U);

    if (status != ESP01_MODEM_STATUS_OK)
        return status;

    command_length = snprintf(
        command,
        sizeof(command),
        "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n",
        ssid,
        password);

    if (command_length <= 0 ||
        (size_t)command_length >= sizeof(command))
    {
        return ESP01_MODEM_STATUS_INVALID_ARGUMENT;
    }

    status = execute_command(
        command,
        "OK",
        25000U);

    /*
     * 명령 echo에 Wi-Fi 비밀번호가 있으므로
     * 사용 후 command와 control buffer를 지운다.
     */
    memset(command, 0, sizeof(command));
    memset(control_buffer, 0, sizeof(control_buffer));
    control_length = 0U;

    return status;
}

esp01_modem_status_t esp01_modem_reset(void)
{
    esp01_modem_status_t status;

    status = execute_command(
        "AT+RST\r\n",
        "OK",
        1000U);

    if (status != ESP01_MODEM_STATUS_OK)
        return status;

    HAL_Delay(3000U);

    /* ESP 부트 로그를 parser가 소비하도록 한다. */
    esp01_modem_poll();
    control_reset();

    status = execute_command(
        "AT\r\n",
        "OK",
        1000U);

    if (status != ESP01_MODEM_STATUS_OK)
        return status;

    status = execute_command(
        "AT+CIPMUX=1\r\n",
        "OK",
        1000U);

    if (status != ESP01_MODEM_STATUS_OK)
        return status;

    /*
     * +IPD,<link>,<length>:<binary> 형식을 고정한다.
     */
    return execute_command(
        "AT+CIPDINFO=0\r\n",
        "OK",
        1000U);
}

uint32_t esp01_modem_dropped_packet_count(void)
{
    return dropped_packet_count;
}