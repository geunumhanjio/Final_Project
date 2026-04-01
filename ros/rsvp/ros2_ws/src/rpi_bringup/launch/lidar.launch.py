#!/usr/bin/env python3
"""
lidar.launch.py
===============
YDLiDAR X4 단독 런처
라이다 포트를 런타임에 오버라이드할 수 있습니다.

사용법:
  ros2 launch rpi_bringup lidar.launch.py
  ros2 launch rpi_bringup lidar.launch.py lidar_port:=/dev/ttyUSB1
  ros2 launch rpi_bringup lidar.launch.py lidar_port:=/dev/ydlidar  # udev rule 사용 시
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_share = FindPackageShare('rpi_bringup')

    lidar_port_arg = DeclareLaunchArgument(
        'lidar_port',
        default_value='/dev/ttyUSB0',
        description='YDLiDAR USB serial port'
    )

    # ydlidar_params.yaml 의 나머지 파라미터 + 포트를 런타임 오버라이드
    ydlidar_node = Node(
        package='ydlidar_ros2_driver',
        executable='ydlidar_ros2_driver_node',
        name='ydlidar_ros2_driver_node',
        parameters=[
            PathJoinSubstitution([pkg_share, 'config', 'ydlidar_params.yaml']),
            # port는 yaml보다 나중에 오므로 오버라이드됨
            {'port': LaunchConfiguration('lidar_port')},
        ],
        output='screen',
    )

    return LaunchDescription([
        lidar_port_arg,
        ydlidar_node,
    ])
