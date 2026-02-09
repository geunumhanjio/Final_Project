#!/bin/bash
set -e

# ROS2 환경 로드
source /opt/ros/humble/setup.bash

# 워크스페이스 빌드가 있으면 로드
if [ -f /root/ros2_ws/install/setup.bash ]; then
    source /root/ros2_ws/install/setup.bash
fi

# USB 장치 권한 설정
if [ -e /dev/ttyUSB0 ]; then
    chmod 666 /dev/ttyUSB0 || true
fi

if [ -e /dev/ttyUSB1 ]; then
    chmod 666 /dev/ttyUSB1 || true
fi

if [ -e /dev/video0 ]; then
    chmod 666 /dev/video0 || true
fi

echo "================================================"
echo "🍓 Raspberry Pi ROS2 Container Ready!"
echo "================================================"
echo "ROS_DOMAIN_ID: $ROS_DOMAIN_ID"
echo "RMW_IMPLEMENTATION: $RMW_IMPLEMENTATION"
echo "================================================"

exec "$@"
