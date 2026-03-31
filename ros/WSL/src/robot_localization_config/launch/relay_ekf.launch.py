#!/usr/bin/env python3
"""
relay_ekf.launch.py
====================
WSL relay EKF: /odom_relay → odom→base_footprint TF 연속 발행

WiFi 갭(최대 200ms) 구간에도 EKF 예측으로 TF를 끊기지 않게 발행.
scan_relay의 TF 발행 없이 이 노드가 TF를 담당한다.
"""

import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    relay_ekf_config = os.path.join(
        get_package_share_directory('robot_localization_config'),
        'config', 'relay_ekf.yaml'
    )

    relay_ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='relay_ekf_node',
        output='screen',
        parameters=[relay_ekf_config],
        remappings=[
            ('odometry/filtered', '/odom_relay_filtered'),
        ]
    )

    return LaunchDescription([relay_ekf_node])
