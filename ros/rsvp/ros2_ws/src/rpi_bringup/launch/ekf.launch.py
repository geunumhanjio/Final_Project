#!/usr/bin/env python3
"""
ekf.launch.py (RPi)

wheel_odom + IMU → EKF → /odom 발행 (RPi에서 로컬 처리)
publish_tf: false → WSL scan_relay가 타임스탬프 보정 후 TF 발행
"""

from launch import LaunchDescription
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution


def generate_launch_description():

    ekf_config = PathJoinSubstitution([
        FindPackageShare('rpi_bringup'), 'config', 'ekf_rpi.yaml'
    ])

    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[ekf_config],
        remappings=[
            ('odometry/filtered', '/odom'),
        ]
    )

    return LaunchDescription([ekf_node])
