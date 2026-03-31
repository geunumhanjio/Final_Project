#!/usr/bin/env python3
"""
websocket_bridge.launch.py
==========================
WebSocket 브릿지 단독 런처

사용법:
  ros2 launch wsl_bringup websocket_bridge.launch.py

파라미터 변경 (yaml):
  WSL/src/websocket_bridge/config/websocket_bridge_params.yaml
    odom_source:           "slam" | "odom"
    odom_publish_mode:     "timer" | "topic"
    odom_publish_interval: 0.1  (초)
"""

from launch import LaunchDescription
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution


def generate_launch_description():
    params_file = PathJoinSubstitution([
        FindPackageShare('websocket_bridge'),
        'config',
        'websocket_bridge_params.yaml',
    ])

    return LaunchDescription([
        Node(
            package='websocket_bridge',
            executable='websocket_bridge_node',
            name='websocket_bridge_node',
            output='screen',
            parameters=[params_file],
        ),
    ])
