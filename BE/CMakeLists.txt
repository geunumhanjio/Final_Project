cmake_minimum_required(VERSION 3.10)
project(ProxyServer)

set(CMAKE_CXX_STANDARD 17)

# Find Packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-app-1.0)
find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread) 
# Note: Beast is header-only but depends on system/thread usually. 
# If Boost is header-only in the system, COMPONENTS might not be needed, but 'system' is safer.

# Include Directories
include_directories(include)
include_directories(${GST_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

# Source Files
set(SOURCES
    src/main.cpp
    src/media/RtspProxy.cpp
    src/calibration/CalibrationManager.cpp
    src/network/VmsServer.cpp 
    src/network/RosClient.cpp
)

# Executable
add_executable(ProxyServer ${SOURCES})

# Link Libraries
target_link_libraries(ProxyServer 
    ${GST_LIBRARIES} 
    ${OpenCV_LIBS} 
    ${Boost_LIBRARIES}
    pthread
)