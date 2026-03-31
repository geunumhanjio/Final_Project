#!/usr/bin/env python3
"""
robot_core.launch.py
====================
robot_description (URDF/TF) + EKF localization 단독 런처

사용법:
  ros2 launch wsl_bringup robot_core.launch.py
"""

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution


def generate_launch_description():
    return LaunchDescription([
        # robot_state_publisher (URDF → TF)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('robot_description'),
                    'launch',
                    'robot_state_publisher.launch.py',
                ])
            ]),
        ),

        # EKF (odometry + IMU fusion)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('robot_localization_config'),
                    'launch',
                    'ekf.launch.py',
                ])
            ]),
        ),
    ])
