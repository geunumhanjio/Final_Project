#!/usr/bin/env python3
"""
localization.launch.py

저장된 맵을 불러와서 위치 추정만 수행 (맵 새로 안 그림)
WSL에서 실행

실행 순서:
1. 라즈베리파이: robot bringup
2. WSL: robot_state_publisher.launch.py
3. WSL: ekf.launch.py
4. WSL: localization.launch.py  ← 이 파일
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    pkg_dir = get_package_share_directory('robot_navigation')

    # 맵 파일 경로 (인자로 변경 가능)
    default_map = os.path.join(pkg_dir, 'maps', 'my_map.yaml')

    declare_map_arg = DeclareLaunchArgument(
        'map',
        default_value=default_map,
        description='Full path to map yaml file'
    )

    slam_params = os.path.join(pkg_dir, 'config', 'slam_params.yaml')

    # SLAM Toolbox - localization 모드
    localization_node = Node(
        package='slam_toolbox',
        executable='localization_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[
            slam_params,
            {
                'use_sim_time': False,
                'mode': 'localization',
                'map_file_name': LaunchConfiguration('map'),
            }
        ],
    )

    return LaunchDescription([
        declare_map_arg,
        localization_node,
    ])
