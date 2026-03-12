#!/usr/bin/env python3
"""
localization.launch.py

저장된 맵을 불러와서 위치 추정만 수행 (맵 새로 안 그림)
WSL에서 실행

실행 순서:
1. 라즈베리파이: robot bringup
2. WSL: robot_state_publisher.launch.py
3. WSL: ekf.launch.py
4. WSL: localization.launch.py  ← 이 파일
"""

import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    pkg_dir = get_package_share_directory('robot_navigation')

    # SLAM Toolbox 직렬화 맵 경로 (확장자 없이 지정)
    # 필요 파일: my_map.posegraph + my_map.data
    # 저장 방법: ros2 service call /slam_toolbox/save_map slam_toolbox/srv/SaveMap "{name: {data: '/path/to/my_map'}}"
    default_map = os.path.join(pkg_dir, 'maps', 'my_map')

    declare_map_arg = DeclareLaunchArgument(
        'map',
        default_value=default_map,
        description='Full path to map yaml file'
    )

    slam_params = os.path.join(pkg_dir, 'config', 'slam_params.yaml')

    # SLAM Toolbox - localization 모드
    localization_node = Node(
        package='slam_toolbox',
        executable='localization_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[
            slam_params,
            {
                'use_sim_time': False,
                'mode': 'localization',
                'map_file_name': LaunchConfiguration('map'),
                'map_start_at_dock': True,

                # ── localization 전용 보수적 설정 ──────────────────────────
                # mapping 모드 값을 오버라이드해서 false positive 점프 방지

                # loop closure 비활성화: localization에서 loop closure는
                # 잘못된 위치로 갑자기 점프하는 주요 원인
                'do_loop_closing': False,

                # scan match 수락 기준 강화: 낮으면 엉뚱한 위치도 수락
                'link_match_minimum_response_fine': 0.50,   # 0.35 → 0.50

                # response expansion 비활성화: 약한 매칭시 검색 범위 확장하다
                # 전혀 다른 위치를 찾아버리는 현상 방지
                'use_response_expansion': False,

                # 오도메트리와 크게 다른 pose에 패널티 강화
                # EKF가 완벽하지 않아도 갑작스러운 대각도/대거리 점프를 억제
                'distance_variance_penalty': 1.0,   # 0.5 → 1.0
                'angle_variance_penalty': 1.5,      # 1.0 → 1.5
            }
        ],
    )

    return LaunchDescription([
        declare_map_arg,
        localization_node,
    ])
