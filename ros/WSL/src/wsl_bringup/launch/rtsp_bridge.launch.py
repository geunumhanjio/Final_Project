#!/usr/bin/env python3
"""
rtsp_bridge.launch.py
=====================
RTSP 브릿지 단독 런처

사용법:
  ros2 launch wsl_bringup rtsp_bridge.launch.py
"""

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution


def generate_launch_description():
    return LaunchDescription([
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('rtsp_bridge'),
                    'launch',
                    'rtsp_publisher.launch.py',
                ])
            ]),
        )
    ])
