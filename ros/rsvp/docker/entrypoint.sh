#!/bin/bash
set -e

source /opt/ros/humble/setup.bash

if [ -f /root/ros2_ws/install/setup.bash ]; then
    source /root/ros2_ws/install/setup.bash
fi

# USB 장치 권한
echo "Setting up device permissions..."
for device in /dev/ttyUSB* /dev/ttyACM* /dev/video*; do
    if [ -e "$device" ]; then
        chmod 666 "$device" 2>/dev/null || true
        echo "  ✓ $device"
    fi
done

echo "================================================"
echo "🍓 Raspberry Pi ROS2 Container Ready!"
echo "ROS_DOMAIN_ID: $ROS_DOMAIN_ID"
echo "RMW_IMPLEMENTATION: $RMW_IMPLEMENTATION"
echo "Network: $(hostname -I)"
echo "================================================"

exec "$@"
