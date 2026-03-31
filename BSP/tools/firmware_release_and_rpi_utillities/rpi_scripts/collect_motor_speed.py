#!/usr/bin/env python3
"""STM32 모터 속도 수집기 (USART1, 115200 baud)

RSP_ODOM 패킷만 골라 텍스트 파일로 저장.

사용법:
  python3 collect_motor_speed.py [시리얼포트] [출력파일]

예시:
  python3 collect_motor_speed.py                              # 기본값
  python3 collect_motor_speed.py /dev/ttyAMA0 speed_log.txt
"""

import sys
import struct
import time
import serial
from datetime import datetime

PORT    = sys.argv[1] if len(sys.argv) > 1 else "/dev/serial0"
OUTFILE = sys.argv[2] if len(sys.argv) > 2 else f"motor_speed_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
BAUD    = 115200

HEADER1  = 0xFF
HEADER2  = 0xFE
RSP_ODOM = 0x81


def parse_odom(data):
    left, right = struct.unpack_from("<hh", bytes(data))
    return left, right


def main():
    print(f"Opening {PORT} @ {BAUD} baud...")
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"저장 파일: {OUTFILE}")
    print("Listening for RSP_ODOM packets (Ctrl+C to quit)\n")

    STATE_H1, STATE_H2, STATE_CMD, STATE_LEN, STATE_DATA, STATE_CHK = range(6)
    state    = STATE_H1
    cmd      = 0
    length   = 0
    data     = []
    checksum = 0
    count    = 0
    start    = time.time()

    with open(OUTFILE, "w") as f:
        f.write("elapsed_s\tleft_mmps\tright_mmps\n")

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
                        pass
                    else:
                        state = STATE_H1

                elif state == STATE_CMD:
                    cmd      = b
                    checksum = b
                    state    = STATE_LEN

                elif state == STATE_LEN:
                    length    = b
                    checksum ^= b
                    data      = []
                    if length > 12:
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
                    if checksum == b and cmd == RSP_ODOM:
                        left, right = parse_odom(data)
                        elapsed = time.time() - start
                        count += 1
                        line = f"{elapsed:.4f}\t{left}\t{right}\n"
                        f.write(line)
                        f.flush()
                        print(f"[{count:4d}] {elapsed:8.3f}s  L={left:6d} mm/s  R={right:6d} mm/s", end="\r")
                    state = STATE_H1

        except KeyboardInterrupt:
            print(f"\n종료. 수집된 패킷: {count}개 → {OUTFILE}")

    ser.close()


if __name__ == "__main__":
    main()
