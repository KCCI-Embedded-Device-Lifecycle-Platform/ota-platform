#include "boot_protocol.h"

#include <stddef.h>
#include <string.h>

#include "Middleware/boot_crc16.h"

// inners
static void BootProtocol_ResetFrameState(boot_protocol_parser_t *parser) {
    uint8_t last_command;

    if (parser == NULL) return;

    // 오류 응답에서 사용할 수 있도록 마지막 Command 유지
    last_command = parser->last_command;

    memset(parser, 0, sizeof(*parser));

    parser->state = BOOT_PROTOCOL_STATE_WAIT_SOF_1;

    parser->calculated_crc = BOOT_CRC16_INITIAL_VALUE;

    parser->last_command = last_command;
}

// functions
void BootProtocol_Init(boot_protocol_parser_t *parser) {
    // Parser 객체 초기화
    if (parser == NULL) return;

    memset(parser, 0, sizeof(*parser));

    parser->state = BOOT_PROTOCOL_STATE_WAIT_SOF_1;

    parser->calculated_crc = BOOT_CRC16_INITIAL_VALUE;
}

boot_protocol_result_t BootProtocol_ProcessByte(boot_protocol_parser_t *parser, uint8_t received_byte, boot_protocol_packet_t *output_packet) {
    // 수신 바이트 하나를 Parser에 전달
    if ((parser == NULL) || (output_packet == NULL)) {
        if (parser != NULL) BootProtocol_ResetFrameState(parser);

        return BOOT_PROTOCOL_RESULT_FORMAT_ERROR;
    }

    switch (parser->state) {
        case BOOT_PROTOCOL_STATE_WAIT_SOF_1: {
            if (received_byte == BOOT_PROTOCOL_SOF_1) parser->state = BOOT_PROTOCOL_STATE_WAIT_SOF_2;

            break;
        }

        case BOOT_PROTOCOL_STATE_WAIT_SOF_2: {
            if (received_byte == BOOT_PROTOCOL_SOF_2) {
                parser->state = BOOT_PROTOCOL_STATE_COMMAND;
            } 
            else if (received_byte == BOOT_PROTOCOL_SOF_1) {
                // AA AA 55 입력에서 두 번째 AA를 새로운 SOF1로 처리
                parser->state = BOOT_PROTOCOL_STATE_WAIT_SOF_2;
            }
            else {
                BootProtocol_ResetFrameState(parser);
            }

            break;
        }

        case BOOT_PROTOCOL_STATE_COMMAND: {
            parser->packet.command = received_byte;
            parser->last_command = received_byte;
            parser->calculated_crc = BOOT_CRC16_INITIAL_VALUE;
            parser->calculated_crc = BootCrc16_Update(parser->calculated_crc, received_byte);
            parser->state = BOOT_PROTOCOL_STATE_LENGTH_LOW;

            break;
        }

        case BOOT_PROTOCOL_STATE_LENGTH_LOW: {
            parser->packet.length = (uint16_t)received_byte;
            parser->calculated_crc = BootCrc16_Update(parser->calculated_crc, received_byte);
            parser->state = BOOT_PROTOCOL_STATE_LENGTH_HIGH;

            break;
        }

        case BOOT_PROTOCOL_STATE_LENGTH_HIGH: {
            parser->packet.length |= (uint16_t)received_byte << 8U;
            parser->calculated_crc = BootCrc16_Update(parser->calculated_crc, received_byte);

            if (parser->packet.length > BOOT_PROTOCOL_MAX_DATA_SIZE) {
                BootProtocol_ResetFrameState(parser);

                return BOOT_PROTOCOL_RESULT_LENGTH_ERROR;
            }

            if (parser->packet.length == 0U) {
                parser->state = BOOT_PROTOCOL_STATE_CRC_LOW;
            }
            else {
                parser->data_index = 0U;
                parser->state = BOOT_PROTOCOL_STATE_DATA;
            }

            break;
        }

        case BOOT_PROTOCOL_STATE_DATA: {
            if (parser->data_index >= BOOT_PROTOCOL_MAX_DATA_SIZE) {
                BootProtocol_ResetFrameState(parser);

                return BOOT_PROTOCOL_RESULT_LENGTH_ERROR;
            }

            parser->packet.data[parser->data_index] = received_byte;
            parser->data_index++;
            parser->calculated_crc = BootCrc16_Update(parser->calculated_crc, received_byte);

            if (parser->data_index >= parser->packet.length) parser->state = BOOT_PROTOCOL_STATE_CRC_LOW;

            break;
        }

        case BOOT_PROTOCOL_STATE_CRC_LOW: {
            parser->received_crc = (uint16_t)received_byte;
            parser->state = BOOT_PROTOCOL_STATE_CRC_HIGH;

            break;
        }

        case BOOT_PROTOCOL_STATE_CRC_HIGH: {
            parser->received_crc |= (uint16_t)received_byte << 8U;

            if (parser->received_crc != parser->calculated_crc) {
                BootProtocol_ResetFrameState(parser);

                return BOOT_PROTOCOL_RESULT_CRC_ERROR;
            }

            *output_packet = parser->packet;

            BootProtocol_ResetFrameState(parser);

            return BOOT_PROTOCOL_RESULT_PACKET_READY;
        }

        default: {
            BootProtocol_ResetFrameState(parser);

            return BOOT_PROTOCOL_RESULT_FORMAT_ERROR;
        }
    }

    return BOOT_PROTOCOL_RESULT_NONE;
}

