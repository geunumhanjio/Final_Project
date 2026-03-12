#!/usr/bin/env python3
"""
servo_tilt.launch.py
====================
카메라 틸트 서보 단독 런처

사용법:
  ros2 launch rpi_servo servo_tilt.launch.py
  ros2 launch rpi_servo servo_tilt.launch.py gpio_pin:=13 tilt_min:=-45.0 tilt_max:=45.0
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('gpio_pin',   default_value='18',    description='서보 PWM GPIO 핀 번호'),
        DeclareLaunchArgument('tilt_min',   default_value='-30.0', description='최소 틸트 각도 (deg)'),
        DeclareLaunchArgument('tilt_max',   default_value='30.0',  description='최대 틸트 각도 (deg)'),
        DeclareLaunchArgument('init_angle', default_value='0.0',   description='초기 각도 (deg)'),

        Node(
            package='rpi_servo',
            executable='servo_tilt_node',
            name='servo_tilt_node',
            output='screen',
            parameters=[{
                'gpio_pin':   LaunchConfiguration('gpio_pin'),
                'tilt_min':   LaunchConfiguration('tilt_min'),
                'tilt_max':   LaunchConfiguration('tilt_max'),
                'init_angle': LaunchConfiguration('init_angle'),
            }],
        ),
    ])
