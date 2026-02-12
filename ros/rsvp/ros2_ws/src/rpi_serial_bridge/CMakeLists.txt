cmake_minimum_required(VERSION 3.8)
project(rpi_serial_bridge)

# C++17 standard
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

# Include directories
include_directories(
  include
)

# Serial protocol library
add_library(serial_protocol
  src/serial_protocol.cpp
)
ament_target_dependencies(serial_protocol
  rclcpp
)

# Odometry calculator library
add_library(odometry_calculator
  src/odometry_calculator.cpp
)
ament_target_dependencies(odometry_calculator
  rclcpp
  geometry_msgs
  nav_msgs
  tf2
  tf2_geometry_msgs
)

# Main node executable
add_executable(serial_bridge_node
  src/main.cpp
  src/serial_bridge_node.cpp
)
ament_target_dependencies(serial_bridge_node
  rclcpp
  std_msgs
  sensor_msgs
  nav_msgs
  geometry_msgs
  tf2
  tf2_ros
  tf2_geometry_msgs
)
target_link_libraries(serial_bridge_node
  serial_protocol
  odometry_calculator
)

# Install targets
install(TARGETS
  serial_protocol
  odometry_calculator
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(TARGETS
  serial_bridge_node
  DESTINATION lib/${PROJECT_NAME}
)

# Install launch files
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

# Install config files
install(DIRECTORY
  config
  DESTINATION share/${PROJECT_NAME}/
)

# Install include directory
install(DIRECTORY
  include/
  DESTINATION include
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
