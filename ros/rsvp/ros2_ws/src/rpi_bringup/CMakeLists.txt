cmake_minimum_required(VERSION 3.8)
project(rpi_bringup)

find_package(ament_cmake REQUIRED)

# Install launch files
install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

# Install config files
install(DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

# Install scripts
install(PROGRAMS scripts/find_lidar_port.sh
  DESTINATION share/${PROJECT_NAME}/scripts
)

ament_package()
