from __future__ import annotations

import struct
import sys
import time
from urllib import request

import serial


SOF = b"\xAA\x55"

CMD_HELLO = 0x01
CMD_ACK = 0x79


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF

    for value in data:
        crc ^= value << 8

        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF

    return crc


def build_packet(command: int, payload: bytes = b"") -> bytes:
    body = (
        bytes([command])
        + struct.pack("<H", len(payload))
        + payload
    )

    crc = crc16_ccitt_false(body)

    return SOF + body + struct.pack("<H", crc)


def read_exact(
    port: serial.Serial,
    size: int,
) -> bytes:
    received = bytearray()

    while len(received) < size:
        chunk = port.read(size - len(received))

        if not chunk:
            raise TimeoutError(
                f"{size}바이트 중 "
                f"{len(received)}바이트만 수신했습니다."
            )

        received.extend(chunk)

    return bytes(received)


def read_packet(
    port: serial.Serial,
) -> tuple[int, bytes]:
    header = read_exact(port, 5)

    if header[0:2] != SOF:
        raise ValueError(
            f"잘못된 SOF: {header[0:2].hex(' ')}"
        )

    command = header[2]

    payload_length = struct.unpack(
        "<H",
        header[3:5],
    )[0]

    payload_and_crc = read_exact(
        port,
        payload_length + 2,
    )

    payload = payload_and_crc[:payload_length]

    received_crc = struct.unpack(
        "<H",
        payload_and_crc[payload_length:],
    )[0]

    crc_body = (
        bytes([command])
        + struct.pack("<H", payload_length)
        + payload
    )

    calculated_crc = crc16_ccitt_false(crc_body)

    if received_crc != calculated_crc:
        raise ValueError(
            "응답 CRC 오류: "
            f"received=0x{received_crc:04X}, "
            f"calculated=0x{calculated_crc:04X}"
        )

    return command, payload


def main() -> int:
    if len(sys.argv) != 2:
        print(
            "사용법: "
            "python protocol_hello_test.py COM5"
        )

        return 1

    port_name = sys.argv[1]

    try:
        with serial.Serial(
            port=port_name,
            baudrate=115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1.0,
        ) as port:
            time.sleep(0.2)

            port.reset_input_buffer()

            # 텍스트 모드에서 바이너리 프로토콜 모드로 전환
            port.write(b"PROTO\r\n")
            port.flush()

            time.sleep(0.1)

            text_response = port.read_all()

            if text_response:
                print(
                    text_response.decode(
                        "utf-8",
                        errors="replace",
                    ).rstrip()
                )

            port.reset_input_buffer()

            request = build_packet(CMD_HELLO)

            print(f"TX: {request.hex(' ')}")

            # CRC test
            # request = bytearray(build_packet(CMD_HELLO))
            # request[-1] ^= 0xFF

            port.write(request)
            port.flush()

            response_command, payload = read_packet(port)

            print(
                "RX: "
                f"command=0x{response_command:02X}, "
                f"payload={payload.hex(' ')}"
            )

            if response_command != CMD_ACK:
                print("ACK 명령이 아닙니다.")

                return 3

            if payload != bytes([CMD_HELLO, 0x00]):
                print("ACK Payload가 예상과 다릅니다.")

                return 4

            print("바이너리 HELLO/ACK 테스트 성공")

            return 0

    except (
        serial.SerialException,
        TimeoutError,
        ValueError,
    ) as error:
        print(f"테스트 실패: {error}")

        return 2


if __name__ == "__main__":
    raise SystemExit(main())