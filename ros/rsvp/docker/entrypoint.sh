#!/bin/bash
set -e

# ROS2 환경 로드
source /opt/ros/humble/setup.bash

# 워크스페이스 빌드가 있으면 로드
if [ -f /root/ros2_ws/install/setup.bash ]; then
    source /root/ros2_ws/install/setup.bash
fi

# USB 장치 권한 설정
echo "Setting up device permissions..."
for device in /dev/ttyUSB* /dev/ttyACM* /dev/video*; do
    if [ -e "$device" ]; then
        chmod 666 "$device" 2>/dev/null || true
        echo "  ✓ $device"
    fi
done

# udev 규칙 적용 (있는 경우)
if [ -d /root/config/udev ]; then
    cp /root/config/udev/*.rules /etc/udev/rules.d/ 2>/dev/null || true
fi

echo "================================================"
echo "🍓 Raspberry Pi ROS2 Container"
echo "================================================"
echo "ROS_DOMAIN_ID: $ROS_DOMAIN_ID"
echo "ROS_LOCALHOST_ONLY: $ROS_LOCALHOST_ONLY"
echo "RMW_IMPLEMENTATION: $RMW_IMPLEMENTATION"
echo "Hostname: $(hostname)"
echo "IP Address: $(hostname -I)"
echo "================================================"

exec "$@"
