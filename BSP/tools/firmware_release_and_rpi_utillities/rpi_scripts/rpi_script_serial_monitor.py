#!/usr/bin/env python3
"""STM32 -> RPi 패킷 모니터 (USART1, 115200 baud)

사용법:
  python3 rpi_serial_monitor.py [시리얼포트]

예시:
  python3 rpi_serial_monitor.py              # 기본: /dev/serial0
  python3 rpi_serial_monitor.py /dev/ttyAMA0
"""

import sys
import struct
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/serial0"
BAUD = 115200

HEADER1 = 0xFF
HEADER2 = 0xFE

RSP_NAMES = {
    0x81: "RSP_ODOM",
    0x82: "RSP_IMU",
}


def parse_odom(data):
    left, right = struct.unpack_from("<hh", bytes(data))
    return f"Left={left:6d}  Right={right:6d}"


def parse_imu(data):
    ax, ay, az, gx, gy, gz = struct.unpack_from("<hhhhhh", bytes(data))
    return f"AX={ax:6d} AY={ay:6d} AZ={az:6d} GX={gx:6d} GY={gy:6d} GZ={gz:6d}"


PARSERS = {
    0x81: parse_odom,
    0x82: parse_imu,
}


def main():
    print(f"Opening {PORT} @ {BAUD} baud...")
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print("Listening for STM32 packets (Ctrl+C to quit)\n")

    # State machine (mirrors STM32 parser)
    STATE_H1, STATE_H2, STATE_CMD, STATE_LEN, STATE_DATA, STATE_CHK = range(6)
    state = STATE_H1
    cmd = 0
    length = 0
    data = []
    checksum = 0
    pkt_count = 0

    try:
        while True:
            raw = ser.read(1)
            if not raw:
                continue
            b = raw[0]

            if state == STATE_H1:
                if b == HEADER1:
                    state = STATE_H2
            elif state == STATE_H2:
                if b == HEADER2:
                    state = STATE_CMD
                elif b == HEADER1:
                    pass  # stay
                else:
                    state = STATE_H1
            elif state == STATE_CMD:
                cmd = b
                checksum = b
                state = STATE_LEN
            elif state == STATE_LEN:
                length = b
                checksum ^= b
                data = []
                if length > 12:
                    print(f"  [ERR] invalid len={length}")
                    state = STATE_H1
                elif length == 0:
                    state = STATE_CHK
                else:
                    state = STATE_DATA
            elif state == STATE_DATA:
                data.append(b)
                checksum ^= b
                if len(data) >= length:
                    state = STATE_CHK
            elif state == STATE_CHK:
                pkt_count += 1
                name = RSP_NAMES.get(cmd, f"0x{cmd:02X}")
                if checksum == b:
                    detail = ""
                    parser = PARSERS.get(cmd)
                    if parser and len(data) >= length:
                        detail = "  " + parser(data)
                    hex_str = " ".join(f"{x:02X}" for x in data)
                    print(f"[{pkt_count:4d}] {name:<10s} LEN={length:2d}  [{hex_str}]{detail}")
                else:
                    print(f"[{pkt_count:4d}] {name:<10s} CHECKSUM ERROR (got 0x{b:02X}, expected 0x{checksum:02X})")
                state = STATE_H1

    except KeyboardInterrupt:
        print(f"\nStopped. Total packets: {pkt_count}")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
