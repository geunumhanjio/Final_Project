#!/usr/bin/env python3
"""
camera.launch.py
================
IMX219 카메라 단독 런처

사용법:
  ros2 launch rpi_bringup camera.launch.py
  ros2 launch rpi_bringup camera.launch.py width:=1280 height:=720
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_share = FindPackageShare('rpi_bringup')

    return LaunchDescription([
        DeclareLaunchArgument('camera_id', default_value='0'),
        DeclareLaunchArgument('width',     default_value='640'),
        DeclareLaunchArgument('height',    default_value='480'),
        DeclareLaunchArgument('format',    default_value='RGB888'),

        Node(
            package='camera_ros',
            executable='camera_node',
            name='camera',
            parameters=[
                PathJoinSubstitution([pkg_share, 'config', 'rpi_params.yaml']),
                {
                    'camera': LaunchConfiguration('camera_id'),
                    'width':  LaunchConfiguration('width'),
                    'height': LaunchConfiguration('height'),
                    'format': LaunchConfiguration('format'),
                },
            ],
            output='screen',
        ),
    ])
