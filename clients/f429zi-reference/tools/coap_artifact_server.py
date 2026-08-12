#!/usr/bin/env python3
"""Minimal CoAP Block2 artifact server for the F429ZI OTA E2E test only."""

import argparse
import math
import socket
from pathlib import Path


def decode_extended(nibble, packet, cursor):
    if nibble < 13:
        return nibble, cursor
    if nibble == 13:
        if cursor >= len(packet):
            raise ValueError("truncated option")
        return packet[cursor] + 13, cursor + 1
    if nibble == 14:
        if cursor + 2 > len(packet):
            raise ValueError("truncated option")
        return int.from_bytes(packet[cursor:cursor + 2], "big") + 269, cursor + 2
    raise ValueError("reserved option encoding")


def parse_request(packet):
    if len(packet) < 4:
        raise ValueError("short CoAP packet")

    version = packet[0] >> 6
    message_type = (packet[0] >> 4) & 0x03
    token_length = packet[0] & 0x0F
    code = packet[1]
    message_id = int.from_bytes(packet[2:4], "big")
    if version != 1 or token_length > 8 or len(packet) < 4 + token_length:
        raise ValueError("invalid CoAP header")

    token = packet[4:4 + token_length]
    cursor = 4 + token_length
    option_number = 0
    path_segments = []
    block_number = 0
    block_size = 512

    while cursor < len(packet) and packet[cursor] != 0xFF:
        first = packet[cursor]
        cursor += 1
        delta, cursor = decode_extended(first >> 4, packet, cursor)
        length, cursor = decode_extended(first & 0x0F, packet, cursor)
        option_number += delta
        if cursor + length > len(packet):
            raise ValueError("truncated option value")
        value = packet[cursor:cursor + length]
        cursor += length

        if option_number == 11:
            path_segments.append(value.decode("utf-8"))
        elif option_number == 23:
            block_value = int.from_bytes(value, "big") if value else 0
            szx = block_value & 0x07
            if szx > 6:
                raise ValueError("invalid Block2 SZX")
            block_number = block_value >> 4
            block_size = 1 << (szx + 4)

    return {
        "type": message_type,
        "code": code,
        "mid": message_id,
        "token": token,
        "path": "/".join(path_segments),
        "block_number": block_number,
        "block_size": block_size,
    }


def encode_extended(value):
    if value < 13:
        return value, b""
    if value < 269:
        return 13, bytes([value - 13])
    if value < 65805:
        return 14, (value - 269).to_bytes(2, "big")
    raise ValueError("option value too large")


def encode_uint(value):
    if value == 0:
        return b""
    return value.to_bytes((value.bit_length() + 7) // 8, "big")


def encode_option(previous_number, number, value):
    delta_nibble, delta_extra = encode_extended(number - previous_number)
    length_nibble, length_extra = encode_extended(len(value))
    header = bytes([(delta_nibble << 4) | length_nibble])
    return header + delta_extra + length_extra + value


def response_header(request, code):
    response_type = 2 if request["type"] == 0 else 1
    first = 0x40 | (response_type << 4) | len(request["token"])
    return bytes([first, code]) + request["mid"].to_bytes(2, "big") + request["token"]


def build_response(request, artifact, expected_path, max_block_size):
    if request["code"] != 1 or request["path"] != expected_path:
        return response_header(request, 0x84), None

    block_size = min(request["block_size"], max_block_size)
    block_number = request["block_number"]
    offset = block_number * block_size
    if offset >= len(artifact):
        return response_header(request, 0x82), None

    payload = artifact[offset:offset + block_size]
    more = offset + len(payload) < len(artifact)
    szx = int(math.log2(block_size)) - 4
    block_value = (block_number << 4) | (int(more) << 3) | szx

    options = encode_option(0, 23, encode_uint(block_value))
    options += encode_option(23, 28, encode_uint(len(artifact)))
    response = response_header(request, 0x45) + options + b"\xFF" + payload
    return response, (block_number, offset, len(payload), more)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5685)
    parser.add_argument("--path", default="firmware.bin")
    parser.add_argument("--block-size", type=int, default=512,
                        choices=(16, 32, 64, 128, 256, 512, 1024))
    args = parser.parse_args()

    artifact = args.file.read_bytes()
    if not artifact:
        raise SystemExit("artifact must not be empty")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.bind, args.port))
    print(f"Serving {args.file} ({len(artifact)} bytes) at "
          f"coap://{args.bind}:{args.port}/{args.path}", flush=True)

    try:
        while True:
            packet, peer = sock.recvfrom(1500)
            try:
                request = parse_request(packet)
                response, progress = build_response(
                    request, artifact, args.path.strip("/"), args.block_size)
                sock.sendto(response, peer)
                if progress is not None:
                    block_number, offset, length, more = progress
                    print(f"{peer[0]} block={block_number} offset={offset} "
                          f"bytes={length} more={int(more)}", flush=True)
            except (ValueError, UnicodeDecodeError) as error:
                print(f"Ignored malformed packet from {peer}: {error}",
                      flush=True)
    except KeyboardInterrupt:
        print("Stopped", flush=True)


if __name__ == "__main__":
    main()
