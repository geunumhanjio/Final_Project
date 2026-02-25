#!/usr/bin/env python3
"""
navigation.launch.py
====================
기존 맵을 사용한 Localization + Nav2 모드

사용법:
  ros2 launch wsl_bringup navigation.launch.py
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
                    FindPackageShare('robot_navigation'),
                    'launch',
                    'localization.launch.py',
                ])
            ]),
        ),
    ])
