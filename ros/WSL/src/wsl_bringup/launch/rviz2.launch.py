#!/usr/bin/env python3
"""
rviz2.launch.py
===============
RViz2 / rqt 선택적 런처
nav_mode에 따라 적합한 RViz2 설정 파일을 자동 선택합니다.

사용법:
  ros2 launch wsl_bringup rviz2.launch.py use_rviz:=true
  ros2 launch wsl_bringup rviz2.launch.py use_rviz:=true rviz_mode:=navigation
  ros2 launch wsl_bringup rviz2.launch.py use_rviz:=true use_rqt:=true
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, PythonExpression
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_share = FindPackageShare('wsl_bringup')

    use_rviz_arg  = DeclareLaunchArgument('use_rviz',  default_value='false')
    use_rqt_arg   = DeclareLaunchArgument('use_rqt',   default_value='false')
    rviz_mode_arg = DeclareLaunchArgument(
        'rviz_mode',
        default_value='slam',
        description='"slam" | "localization" | "none"'
    )

    # localization / hector 모드 → navigation.rviz (Nav2 패널 포함)
    # slam / none 모드 → slam.rviz
    rviz_config = PathJoinSubstitution([
        pkg_share, 'config', 'rviz',
        PythonExpression([
            '"navigation.rviz" if "', LaunchConfiguration('rviz_mode'),
            '" in ("localization", "hector") else "slam.rviz"'
        ])
    ])

    rviz2_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_config],
        condition=IfCondition(LaunchConfiguration('use_rviz')),
        output='screen',
    )

    rqt_node = Node(
        package='rqt_gui',
        executable='rqt_gui',
        name='rqt',
        condition=IfCondition(LaunchConfiguration('use_rqt')),
        output='screen',
    )

    return LaunchDescription([
        use_rviz_arg,
        use_rqt_arg,
        rviz_mode_arg,
        rviz2_node,
        rqt_node,
    ])
