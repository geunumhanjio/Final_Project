#!/usr/bin/env python3
"""
fall_detection.launch.py
========================
낙상(누워있는 상태) 감지 노드 단독 런처

사용법:
  ros2 launch fall_detection fall_detection.launch.py
  ros2 launch fall_detection fall_detection.launch.py enabled_on_start:=false
  ros2 launch fall_detection fall_detection.launch.py cooldown_sec:=30.0 max_fps:=3.0
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('max_fps',               default_value='5.0',
                              description='최대 추론 FPS (낮을수록 CPU 절약)'),
        DeclareLaunchArgument('image_topic',           default_value='/camera/image_raw/compressed',
                              description='구독할 카메라 토픽'),
        DeclareLaunchArgument('lying_angle_threshold', default_value='45.0',
                              description='누워있음 판정 각도 임계값 (deg, 수직=0)'),
        DeclareLaunchArgument('cooldown_sec',          default_value='20.0',
                              description='경고 재발행 최소 간격 (초)'),
        DeclareLaunchArgument('confirm_count',         default_value='3',
                              description='연속 판정 횟수 (false positive 방지)'),
        DeclareLaunchArgument('enabled_on_start',      default_value='true',
                              description='시작 시 감지 활성화 여부'),

        Node(
            package='fall_detection',
            executable='fall_detection_node',
            name='fall_detection_node',
            output='screen',
            parameters=[{
                'max_fps':               LaunchConfiguration('max_fps'),
                'image_topic':           LaunchConfiguration('image_topic'),
                'lying_angle_threshold': LaunchConfiguration('lying_angle_threshold'),
                'cooldown_sec':          LaunchConfiguration('cooldown_sec'),
                'confirm_count':         LaunchConfiguration('confirm_count'),
                'enabled_on_start':      LaunchConfiguration('enabled_on_start'),
            }],
        ),
    ])
