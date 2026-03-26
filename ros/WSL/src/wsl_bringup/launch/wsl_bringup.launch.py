#!/usr/bin/env python3
"""
wsl_bringup.launch.py
=====================
WSL 전체 시스템 메인 런처

사용법:
  # SLAM 모드 (기본 - 새 맵 생성)
  ros2 launch wsl_bringup wsl_bringup.launch.py

  # HectorSLAM 모드 (scan-only, odom/imu 불필요)
  ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector

  # HectorSLAM + odom 연동 (map→odom TF 발행)
  ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector hector_odom_frame:=odom

  # Localization 모드 (기존 맵 사용)
  ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization

  # RViz2 + rqt 함께 실행
  ros2 launch wsl_bringup wsl_bringup.launch.py use_rviz:=true use_rqt:=true

  # SLAM + RViz2
  ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=slam use_rviz:=true

  # 네비게이션 없이 통신 레이어만
  ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=none

  # RTSP/웹소켓 없이 네비만
  ros2 launch wsl_bringup wsl_bringup.launch.py use_rtsp:=false use_websocket:=false nav_mode:=localization use_rviz:=true

  # 낙상 감지 활성화
  ros2 launch wsl_bringup wsl_bringup.launch.py use_fall_detection:=true

새 패키지 추가 시:
  1. DeclareLaunchArgument('use_xxx', ...) 추가
  2. IncludeLaunchDescription(..., condition=IfCondition('use_xxx')) 추가
  3. wsl_bringup/launch/xxx.launch.py 단독 런처 생성
"""

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    LogInfo,
)
from launch.conditions import IfCondition, LaunchConfigurationEquals
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch_ros.actions import Node


