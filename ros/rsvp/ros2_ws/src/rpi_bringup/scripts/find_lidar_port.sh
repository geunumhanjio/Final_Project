#!/bin/bash
# find_lidar_port.sh
# ==================
# YDLiDAR X4 USB 포트 자동 감지 스크립트
#
# 사용법:
#   chmod +x find_lidar_port.sh
#   ./find_lidar_port.sh
#
# udev rule 설정으로 포트 영구 고정하기:
#   ./find_lidar_port.sh --setup-udev

set -e

YDLIDAR_VENDOR="10c4"   # Silicon Labs CP210x (YDLiDAR X4 기본 칩)
YDLIDAR_PRODUCT="ea60"

# ── 현재 연결된 포트 감지 ──────────────────────────────────────────
echo "=== YDLiDAR 포트 탐색 중... ==="
echo ""

FOUND_PORT=""

for dev in /dev/ttyUSB*; do
    if [ ! -e "$dev" ]; then
        echo "  USB 시리얼 장치 없음"
        break
    fi

    VENDOR=$(udevadm info -a -n "$dev" 2>/dev/null | grep -m1 'idVendor' | grep -o '"[^"]*"' | tr -d '"' || true)
    PRODUCT=$(udevadm info -a -n "$dev" 2>/dev/null | grep -m1 'idProduct' | grep -o '"[^"]*"' | tr -d '"' || true)

    echo "  $dev  vendor=$VENDOR  product=$PRODUCT"

    if [ "$VENDOR" = "$YDLIDAR_VENDOR" ] && [ "$PRODUCT" = "$YDLIDAR_PRODUCT" ]; then
        FOUND_PORT="$dev"
        echo "  ✅ YDLiDAR X4 감지됨: $dev"
    fi
done

echo ""

if [ -z "$FOUND_PORT" ]; then
    echo "❌ YDLiDAR를 찾을 수 없습니다."
    echo "   - USB 케이블 연결 확인"
    echo "   - 다른 vendor/product ID인 경우: udevadm info -a -n /dev/ttyUSBx 로 직접 확인"
    exit 1
fi

# ── udev rule 설정 ────────────────────────────────────────────────
if [ "$1" = "--setup-udev" ]; then
    echo "=== udev rule 설정 ==="
    UDEV_RULE="SUBSYSTEM==\"tty\", ATTRS{idVendor}==\"${YDLIDAR_VENDOR}\", ATTRS{idProduct}==\"${YDLIDAR_PRODUCT}\", SYMLINK+=\"ydlidar\", MODE=\"0666\""
    UDEV_FILE="/etc/udev/rules.d/99-ydlidar.rules"

    echo "  Rule: $UDEV_RULE"
    echo "$UDEV_RULE" | sudo tee "$UDEV_FILE"
    sudo udevadm control --reload-rules
    sudo udevadm trigger

    echo ""
    echo "✅ udev rule 설정 완료!"
    echo "   /dev/ydlidar 심볼릭 링크가 생성됩니다."
    echo ""
    echo "   이후 ydlidar_params.yaml 에서:"
    echo "   port: \"/dev/ydlidar\"  으로 고정하세요."
    echo ""
    echo "   또는 런치 시:"
    echo "   ros2 launch rpi_bringup rpi_bringup.launch.py lidar_port:=/dev/ydlidar"
else
    echo "=== 감지된 포트로 실행하기 ==="
    echo ""
    echo "  ros2 launch rpi_bringup rpi_bringup.launch.py lidar_port:=${FOUND_PORT}"
    echo "  ros2 launch rpi_bringup lidar.launch.py lidar_port:=${FOUND_PORT}"
    echo ""
    echo "  포트를 영구 고정하려면:"
    echo "  $0 --setup-udev"
fi
