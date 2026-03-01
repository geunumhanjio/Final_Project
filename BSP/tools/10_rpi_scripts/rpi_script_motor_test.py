#!/usr/bin/env python3
"""RPi → STM32 모터 제어 테스트 (반복 전송 방식)

사용법:
  sudo python3 rpi_motor_test.py [시리얼포트]

예시:
  sudo python3 rpi_motor_test.py              # 기본: /dev/serial0
  sudo python3 rpi_motor_test.py /dev/ttyAMA0
"""

import sys
import serial
import struct
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/serial0"
BAUD = 115200
INTERVAL = 0.2  # 200ms 간격으로 반복 전송 (타임아웃 500ms 이내)
DURATION = 5.0   # 각 동작 5초

ser = serial.Serial(PORT, BAUD, timeout=1)


def send_packet(cmd, data=b""):
    length = len(data)
    checksum = cmd ^ length
    for b in data:
        checksum ^= b
    packet = bytes([0xFF, 0xFE, cmd, length]) + data + bytes([checksum])
    ser.write(packet)


def send_velocity(left_mm_s, right_mm_s):
    data = struct.pack("<hh", left_mm_s, right_mm_s)
    send_packet(0x01, data)


def run_for(left, right, seconds, label):
    print(f"== {label} ({seconds}s) ==")
    end_time = time.time() + seconds
    while time.time() < end_time:
        send_velocity(left, right)
        remaining = end_time - time.time()
        print(f"  L={left:+4d} R={right:+4d} mm/s  남은시간: {remaining:.1f}s", end="\r")
        time.sleep(INTERVAL)
    print()


def soft_stop(seconds=2):
    print(f"== 정지 대기 ({seconds}s) ==")
    send_packet(0x02)
    time.sleep(seconds)


# === 테스트 시나리오 ===
try:
    print(f"포트: {PORT} @ {BAUD} baud")
    print(f"동작시간: {DURATION}s, 전송간격: {INTERVAL}s\n")
    time.sleep(0.5)

    # 1) 전진
    run_for(300, 300, DURATION, "전진 300mm/s")
    soft_stop()

    # 2) 후진
    run_for(-300, -300, DURATION, "후진 300mm/s")
    soft_stop()

    # 3) 제자리 좌회전
    run_for(-300, 300, DURATION, "제자리 좌회전")
    soft_stop()

    # 4) 제자리 우회전
    run_for(300, -300, DURATION, "제자리 우회전")
    soft_stop()

    # 5) 비상정지 테스트
    print("== 전진 중 비상정지 테스트 ==")
    run_for(300, 300, 2, "전진 300mm/s (2초 후 비상정지)")
    print("  -> Emergency Stop!")
    send_packet(0x03)
    time.sleep(2)
    print("  -> Release")
    send_packet(0x04)
    time.sleep(1)

    print("\n테스트 완료!")

except KeyboardInterrupt:
    print("\n\n중단됨 -> 비상정지 전송")
    send_packet(0x03)

finally:
    ser.close()
