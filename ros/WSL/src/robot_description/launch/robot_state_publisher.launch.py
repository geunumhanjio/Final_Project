#!/usr/bin/env python3
"""
robot_state_publisher.launch.py

TF 트리를 발행하는 런치 파일.
라즈베리파이와 WSL 양쪽에서 실행 가능하지만,
DDS로 연결된 환경에서는 한쪽에서만 실행해야 함.
→ 권장: WSL에서 실행
"""

import os
from pathlib import Path

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    pkg_dir = get_package_share_directory('robot_description')
    urdf_file = os.path.join(pkg_dir, 'urdf', 'robot.urdf.xacro')

    # xacro → URDF 변환
    robot_description = ParameterValue(
        Command(['xacro ', urdf_file]),
        value_type=str
    )

    # ----------------------------------------------------------------
    # robot_state_publisher
    # URDF를 /robot_description 토픽으로 발행하고
    # 고정 조인트(fixed joint)에 대한 TF를 자동 발행
    # ----------------------------------------------------------------
    robot_state_publisher_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': robot_description,
            'publish_frequency': 50.0,       # TF 발행 주파수
            'use_sim_time': False,
        }]
    )

    # ----------------------------------------------------------------
    # joint_state_publisher
    # continuous 조인트(바퀴)의 상태를 발행
    # 실제 엔코더 데이터가 있으면 이 노드는 불필요하지만,
    # 초기 테스트 시 유용
    # ----------------------------------------------------------------
    joint_state_publisher_node = Node(
        package='joint_state_publisher',
        executable='joint_state_publisher',
        name='joint_state_publisher',
        output='screen',
        parameters=[{
            'use_sim_time': False,
            'rate': 50,
        }]
    )

    return LaunchDescription([
        robot_state_publisher_node,
        joint_state_publisher_node,
    ])
