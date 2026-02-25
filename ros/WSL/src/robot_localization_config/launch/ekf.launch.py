#!/usr/bin/env python3
"""
ekf.launch.py

wheel_odom + IMU → EKF → /odom 발행
WSL에서 실행
"""

import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    ekf_config = os.path.join(
        get_package_share_directory('robot_localization_config'),
        'config', 'ekf.yaml'
    )

    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[ekf_config],
        remappings=[
            ('odometry/filtered', '/odom'),  # EKF 출력 → /odom
        ]
    )

    return LaunchDescription([ekf_node])
