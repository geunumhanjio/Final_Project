#!/usr/bin/env python3
"""
navigation_manager.launch.py
=============================
자율 주행 미션 컨트롤러 런처

사용법 (단독):
  ros2 launch navigation_manager navigation_manager.launch.py

사용법 (웨이포인트 파일 지정):
  ros2 launch navigation_manager navigation_manager.launch.py \
    waypoints_file:=/path/to/waypoints.yaml
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_share = FindPackageShare('navigation_manager')

    default_wp_file = PathJoinSubstitution([pkg_share, 'config', 'waypoints.yaml'])

    return LaunchDescription([
        DeclareLaunchArgument(
            'waypoints_file',
            default_value=default_wp_file,
            description='웨이포인트 YAML 파일 경로'
        ),
        DeclareLaunchArgument(
            'max_retries',
            default_value='2',
            description='목표 실패 시 최대 재시도 횟수'
        ),
        DeclareLaunchArgument(
            'retry_delay_sec',
            default_value='2.0',
            description='재시도 전 대기 시간 (초)'
        ),

        Node(
            package='navigation_manager',
            executable='navigation_manager',
            name='navigation_manager',
            output='screen',
            parameters=[{
                'waypoints_file':  LaunchConfiguration('waypoints_file'),
                'max_retries':     LaunchConfiguration('max_retries'),
                'retry_delay_sec': LaunchConfiguration('retry_delay_sec'),
                'linear_speed':    0.12,
            }],
        ),
    ])
