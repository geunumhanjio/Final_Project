FROM osrf/ros:humble-desktop

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

# 이미지 처리
RUN apt-get update && apt-get install -y \
    ros-humble-cv-bridge \
    ros-humble-image-transport \
    ros-humble-image-transport-plugins \
    ros-humble-compressed-image-transport \
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

# Navigation2 스택
RUN apt-get update && apt-get install -y \
    ros-humble-navigation2 \
    ros-humble-nav2-bringup \
    ros-humble-nav2-common \
    ros-humble-nav2-msgs \
    && rm -rf /var/lib/apt/lists/*

# SLAM
RUN apt-get update && apt-get install -y \
    ros-humble-slam-toolbox \
    ros-humble-cartographer \
    ros-humble-cartographer-ros \
    && rm -rf /var/lib/apt/lists/*

# 센서 융합 & Localization
RUN apt-get update && apt-get install -y \
    ros-humble-robot-localization \
    ros-humble-imu-tools \
    && rm -rf /var/lib/apt/lists/*

# TF2 & 로봇 상태
RUN apt-get update && apt-get install -y \
    ros-humble-tf2-ros \
    ros-humble-tf2-tools \
    ros-humble-robot-state-publisher \
    ros-humble-joint-state-publisher \
    ros-humble-joint-state-publisher-gui \
    && rm -rf /var/lib/apt/lists/*

# URDF & Xacro
RUN apt-get update && apt-get install -y \
    ros-humble-urdf \
    ros-humble-xacro \
    && rm -rf /var/lib/apt/lists/*

# RViz2 플러그인
RUN apt-get update && apt-get install -y \
    ros-humble-rviz2 \
    ros-humble-rviz-common \
    ros-humble-rviz-default-plugins \
    && rm -rf /var/lib/apt/lists/*

# 기본 Python 라이브러리
RUN pip3 install --no-cache-dir \
    numpy \
    scipy \
    matplotlib \
    pandas

# X11 및 그래픽
RUN apt-get update && apt-get install -y \
    libgl1-mesa-glx \
    libglib2.0-0 \
    libxext6 \
    libsm6 \
    libxrender1 \
    libgomp1 \
    x11-apps \
    && rm -rf /var/lib/apt/lists/*

# 네트워크 도구
RUN apt-get update && apt-get install -y \
    net-tools \
    iputils-ping \
    iproute2 \
    tcpdump \
    nano \
    htop \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /root/ros2_ws

# ROS2 환경 설정
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc && \
    echo "if [ -f /root/ros2_ws/install/setup.bash ]; then source /root/ros2_ws/install/setup.bash; fi" >> ~/.bashrc && \
    echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc && \
    echo "export ROS_DOMAIN_ID=0" >> ~/.bashrc && \
    echo "export ROS_LOCALHOST_ONLY=0" >> ~/.bashrc && \
    echo "alias sb='source /root/ros2_ws/install/setup.bash'" >> ~/.bashrc && \
    echo "alias cb='cd /root/ros2_ws && colcon build --symlink-install'" >> ~/.bashrc

CMD ["/bin/bash"]
