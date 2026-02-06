FROM osrf/ros:humble-desktop

# 기본 도구 설치
RUN apt-get update && apt-get install -y \
    python3-pip \
    git \
    vim \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# OpenCV 및 ROS2 이미지 관련 패키지
RUN apt-get update && apt-get install -y \
    ros-humble-cv-bridge \
    ros-humble-image-transport \
    ros-humble-image-transport-plugins \
    libopencv-dev \
    python3-opencv \
    && rm -rf /var/lib/apt/lists/*

# 작업 디렉터리
WORKDIR /root/ros2_ws

# ROS2 환경 자동 로드
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc && \
    echo "if [ -f /root/ros2_ws/install/setup.bash ]; then source /root/ros2_ws/install/setup.bash; fi" >> ~/.bashrc

CMD ["/bin/bash"]
