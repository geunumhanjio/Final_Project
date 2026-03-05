#!/usr/bin/env python3
import subprocess
import sys

RPI_USER = "iam"
RPI_IP   = "192.168.0.28"          # 여기에 라즈베리파이 IP 입력
RPI_DEST = "~/"

if not RPI_IP:
    print("RPI_IP를 설정해주세요.")
    sys.exit(1)

subprocess.run([
    "scp", "-r",
    "tools/firmware_release_and_rpi_utillities",
    f"{RPI_USER}@{RPI_IP}:{RPI_DEST}"
], check=True)
