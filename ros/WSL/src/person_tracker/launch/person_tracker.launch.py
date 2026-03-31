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
        DeclareLaunchArgument('max_fps',              default_value='10.0',                         description='최대 추론 FPS'),
        DeclareLaunchArgument('visibility_threshold', default_value='0.5',                         description='MediaPipe 랜드마크 신뢰도 임계값'),
        DeclareLaunchArgument('pan_gain',              default_value='0.6',                         description='수평 추종 게인'),
        DeclareLaunchArgument('tilt_gain',             default_value='25.0',                        description='수직 틸트 게인'),
        DeclareLaunchArgument('linear_gain',           default_value='0.4',                         description='거리 제어 게인'),
        DeclareLaunchArgument('target_shoulder_width', default_value='0.25',                        description='목표 어깨 너비 (0~1, 클수록 가까이)'),
        DeclareLaunchArgument('image_topic',           default_value='/camera/image_raw/compressed', description='구독할 카메라 토픽 (raw 또는 isp)'),

        Node(
            package='person_tracker',
            executable='person_tracker_node',
            name='person_tracker_node',
            output='screen',
            parameters=[{
                'max_fps':               LaunchConfiguration('max_fps'),
                'visibility_threshold':  LaunchConfiguration('visibility_threshold'),
                'pan_gain':              LaunchConfiguration('pan_gain'),
                'tilt_gain':             LaunchConfiguration('tilt_gain'),
                'linear_gain':           LaunchConfiguration('linear_gain'),
                'target_shoulder_width': LaunchConfiguration('target_shoulder_width'),
                'image_topic':           LaunchConfiguration('image_topic'),
            }],
        ),
    ])
