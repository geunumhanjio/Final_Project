from ament_index_python.resources import has_resource

from launch.actions import DeclareLaunchArgument
from launch.launch_description import LaunchDescription
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description() -> LaunchDescription:
    """
    Generate a launch description for the camera node.

    Returns
    -------
        LaunchDescription: the launch description
    """
    # Parameters
    camera_param_name = "camera"
    camera_param_default = "0"
    camera_param = LaunchConfiguration(camera_param_name, default=camera_param_default)
    camera_launch_arg = DeclareLaunchArgument(
        camera_param_name,
        default_value=camera_param_default,
        description="camera ID or name"
    )

    width_param_name = "width"
    width_param_default = "640"
    width_param = LaunchConfiguration(width_param_name, default=width_param_default)
    width_launch_arg = DeclareLaunchArgument(
        width_param_name,
        default_value=width_param_default,
        description="image width"
    )

    height_param_name = "height"
    height_param_default = "480"
    height_param = LaunchConfiguration(height_param_name, default=height_param_default)
    height_launch_arg = DeclareLaunchArgument(
        height_param_name,
        default_value=height_param_default,
        description="image height"
    )

    format_param_name = "format"
    format_param_default = "BGR888"
    format_param = LaunchConfiguration(format_param_name, default=format_param_default)
    format_launch_arg = DeclareLaunchArgument(
        format_param_name,
        default_value=format_param_default,
        description="pixel format"
    )

    fps_param_name = "fps"
    fps_param_default = "30.0"
    fps_param = LaunchConfiguration(fps_param_name, default=fps_param_default)
    fps_launch_arg = DeclareLaunchArgument(
        fps_param_name,
        default_value=fps_param_default,
        description="frames per second"
    )

    # Camera node
    composable_nodes = [
        ComposableNode(
            package='camera_ros',
            plugin='camera::CameraNode',
            parameters=[{
                "camera": camera_param,
                "width": width_param,
                "height": height_param,
                "format": format_param,
                "fps": fps_param,
            }],
            extra_arguments=[{'use_intra_process_comms': True}],
        ),
    ]

    # Optionally add ImageViewNode to show camera image
    # Disabled by default for headless operation
    # if has_resource("packages", "image_view"):
    #     composable_nodes += [
    #         ComposableNode(
    #             package='image_view',
    #             plugin='image_view::ImageViewNode',
    #             remappings=[('/image', '/camera/image_raw')],
    #             extra_arguments=[{'use_intra_process_comms': True}],
    #         ),
    #     ]

    # Composable nodes in single container
    container = ComposableNodeContainer(
        name='camera_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=composable_nodes,
    )

    return LaunchDescription([
        camera_launch_arg,
        width_launch_arg,
        height_launch_arg,
        format_launch_arg,
        fps_launch_arg,
        container,
    ])