def generate_launch_description():
    pkg_share = FindPackageShare('wsl_bringup')

    # ── Launch Arguments ───────────────────────────────────────────────────────
    args = [
        DeclareLaunchArgument(
            'use_rtsp',
            default_value='true',
            description='Enable RTSP bridge (video streaming)'
        ),
        DeclareLaunchArgument(
            'use_websocket',
            default_value='true',
            description='Enable WebSocket bridge (Qt communication)'
        ),
        DeclareLaunchArgument(
            'use_robot_core',
            default_value='true',
            description='Enable robot_description + EKF localization'
        ),
        DeclareLaunchArgument(
            'nav_mode',
            default_value='slam',
            description='"slam" = 새 맵 생성 | "localization" = 기존 맵 사용 | "none" = 비활성화'
        ),
        DeclareLaunchArgument(
            'use_rviz',
            default_value='false',
            description='Launch RViz2 for visualization'
        ),
        DeclareLaunchArgument(
            'use_rqt',
            default_value='false',
            description='Launch rqt for debugging'
        ),
        DeclareLaunchArgument(
            'use_tracker',
            default_value='false',
            description='Enable YOLO person tracker node'
        ),
        DeclareLaunchArgument(
            'use_fall_detection',
            default_value='false',
            description='Enable fall detection node (lying-down detection)'
        ),
        DeclareLaunchArgument(
            'hector_odom_frame',
            default_value='odom',
            description=(
                'HectorSLAM odom_frame: '
                '"odom" = map→odom TF 발행 (navigation 기본), '
                '"base_footprint" = scan-only 순수 테스트'
            ),
        ),
    ]

    # ── Helper: include from wsl_bringup/launch/ ───────────────────────────────
    def local_include(launch_file, extra_args=None, condition=None):
        kwargs = dict(
            source=PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', launch_file])
            ]),
        )
        if extra_args:
            kwargs['launch_arguments'] = extra_args.items()
        inc = IncludeLaunchDescription(**kwargs)
        if condition is not None:
            inc = IncludeLaunchDescription(
                PythonLaunchDescriptionSource([
                    PathJoinSubstitution([pkg_share, 'launch', launch_file])
                ]),
                launch_arguments=(extra_args or {}).items(),
                condition=condition,
            )
        return inc

    # ── Nodes ──────────────────────────────────────────────────────────────────
    # hector 모드: scan_relay가 직접 odom→base_footprint TF 발행 (keepalive 50Hz)
    #   /odom은 이미 RPi EKF 출력 → WSL relay_ekf 중복 계산 불필요
    # 그 외 모드: relay_ekf가 TF 담당, scan_relay는 TF 발행 안 함
    scan_relay_hector = Node(
        package='scan_relay',
        executable='scan_relay',
        name='scan_relay',
        output='screen',
        parameters=[{'use_ekf_relay': True, 'publish_tf': True}],
        condition=LaunchConfigurationEquals('nav_mode', 'hector'),
    )
    scan_relay_other = Node(
        package='scan_relay',
        executable='scan_relay',
        name='scan_relay',
        output='screen',
        parameters=[{'use_ekf_relay': True, 'publish_tf': False}],
        condition=LaunchConfigurationEquals('nav_mode', 'slam'),
    )
    scan_relay_localization = Node(
        package='scan_relay',
        executable='scan_relay',
        name='scan_relay',
        output='screen',
        parameters=[{'use_ekf_relay': True, 'publish_tf': False}],
        condition=LaunchConfigurationEquals('nav_mode', 'localization'),
    )
    scan_relay_none = Node(
        package='scan_relay',
        executable='scan_relay',
        name='scan_relay',
        output='screen',
        parameters=[{'use_ekf_relay': True, 'publish_tf': False}],
        condition=LaunchConfigurationEquals('nav_mode', 'none'),
    )

    # ── Includes ───────────────────────────────────────────────────────────────
    includes = [
        # 로그
        LogInfo(msg=['[wsl_bringup] Starting WSL system...']),
        LogInfo(msg=['[wsl_bringup] nav_mode = ', LaunchConfiguration('nav_mode')]),

        # RTSP 브릿지
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'rtsp_bridge.launch.py'])
            ]),
            condition=IfCondition(LaunchConfiguration('use_rtsp')),
        ),

        # WebSocket 브릿지
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'websocket_bridge.launch.py'])
            ]),
            condition=IfCondition(LaunchConfiguration('use_websocket')),
        ),

        # Robot Description - SLAM/Localization/none 모드: relay_ekf 활성화
        # relay_ekf가 odom→base_footprint TF를 WiFi 갭 구간에도 유지
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'robot_core.launch.py'])
            ]),
            launch_arguments={'use_ekf': 'false', 'use_relay_ekf': 'true'}.items(),
            condition=LaunchConfigurationEquals('nav_mode', 'slam'),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'robot_core.launch.py'])
            ]),
            launch_arguments={'use_ekf': 'false', 'use_relay_ekf': 'true'}.items(),
            condition=LaunchConfigurationEquals('nav_mode', 'localization'),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'robot_core.launch.py'])
            ]),
            launch_arguments={'use_ekf': 'false', 'use_relay_ekf': 'true'}.items(),
            condition=LaunchConfigurationEquals('nav_mode', 'none'),
        ),

        # Robot Description - HectorSLAM 모드
        # relay_ekf 비활성화: scan_relay가 직접 TF 발행 (50Hz keepalive)
        # /odom = RPi EKF 출력(이미 필터됨) → WSL에서 추가 EKF 불필요
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'robot_core.launch.py'])
            ]),
            launch_arguments={'use_ekf': 'false', 'use_relay_ekf': 'false'}.items(),
            condition=LaunchConfigurationEquals('nav_mode', 'hector'),
        ),

        # SLAM (새 맵 생성 - SLAM Toolbox)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'slam_mapping.launch.py'])
            ]),
            condition=LaunchConfigurationEquals('nav_mode', 'slam'),
        ),

        # HectorSLAM (scan-only 또는 odom 연동)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('hector_mapping'),
                    'launch',
                    'hector_mapping.launch.py',
                ])
            ]),
            launch_arguments={
                'odom_frame': LaunchConfiguration('hector_odom_frame'),
            }.items(),
            condition=LaunchConfigurationEquals('nav_mode', 'hector'),
        ),

        # Localization (기존 맵 사용)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'navigation.launch.py'])
            ]),
            condition=LaunchConfigurationEquals('nav_mode', 'localization'),
        ),

        # Nav2 (HectorSLAM 모드 - HectorSLAM이 map→odom TF 발행)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('robot_navigation'),
                    'launch',
                    'nav2.launch.py',
                ])
            ]),
            condition=LaunchConfigurationEquals('nav_mode', 'hector'),
        ),

        # Navigation Manager (localization / hector 모드)
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('navigation_manager'),
                    'launch',
                    'navigation_manager.launch.py',
                ])
            ]),
            condition=LaunchConfigurationEquals('nav_mode', 'localization'),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('navigation_manager'),
                    'launch',
                    'navigation_manager.launch.py',
                ])
            ]),
            launch_arguments={'replan_on_map_update': 'false'}.items(),
            condition=LaunchConfigurationEquals('nav_mode', 'hector'),
        ),

        # RViz2 / rqt
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([pkg_share, 'launch', 'rviz2.launch.py'])
            ]),
            launch_arguments={
                'use_rviz':   LaunchConfiguration('use_rviz'),
                'use_rqt':    LaunchConfiguration('use_rqt'),
                'rviz_mode':  LaunchConfiguration('nav_mode'),
            }.items(),
        ),
    ]

    tracker_node = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('person_tracker'),
                'launch',
                'person_tracker.launch.py',
            ])
        ]),
        condition=IfCondition(LaunchConfiguration('use_tracker')),
    )

    fall_detection_node = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            PathJoinSubstitution([
                FindPackageShare('fall_detection'),
                'launch',
                'fall_detection.launch.py',
            ])
        ]),
        condition=IfCondition(LaunchConfiguration('use_fall_detection')),
    )

    scan_relay_nodes = [
        scan_relay_hector,
        scan_relay_other,
        scan_relay_localization,
        scan_relay_none,
    ]
    return LaunchDescription(args + scan_relay_nodes + includes + [tracker_node, fall_detection_node])
