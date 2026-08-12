#ifndef ESP01_MODEM_H
#define ESP01_MODEM_H

#include <stddef.h>
#include <stdint.h>

#define ESP01_MODEM_MAX_LINKS       5U
#define ESP01_MODEM_MAX_UDP_PAYLOAD 2048U

typedef enum
{
    ESP01_MODEM_STATUS_OK = 0,
    ESP01_MODEM_STATUS_INVALID_ARGUMENT,
    ESP01_MODEM_STATUS_UART_FAILURE,
    ESP01_MODEM_STATUS_TIMEOUT,
    ESP01_MODEM_STATUS_AT_ERROR,
    ESP01_MODEM_STATUS_NO_PACKET,
    ESP01_MODEM_STATUS_PACKET_TOO_LARGE
} esp01_modem_status_t;

void esp01_modem_init(void);

esp01_modem_status_t esp01_modem_reset(void);

esp01_modem_status_t esp01_modem_join_wifi(
    const char *ssid,
    const char *password);

esp01_modem_status_t esp01_modem_open_udp(
    uint8_t link_id,
    const char *remote_host,
    uint16_t remote_port,
    uint16_t local_port);

esp01_modem_status_t esp01_modem_close(
    uint8_t link_id);

esp01_modem_status_t esp01_modem_send_udp(
    uint8_t link_id,
    const uint8_t *data,
    size_t length);

/*
 * UART ring buffer의 AT 응답과 +IPD packet을 분리한다.
 * main loop에서 반복 호출한다.
 */
void esp01_modem_poll(void);

/*
 * 완성된 UDP datagram 하나를 가져온다.
 * packet이 없으면 ESP01_MODEM_STATUS_NO_PACKET을 반환한다.
 */
esp01_modem_status_t esp01_modem_receive_udp(
    uint8_t link_id,
    uint8_t *buffer,
    size_t capacity,
    size_t *length);

uint32_t esp01_modem_dropped_packet_count(void);

#endif