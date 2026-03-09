#!/usr/bin/env python3
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    # Launch arguments 선언
    input_topic_arg = DeclareLaunchArgument(
        'input_topic',
        default_value='/camera/image_raw/compressed',  # compressed로 변경
        description='Input ROS compressed image topic'
    )
    
    rtsp_host_arg = DeclareLaunchArgument(
        'rtsp_host',
        default_value='localhost',
        description='RTSP server host address'
    )
    
    rtsp_port_arg = DeclareLaunchArgument(
        'rtsp_port',
        default_value='9554',
        description='RTSP server port'
    )
    
    stream_name_arg = DeclareLaunchArgument(
        'stream_name',
        default_value='camera',
        description='RTSP stream name'
    )
    
    width_arg = DeclareLaunchArgument(
        'width',
        default_value='640',
        description='Output video width'
    )
    
    height_arg = DeclareLaunchArgument(
        'height',
        default_value='480',
        description='Output video height'
    )
    
    fps_arg = DeclareLaunchArgument(
        'fps',
        default_value='30',
        description='Output video FPS'
    )
    
    bitrate_arg = DeclareLaunchArgument(
        'bitrate',
        default_value='2000',
        description='Video bitrate in kbps'
    )
    
    # RTSP Publisher 노드
    rtsp_publisher_node = Node(
        package='rtsp_bridge',
        executable='rtsp_publisher',
        name='rtsp_publisher',
        output='screen',
        parameters=[{
            'input_topic': LaunchConfiguration('input_topic'),
            'rtsp_host': LaunchConfiguration('rtsp_host'),
            'rtsp_port': LaunchConfiguration('rtsp_port'),
            'stream_name': LaunchConfiguration('stream_name'),
            'width': LaunchConfiguration('width'),
            'height': LaunchConfiguration('height'),
            'fps': LaunchConfiguration('fps'),
            'bitrate': LaunchConfiguration('bitrate'),
        }]
    )
    
    return LaunchDescription([
        input_topic_arg,
        rtsp_host_arg,
        rtsp_port_arg,
        stream_name_arg,
        width_arg,
        height_arg,
        fps_arg,
        bitrate_arg,
        rtsp_publisher_node,
    ])
