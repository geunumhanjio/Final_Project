#!/usr/bin/env python3
"""
rpi_bringup.launch.py
=====================
라즈베리파이 전체 시스템 메인 런처

사용법:
  # 전체 실행 (기본)
  ros2 launch rpi_bringup rpi_bringup.launch.py

  # 라이다 포트 지정
  ros2 launch rpi_bringup rpi_bringup.launch.py lidar_port:=/dev/ttyUSB1

  # 라이다 없이 실행
  ros2 launch rpi_bringup rpi_bringup.launch.py use_lidar:=false

  # 카메라 + 시리얼만
  ros2 launch rpi_bringup rpi_bringup.launch.py use_lidar:=false

새 패키지 추가 시:
  1. DeclareLaunchArgument 추가 (use_xxx)
  2. Node 또는 IncludeLaunchDescription 추가 + IfCondition(use_xxx)
"""

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    LogInfo,
)
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_share = FindPackageShare('rpi_bringup')
    params_file = PathJoinSubstitution([pkg_share, 'config', 'rpi_params.yaml'])
    serial_bridge_params = PathJoinSubstitution([pkg_share, 'config', 'serial_bridge_params.yaml'])

    # ── Launch Arguments ───────────────────────────────────────────────────────
    use_camera_arg = DeclareLaunchArgument(
        'use_camera',
        default_value='true',
        description='Enable IMX219 camera node'
    )
    use_serial_arg = DeclareLaunchArgument(
        'use_serial',
        default_value='true',
        description='Enable STM32 serial bridge node'
    )
    use_lidar_arg = DeclareLaunchArgument(
        'use_lidar',
        default_value='true',
        description='Enable YDLiDAR X4 node'
    )
    lidar_port_arg = DeclareLaunchArgument(
        'lidar_port',
        default_value='/dev/ttyUSB0',
        description='YDLiDAR USB port (e.g. /dev/ttyUSB0, /dev/ttyUSB1, /dev/ydlidar)'
    )

    # ── Node: Camera ───────────────────────────────────────────────────────────
    camera_node = Node(
        package='camera_ros',
        executable='camera_node',
        name='camera',
        parameters=[params_file],
        condition=IfCondition(LaunchConfiguration('use_camera')),
        output='screen',
    )

    # ── Node: STM32 Serial Bridge ──────────────────────────────────────────────
    serial_bridge_node = Node(
        package='rpi_serial_bridge',
        executable='serial_bridge_node',
        name='serial_bridge_node',
        parameters=[params_file, serial_bridge_params],
        condition=IfCondition(LaunchConfiguration('use_serial')),
        output='screen',
    )

    # ── Include: LiDAR ────────────────────────────────────────────────────────
    lidar_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([pkg_share, 'launch', 'lidar.launch.py'])
        ]),
        launch_arguments={
            'lidar_port': LaunchConfiguration('lidar_port'),
        }.items(),
        condition=IfCondition(LaunchConfiguration('use_lidar')),
    )

    return LaunchDescription([
        # Arguments
        use_camera_arg,
        use_serial_arg,
        use_lidar_arg,
        lidar_port_arg,

        # Log
        LogInfo(msg=['[rpi_bringup] Starting Raspberry Pi system...']),
        LogInfo(msg=['[rpi_bringup] lidar_port = ', LaunchConfiguration('lidar_port')]),

        # Nodes / Includes
        camera_node,
        serial_bridge_node,
        lidar_launch,
    ])
