cmake_minimum_required(VERSION 3.8)
project(websocket_bridge)

# C++ 표준
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ROS2 패키지 찾기
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

# 외부 라이브러리
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

# pkg-config로 jsoncpp 찾기
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Include 디렉토리
include_directories(
  include
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
  ${JSONCPP_INCLUDE_DIRS}
)

# 실행 파일
add_executable(websocket_bridge_node
  src/main.cpp
  src/websocket_server.cpp
  src/ros_bridge.cpp
)

ament_target_dependencies(websocket_bridge_node
  rclcpp
  std_msgs
  geometry_msgs
  nav_msgs
  sensor_msgs
  tf2
  tf2_geometry_msgs
)

target_link_libraries(websocket_bridge_node
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${JSONCPP_LIBRARIES}
  pthread
)

# 설치
install(TARGETS
  websocket_bridge_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
