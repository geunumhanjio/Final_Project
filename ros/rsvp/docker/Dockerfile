FROM ros:humble-ros-base

ENV DEBIAN_FRONTEND=noninteractive

# 기본 빌드 도구
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3-pip \
    python3-colcon-common-extensions \
    git \
    vim \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# DDS 구현체
RUN apt-get update && apt-get install -y \
    ros-humble-rmw-cyclonedds-cpp \
    ros-humble-rmw-fastrtps-cpp \
    && rm -rf /var/lib/apt/lists/*

# 시리얼 통신
RUN apt-get update && apt-get install -y \
    python3-serial \
    && rm -rf /var/lib/apt/lists/*

# 카메라 & 이미지 처리
RUN apt-get update && apt-get install -y \
    ros-humble-cv-bridge \
    ros-humble-image-transport \
    ros-humble-image-transport-plugins \
    ros-humble-compressed-image-transport \
    python3-opencv \
    libopencv-dev \
    v4l-utils \
    && rm -rf /var/lib/apt/lists/*

# GStreamer
RUN apt-get update && apt-get install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-tools \
    && rm -rf /var/lib/apt/lists/*

# LiDAR 의존성
RUN apt-get update && apt-get install -y \
    libudev-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# TF2 & 로봇 상태
RUN apt-get update && apt-get install -y \
    ros-humble-tf2-ros \
    ros-humble-tf2-tools \
    ros-humble-robot-state-publisher \
    ros-humble-joint-state-publisher \
    ros-humble-robot-localization \
    && rm -rf /var/lib/apt/lists/*

# 네트워크 도구
RUN apt-get update && apt-get install -y \
    net-tools \
    iputils-ping \
    iproute2 \
    tcpdump \
    udev \
    nano \
    htop \
    && rm -rf /var/lib/apt/lists/*

# ROS2 camera 의존성
RUN apt-get update && apt-get install -y \
    ros-humble-camera-info-manager \
    && rm -rf /var/lib/apt/lists/*

# libcamera 빌드 의존성
RUN apt-get update && apt-get install -y \
    meson \
    ninja-build \
    libyaml-dev \
    python3-yaml \
    python3-ply \
    python3-jinja2 \
    libgnutls28-dev \
    openssl \
    libudev-dev \
    && rm -rf /var/lib/apt/lists/*

# libcamera 최신 버전 빌드 및 설치
RUN cd /tmp \
    && git clone --depth 1 --branch v0.3.0 https://github.com/raspberrypi/libcamera.git \
    && cd libcamera \
    && meson setup build --prefix=/usr --libdir=/usr/lib/aarch64-linux-gnu \
    && ninja -C build \
    && ninja -C build install \
    && ldconfig \
    && ln -s /usr/lib/aarch64-linux-gnu/pkgconfig/camera.pc /usr/lib/aarch64-linux-gnu/pkgconfig/libcamera.pc \
    && cd / && rm -rf /tmp/libcamera

# YDLiDAR SDK 설치
RUN cd /tmp \
    && git clone https://github.com/YDLIDAR/YDLidar-SDK.git \
    && cd YDLidar-SDK/build \
    && cmake .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig \
    && cd / && rm -rf /tmp/YDLidar-SDK

WORKDIR /root/ros2_ws

# ROS2 환경 설정
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc && \
    echo "if [ -f /root/ros2_ws/install/setup.bash ]; then source /root/ros2_ws/install/setup.bash; fi" >> ~/.bashrc && \
    echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc && \
    echo "export ROS_DOMAIN_ID=0" >> ~/.bashrc && \
    echo "export ROS_LOCALHOST_ONLY=0" >> ~/.bashrc && \
    echo "alias sb='source /root/ros2_ws/install/setup.bash'" >> ~/.bashrc && \
    echo "alias cb='cd /root/ros2_ws && colcon build --symlink-install'" >> ~/.bashrc

# libcamera 라이브러리 경로 환경변수 추가
RUN echo "export LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu:\$LD_LIBRARY_PATH" >> ~/.bashrc && \
    echo "export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig:\$PKG_CONFIG_PATH" >> ~/.bashrc

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/bash"]
