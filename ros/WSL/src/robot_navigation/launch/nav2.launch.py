#!/usr/bin/env python3
"""
nav2.launch.py
==============
Nav2 자율주행 스택 런처

localization.launch.py와 함께 실행 (navigation.launch.py에서 호출)
SLAM Toolbox localization 모드가 map → odom TF를 발행 중이어야 함

cmd_vel 흐름:
  controller_server → /cmd_vel → serial_bridge (Pi)
  behavior_server   → /cmd_vel → serial_bridge (Pi)

실행:
  ros2 launch robot_navigation nav2.launch.py
"""

import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    pkg_dir = get_package_share_directory('robot_navigation')
    nav2_params = os.path.join(pkg_dir, 'config', 'nav2_params.yaml')

    lifecycle_nodes = [
        'controller_server',
        'smoother_server',
        'planner_server',
        'behavior_server',
        'velocity_smoother',
        'bt_navigator',
    ]

    # 로컬 플래너 + 로컬 costmap
    # cmd_vel → /cmd_vel_raw : velocity_smoother가 받아서 /cmd_vel로 내보냄
    controller_server = Node(
        package='nav2_controller',
        executable='controller_server',
        name='controller_server',
        output='screen',
        parameters=[nav2_params],
        remappings=[('cmd_vel', '/cmd_vel_raw')],
    )

    # 경로 스무딩
    smoother_server = Node(
        package='nav2_smoother',
        executable='smoother_server',
        name='smoother_server',
        output='screen',
        parameters=[nav2_params],
    )

    # 글로벌 플래너 + 글로벌 costmap
    planner_server = Node(
        package='nav2_planner',
        executable='planner_server',
        name='planner_server',
        output='screen',
        parameters=[nav2_params],
    )

    # Recovery behavior (spin, backup, wait)
    # cmd_vel → /cmd_vel_raw : velocity_smoother 경유
    behavior_server = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        name='behavior_server',
        output='screen',
        parameters=[nav2_params],
        remappings=[('cmd_vel', '/cmd_vel_raw')],
    )

    # cmd_vel 가속도 제한 (급격한 회전 방지 → HectorSLAM 맵 보호)
    # /cmd_vel_raw (controller/behavior) → smooth → /cmd_vel (로봇)
    velocity_smoother = Node(
        package='nav2_velocity_smoother',
        executable='velocity_smoother',
        name='velocity_smoother',
        output='screen',
        parameters=[nav2_params],
        remappings=[('cmd_vel', '/cmd_vel_raw'),
                    ('cmd_vel_smoothed', '/cmd_vel')],
    )

    # 고수준 네비게이션 BT 실행기
    # goal_pose 토픽 remap: RViz2 /goal_pose는 navigation_manager가 처리
    # bt_navigator가 직접 /goal_pose를 처리하면 navigation_manager와 충돌
    bt_navigator = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        name='bt_navigator',
        output='screen',
        parameters=[nav2_params],
        remappings=[('goal_pose', '/bt_nav_goal_pose')],
    )

    # Lifecycle 관리 (모든 Nav2 노드 autostart)
    lifecycle_manager = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_navigation',
        output='screen',
        parameters=[
            {'use_sim_time': False},
            {'autostart': True},
            {'node_names': lifecycle_nodes},
        ],
    )

    return LaunchDescription([
        controller_server,
        smoother_server,
        planner_server,
        behavior_server,
        velocity_smoother,
        bt_navigator,
        lifecycle_manager,
    ])
