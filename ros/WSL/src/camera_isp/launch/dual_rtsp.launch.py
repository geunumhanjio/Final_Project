#!/usr/bin/env python3
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, GroupAction
from launch_ros.actions import PushRosNamespace, Node
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():
    rtsp_bridge_launch_dir = os.path.join(get_package_share_directory('rtsp_bridge'), 'launch')
    rtsp_launch_file = os.path.join(rtsp_bridge_launch_dir, 'rtsp_publisher.launch.py')
    
    # 1. 원본 스트림 송출 노드 (기존 rtsp_publisher.launch.py의 기본 설정 모두 상속)
    raw_rtsp_cmd = GroupAction(
        actions=[
            PushRosNamespace('raw'),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(rtsp_launch_file),
                launch_arguments={
                    'input_topic': '/camera/image_raw/compressed',
                    'stream_name': 'camera',
                }.items()
            )
        ]
    )
    
    # 2. ISP 강화 스트림 송출 노드 (기존 rtsp_publisher.launch.py의 기본 설정 모두 상속)
    isp_rtsp_cmd = GroupAction(
        actions=[
            PushRosNamespace('isp'),
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(rtsp_launch_file),
                launch_arguments={
                    'input_topic': '/camera/image_isp/compressed',
                    'stream_name': 'camera_isp',
                }.items()
            )
        ]
    )

    # 3. ISP 지표 측정 파이썬 노드 추가
    metrics_node_cmd = Node(
        package='camera_isp',
        executable='isp_metrics_node.py',
        name='isp_metrics_node',
        output='screen',
        emulate_tty=True
    )

    # 4. 카메라 ISP 노드 추가 (지연율 출력)
    isp_node_cmd = Node(
        package='camera_isp',
        executable='isp_node',
        name='isp_node',
        output='screen',
        emulate_tty=True
    )

    return LaunchDescription([
        raw_rtsp_cmd,
        isp_rtsp_cmd,
        metrics_node_cmd,
        isp_node_cmd
    ])
