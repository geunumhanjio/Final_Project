#!/usr/bin/env python3
"""
navigation.launch.py
====================
기존 맵을 사용한 Localization + Nav2 모드

실행 순서:
  1. SLAM Toolbox localization (map → odom TF 발행)
  2. Nav2 스택 (global/local planner, controller, behaviors)

사용법:
  ros2 launch wsl_bringup navigation.launch.py
"""

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution


def generate_launch_description():
    robot_navigation = FindPackageShare('robot_navigation')

    localization = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([robot_navigation, 'launch', 'localization.launch.py'])
        ]),
    )

    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([robot_navigation, 'launch', 'nav2.launch.py'])
        ]),
    )

    return LaunchDescription([
        localization,
        nav2,
    ])
