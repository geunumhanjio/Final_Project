#!/usr/bin/env python3
"""
slam_mapping.launch.py
=======================
SLAM Toolbox - 새 맵 생성 모드

사용법:
  ros2 launch wsl_bringup slam_mapping.launch.py
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
                    'slam.launch.py',
                ])
            ]),
        ),
    ])
