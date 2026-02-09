FROM osrf/ros:humble-desktop

# 기본 도구 설치
RUN apt-get update && apt-get install -y \
    python3-pip \
    python3-colcon-common-extensions \
    git \
    vim \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# ⭐ CycloneDDS 설치
RUN apt-get update && apt-get install -y \
    ros-humble-rmw-cyclonedds-cpp \
    && rm -rf /var/lib/apt/lists/*

# OpenCV 및 ROS2 이미지 관련 패키지
RUN apt-get update && apt-get install -y \
    ros-humble-cv-bridge \
    ros-humble-image-transport \
    ros-humble-image-transport-plugins \
    libopencv-dev \
    python3-opencv \
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
    libgstrtspserver-1.0-dev \
    gstreamer1.0-rtsp \
    && rm -rf /var/lib/apt/lists/*

# ⭐ Navigation2 및 SLAM 도구
RUN apt-get update && apt-get install -y \
    ros-humble-navigation2 \
    ros-humble-nav2-bringup \
    ros-humble-slam-toolbox \
    ros-humble-robot-localization \
    && rm -rf /var/lib/apt/lists/*

# ⭐ 추가 유틸리티 (디버깅용)
RUN apt-get update && apt-get install -y \
    net-tools \
    iputils-ping \
    iproute2 \
    && rm -rf /var/lib/apt/lists/*

# 작업 디렉터리
WORKDIR /root/ros2_ws

# ROS2 환경 자동 로드
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc && \
    echo "if [ -f /root/ros2_ws/install/setup.bash ]; then source /root/ros2_ws/install/setup.bash; fi" >> ~/.bashrc && \
    echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc && \
    echo "export ROS_DOMAIN_ID=0" >> ~/.bashrc

CMD ["/bin/bash"]
