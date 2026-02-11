# camera_bridge/launch/camera_with_rtsp.launch.py
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # MJPEG를 ROS2 Image 토픽으로 변환
        Node(
            package='camera_bridge',
            executable='mjpeg_bridge',
            name='camera_bridge',
            parameters=[{
                'stream_url': 'http://192.168.0.23:8000/video'
            }],
            output='screen'
        ),
        
        # ROS2 Image 토픽을 RTSP로 송출
        Node(
            package='image2rtsp',
            executable='image2rtsp',
            name='rtsp_server',
            parameters=[{
                'compressed': False,
                'topic': '/camera/image_raw',
                'mountpoint': '/camera',
                'port': '8554',
                'local_only': False  # 외부 접근 허용!
            }],
            output='screen'
        )
    ])
