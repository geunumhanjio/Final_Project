cmake_minimum_required(VERSION 3.10)
project(ProxyServer)

set(CMAKE_CXX_STANDARD 17)

# Find Packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-rtsp-server-1.0 gstreamer-rtsp-1.0 gstreamer-app-1.0 gio-2.0)
find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread) 
find_package(OpenSSL REQUIRED)
set(JWT_CPP_INCLUDE_DIR "" CACHE PATH "Directory containing jwt-cpp/jwt.h")

find_path(JWT_CPP_INCLUDE_DIR jwt-cpp/jwt.h
    HINTS
        ${JWT_CPP_INCLUDE_DIR}
        /opt/vcpkg/packages/jwt-cpp_arm64-linux/include
        /opt/vcpkg/installed/arm64-linux/include
        /usr/include
        /usr/local/include
)

if (NOT JWT_CPP_INCLUDE_DIR)
    message(FATAL_ERROR "jwt-cpp header not found. Install jwt-cpp so that jwt-cpp/jwt.h is available or pass -DJWT_CPP_INCLUDE_DIR=/path/to/include.")
endif()
# Note: Beast is header-only but depends on system/thread usually. 
# If Boost is header-only in the system, COMPONENTS might not be needed, but 'system' is safer.

# Include Directories
include_directories(include)
include_directories(${GST_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${JWT_CPP_INCLUDE_DIR})

# Source Files
set(SOURCES
    src/main.cpp
    src/media/RtspProxy.cpp
    src/media/CctvScanner.cpp
    src/media/GstQualityMonitor.cpp
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
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)
