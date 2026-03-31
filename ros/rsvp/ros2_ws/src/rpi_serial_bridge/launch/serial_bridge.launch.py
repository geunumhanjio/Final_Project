import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # 패키지 경로
    package_dir = get_package_share_directory('rpi_serial_bridge')
    
    # 파라미터 파일 경로
    params_file = os.path.join(package_dir, 'config', 'serial_bridge_params.yaml')
    
    # Serial Bridge 노드
    serial_bridge_node = Node(
        package='rpi_serial_bridge',
        executable='serial_bridge_node',
        name='serial_bridge_node',
        output='screen',
        emulate_tty=True,
        parameters=[params_file],
        respawn=True,  # 노드 크래시 시 재시작
        respawn_delay=2.0
    )
    
    return LaunchDescription([
        serial_bridge_node
    ])
