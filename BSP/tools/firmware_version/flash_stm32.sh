#!/bin/bash
# =============================================================
#  flash_stm32.sh
#  ROS Robot Driver — STM32 Firmware Flash Utility
#
#  Usage:
#    ./flash_stm32.sh                          # 기본 파일명 사용
#    ./flash_stm32.sh ROS_Robot_Driver.bin     # 파일 직접 지정
# =============================================================

set -e

FIRMWARE=${1:-"ROS_Robot_Driver.bin"}
FLASH_ADDR="0x08000000"

print_header() {
    echo "========================================"
    echo "  STM32 Firmware Flash Utility"
    echo "  Target: STM32F401RE (NUCLEO-F401RE)"
    echo "========================================"
    echo ""
}

print_header

# ── 1. 펌웨어 파일 확인 ───────────────────────────────────────
if [ ! -f "$FIRMWARE" ]; then
    echo "[ERROR] 펌웨어 파일을 찾을 수 없습니다: $FIRMWARE"
    echo "        사용법: $0 <firmware.bin>"
    exit 1
fi

FILESIZE=$(wc -c < "$FIRMWARE")
echo "[INFO]  Firmware : $FIRMWARE ($FILESIZE bytes)"
echo "[INFO]  Address  : $FLASH_ADDR"
echo ""

# ── 2. stlink-tools 설치 확인 ────────────────────────────────
if ! command -v st-flash &>/dev/null; then
    echo "[ERROR] stlink-tools 가 설치되어 있지 않습니다."
    echo ""
    echo "  설치 명령어:"
    echo "    sudo apt update && sudo apt install stlink-tools"
    echo ""
    exit 1
fi

# ── 3. ST-Link 연결 확인 ─────────────────────────────────────
echo "[STEP 1/2] ST-Link 장치 확인..."
PROBE_OUTPUT=$(st-info --probe 2>&1)
echo "$PROBE_OUTPUT"

if ! echo "$PROBE_OUTPUT" | grep -qi "serial"; then
    echo ""
    echo "[ERROR] ST-Link 장치를 찾을 수 없습니다."
    echo "        NUCLEO 보드가 USB로 연결되어 있는지 확인하세요."
    exit 1
fi
echo ""

# ── 4. 플래시 ────────────────────────────────────────────────
echo "[STEP 2/2] 펌웨어 업로드 중..."
st-flash --reset write "$FIRMWARE" "$FLASH_ADDR"

echo ""
echo "========================================"
echo "  [OK] 플래시 완료!"
echo "  STM32가 새 펌웨어로 부팅 중입니다."
echo "========================================"
