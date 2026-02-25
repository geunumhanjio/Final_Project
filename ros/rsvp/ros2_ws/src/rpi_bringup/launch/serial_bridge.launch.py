#!/usr/bin/env python3
"""
serial_bridge.launch.py
========================
STM32 시리얼 브릿지 단독 런처

사용법:
  ros2 launch rpi_bringup serial_bridge.launch.py
  ros2 launch rpi_bringup serial_bridge.launch.py serial_port:=/dev/ttyAMA0
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_share = FindPackageShare('rpi_bringup')

    return LaunchDescription([
        DeclareLaunchArgument(
            'serial_port',
            default_value='/dev/serial0',
            description='STM32 serial port'
        ),

        Node(
            package='rpi_serial_bridge',
            executable='serial_bridge_node',
            name='serial_bridge_node',
            parameters=[
                PathJoinSubstitution([pkg_share, 'config', 'rpi_params.yaml']),
                {'serial_port': LaunchConfiguration('serial_port')},
            ],
            output='screen',
        ),
    ])
