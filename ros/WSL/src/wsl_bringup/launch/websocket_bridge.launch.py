#!/usr/bin/env python3
"""
websocket_bridge.launch.py
==========================
WebSocket 브릿지 단독 런처

사용법:
  ros2 launch wsl_bringup websocket_bridge.launch.py
"""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='websocket_bridge',
            executable='websocket_bridge_node',
            name='websocket_bridge_node',
            output='screen',
        ),
    ])
