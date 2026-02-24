cmake_minimum_required(VERSION 3.8)
project(robot_description)

find_package(ament_cmake REQUIRED)

# urdf, launch, config, rviz 디렉토리를 install space에 설치
install(
  DIRECTORY urdf launch rviz
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
