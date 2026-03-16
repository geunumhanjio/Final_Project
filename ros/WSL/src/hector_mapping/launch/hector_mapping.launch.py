#!/usr/bin/env python3
"""
hector_mapping.launch.py
========================
HectorSLAM ROS2 포트 단독 런처

scan-only 모드 (odom/imu 불필요):
  ros2 launch hector_mapping hector_mapping.launch.py

odom 연동 모드 (map→odom TF 발행):
  ros2 launch hector_mapping hector_mapping.launch.py odom_frame:=odom
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg = get_package_share_directory('hector_mapping')
    params = os.path.join(pkg, 'config', 'hector_mapping_params.yaml')

    odom_frame_arg = DeclareLaunchArgument(
        'odom_frame',
        default_value='base_footprint',
        description=(
            'odom_frame: '
            '"base_footprint" = scan-only (odom 불필요), '
            '"odom" = map→odom TF 발행'
        ),
    )

    hector_node = Node(
        package='hector_mapping',
        executable='hector_mapping',
        name='hector_mapping',
        output='screen',
        parameters=[
            params,
            {
                'odom_frame': LaunchConfiguration('odom_frame'),
                'use_sim_time': False,
            }
        ],
    )

    return LaunchDescription([odom_frame_arg, hector_node])
