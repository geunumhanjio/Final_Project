cmake_minimum_required(VERSION 3.8)
project(camera_isp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(std_msgs REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(isp_node src/isp_node.cpp)
ament_target_dependencies(isp_node rclcpp sensor_msgs cv_bridge std_msgs OpenCV)

add_executable(isp_node_custom src/isp_node_custom.cpp)
ament_target_dependencies(isp_node_custom rclcpp sensor_msgs cv_bridge std_msgs OpenCV)

add_executable(eis_node src/eis_node.cpp)
ament_target_dependencies(eis_node rclcpp sensor_msgs cv_bridge std_msgs OpenCV)

install(TARGETS
  isp_node
  isp_node_custom
  eis_node
  DESTINATION lib/${PROJECT_NAME}
)

install(PROGRAMS
  scripts/isp_metrics_node.py
  scripts/isp_stage_compare_node.py
  scripts/eis_metrics_node.py
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
