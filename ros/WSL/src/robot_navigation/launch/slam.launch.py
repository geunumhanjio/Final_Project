#!/usr/bin/env python3
"""
slam.launch.py

SLAM Toolbox로 실시간 맵 생성
WSL에서 실행

실행 순서:
1. 라즈베리파이: robot bringup (serial_bridge, lidar, camera)
2. WSL: robot_state_publisher.launch.py
3. WSL: ekf.launch.py
4. WSL: slam.launch.py  ← 이 파일
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    pkg_dir = get_package_share_directory('robot_navigation')
    slam_params = os.path.join(pkg_dir, 'config', 'slam_params.yaml')

    # SLAM Toolbox - online async 모드 (실시간 맵핑)
    slam_node = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[
            slam_params,
            {'use_sim_time': False}
        ],
    )

    return LaunchDescription([slam_node])