mw_status_t BootProtocol_Encode(const boot_protocol_packet_t *packet, uint8_t *output_frame, uint16_t output_capacity, uint16_t *output_length) {
    // Packet >> 전송 가능한 Frame으로 인코딩
    // Frame: | SOF1 | SOF2 | CMD | LEN_L | LEN_H | DATA | CRC_L | CRC_H |
    uint16_t frame_index;
    uint16_t required_length;
    uint16_t crc;
    uint16_t crc_data_length;

    if ((packet == NULL) || (output_frame == NULL) || (output_length == NULL)) return MW_STATUS_INVALID_ARGUMENT;
    if (packet->length > BOOT_PROTOCOL_MAX_DATA_SIZE) return MW_STATUS_INVALID_ARGUMENT;

    required_length = (uint16_t)(packet->length + BOOT_PROTOCOL_FRAME_OVERHEAD);

    if (output_capacity < required_length) return MW_STATUS_BUFFER_TOO_SMALL;

    frame_index = 0U;

    output_frame[frame_index++] = BOOT_PROTOCOL_SOF_1;
    output_frame[frame_index++] = BOOT_PROTOCOL_SOF_2;
    output_frame[frame_index++] = packet->command;
    output_frame[frame_index++] = (uint8_t)(packet->length & 0x00FFU);
    output_frame[frame_index++] = (uint8_t)((packet->length >> 8U) & 0x00FFU);

    if (packet->length > 0U) {
        memcpy(&output_frame[frame_index], packet->data, packet->length);

        frame_index += packet->length;
    }

    // CRC 대상: CMD + LENGTH 2바이트 + DATA
    // output_frame[2]부터 시작
    crc_data_length = (uint16_t)(1U + 2U + packet->length);

    crc = BootCrc16_Calculate(&output_frame[2], crc_data_length);

    output_frame[frame_index++] = (uint8_t)(crc & 0x00FFU);
    output_frame[frame_index++] = (uint8_t)((crc >> 8U) & 0x00FFU);

    *output_length = frame_index;

    return MW_STATUS_OK;
}

uint8_t BootProtocol_GetLastCommand(const boot_protocol_parser_t *parser) {
    // 파싱 도중 마지막으로 수신한 Command 반환
    // CRC 오류 응답 만들 때 사용
    if (parser == NULL) return 0U;

    return parser->last_command;
}