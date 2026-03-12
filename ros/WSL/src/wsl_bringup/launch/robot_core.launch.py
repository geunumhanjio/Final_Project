#!/usr/bin/env python3
"""
robot_core.launch.py
====================
robot_description (URDF/TF) + EKF localization 단독 런처

사용법:
  # WSL EKF 활성화 (기본, RPi EKF 미사용 시)
  ros2 launch wsl_bringup robot_core.launch.py

  # WSL EKF 비활성화 (RPi EKF 사용 시)
  ros2 launch wsl_bringup robot_core.launch.py use_ekf:=false
"""

from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution, LaunchConfiguration


def generate_launch_description():
    use_ekf_arg = DeclareLaunchArgument(
        'use_ekf',
        default_value='true',
        description='Enable WSL-side EKF. Set false when EKF runs on RPi.'
    )

    return LaunchDescription([
        use_ekf_arg,

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

        # EKF (odometry + IMU fusion) - RPi EKF 사용 시 비활성화
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('robot_localization_config'),
                    'launch',
                    'ekf.launch.py',
                ])
            ]),
            condition=IfCondition(LaunchConfiguration('use_ekf')),
        ),
    ])
