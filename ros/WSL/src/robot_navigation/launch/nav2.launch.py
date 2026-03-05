#!/usr/bin/env python3
"""
nav2.launch.py
==============
Nav2 자율주행 스택 런처

localization.launch.py와 함께 실행 (navigation.launch.py에서 호출)
SLAM Toolbox localization 모드가 map → odom TF를 발행 중이어야 함

cmd_vel 흐름:
  controller_server → (cmd_vel_nav) → velocity_smoother → /cmd_vel → serial_bridge (Pi)

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
        'bt_navigator',
        'waypoint_follower',
        'velocity_smoother',
    ]

    # 로컬 플래너 + 로컬 costmap
    controller_server = Node(
        package='nav2_controller',
        executable='controller_server',
        name='controller_server',
        output='screen',
        parameters=[nav2_params],
        # velocity_smoother 사용 시: cmd_vel → cmd_vel_nav
        remappings=[('cmd_vel', 'cmd_vel_nav')],
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
    behavior_server = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        name='behavior_server',
        output='screen',
        parameters=[nav2_params],
        remappings=[('cmd_vel', '/cmd_vel')],
    )

    # 고수준 네비게이션 BT 실행기
    bt_navigator = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        name='bt_navigator',
        output='screen',
        parameters=[nav2_params],
    )

    # 다중 웨이포인트 순차 실행
    waypoint_follower = Node(
        package='nav2_waypoint_follower',
        executable='waypoint_follower',
        name='waypoint_follower',
        output='screen',
        parameters=[nav2_params],
    )

    # cmd_vel 스무딩: cmd_vel_nav → /cmd_vel
    velocity_smoother = Node(
        package='nav2_velocity_smoother',
        executable='velocity_smoother',
        name='velocity_smoother',
        output='screen',
        parameters=[nav2_params],
        remappings=[
            ('cmd_vel', 'cmd_vel_nav'),           # controller_server 출력 구독
            ('cmd_vel_smoothed', '/cmd_vel'),     # 스무딩 결과 → serial_bridge
        ],
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
        bt_navigator,
        waypoint_follower,
        velocity_smoother,
        lifecycle_manager,
    ])
