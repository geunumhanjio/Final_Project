#!/usr/bin/env python3
"""
person_tracker.launch.py
========================
사람 추종 노드 단독 런처

사용법:
  ros2 launch person_tracker person_tracker.launch.py
  ros2 launch person_tracker person_tracker.launch.py model:=yolov8s.pt max_fps:=15.0
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('model',         default_value='yolov8n.pt', description='YOLO 모델 파일'),
        DeclareLaunchArgument('max_fps',       default_value='10.0',       description='최대 추론 FPS'),
        DeclareLaunchArgument('conf_threshold',default_value='0.5',        description='감지 신뢰도 임계값'),
        DeclareLaunchArgument('pan_gain',      default_value='0.6',        description='수평 추종 게인'),
        DeclareLaunchArgument('tilt_gain',     default_value='25.0',       description='수직 틸트 게인'),

        Node(
            package='person_tracker',
            executable='person_tracker_node',
            name='person_tracker_node',
            output='screen',
            parameters=[{
                'model':         LaunchConfiguration('model'),
                'max_fps':       LaunchConfiguration('max_fps'),
                'conf_threshold':LaunchConfiguration('conf_threshold'),
                'pan_gain':      LaunchConfiguration('pan_gain'),
                'tilt_gain':     LaunchConfiguration('tilt_gain'),
            }],
        ),
    ])
