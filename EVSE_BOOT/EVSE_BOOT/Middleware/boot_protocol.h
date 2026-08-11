#ifndef EVSE_BOOT_MW__BOOT_PROTOCOL_H
#define EVSE_BOOT_MW__BOOT_PROTOCOL_H

#include <stdint.h>

#include "Middleware/mw_common.h"

// defines
#define BOOT_PROTOCOL_SOF_1           0xAAU
#define BOOT_PROTOCOL_SOF_2           0x55U

#define BOOT_PROTOCOL_MAX_DATA_SIZE   512U

    // SOF 2 + Command 1 + Length 2 + CRC16 2
#define BOOT_PROTOCOL_FRAME_OVERHEAD     7U

#define BOOT_PROTOCOL_MAX_FRAME_SIZE     (BOOT_PROTOCOL_MAX_DATA_SIZE + BOOT_PROTOCOL_FRAME_OVERHEAD)

// tables
typedef enum {
    BOOT_PROTOCOL_CMD_HELLO            = 0x01U,
    BOOT_PROTOCOL_CMD_GET_VERSION      = 0x02U,

    BOOT_PROTOCOL_CMD_START_UPDATE     = 0x10U,
    BOOT_PROTOCOL_CMD_DATA             = 0x11U,
    BOOT_PROTOCOL_CMD_END_UPDATE       = 0x12U,
    BOOT_PROTOCOL_CMD_ABORT            = 0x13U,

    BOOT_PROTOCOL_CMD_RUN_APP          = 0x20U,

    BOOT_PROTOCOL_CMD_ACK              = 0x79U,
    BOOT_PROTOCOL_CMD_NACK             = 0x1FU,
    BOOT_PROTOCOL_CMD_VERSION_RESPONSE = 0x82U
} boot_protocol_command_t;

typedef enum {
    BOOT_PROTOCOL_ERROR_NONE = 0x00U,
    BOOT_PROTOCOL_ERROR_CRC,
    BOOT_PROTOCOL_ERROR_LENGTH,
    BOOT_PROTOCOL_ERROR_FORMAT,
    BOOT_PROTOCOL_ERROR_UNSUPPORTED_COMMAND,
    BOOT_PROTOCOL_ERROR_INVALID_STATE
} boot_protocol_error_t;

typedef struct {
    uint8_t command;
    uint16_t length;
    uint8_t data[BOOT_PROTOCOL_MAX_DATA_SIZE];
} boot_protocol_packet_t;

typedef enum {
    BOOT_PROTOCOL_STATE_WAIT_SOF_1 = 0,
    BOOT_PROTOCOL_STATE_WAIT_SOF_2,
    BOOT_PROTOCOL_STATE_COMMAND,
    BOOT_PROTOCOL_STATE_LENGTH_LOW,
    BOOT_PROTOCOL_STATE_LENGTH_HIGH,
    BOOT_PROTOCOL_STATE_DATA,
    BOOT_PROTOCOL_STATE_CRC_LOW,
    BOOT_PROTOCOL_STATE_CRC_HIGH
} boot_protocol_state_t;

typedef struct {
    boot_protocol_state_t state;

    boot_protocol_packet_t packet;

    uint16_t data_index;
    uint16_t calculated_crc;
    uint16_t received_crc;

    uint8_t last_command;
} boot_protocol_parser_t;

typedef enum {
    BOOT_PROTOCOL_RESULT_NONE = 0,
    BOOT_PROTOCOL_RESULT_PACKET_READY,
    BOOT_PROTOCOL_RESULT_CRC_ERROR,
    BOOT_PROTOCOL_RESULT_LENGTH_ERROR,
    BOOT_PROTOCOL_RESULT_FORMAT_ERROR
} boot_protocol_result_t;

// functions
void BootProtocol_Init(boot_protocol_parser_t *parser);

boot_protocol_result_t \
BootProtocol_ProcessByte(boot_protocol_parser_t *parser, uint8_t received_byte, boot_protocol_packet_t *output_packet);

mw_status_t \
BootProtocol_Encode(const boot_protocol_packet_t *packet, uint8_t *output_frame, uint16_t output_capacity, uint16_t *output_length);

uint8_t BootProtocol_GetLastCommand(const boot_protocol_parser_t *parser);

#endif // EVSE_BOOT_MW__BOOT_PROTOCOL_H