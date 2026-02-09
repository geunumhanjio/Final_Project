FROM ros:humble-ros-base

ENV DEBIAN_FRONTEND=noninteractive

# 기본 도구
RUN apt-get update && apt-get install -y \
    python3-pip \
    python3-colcon-common-extensions \
    git \
    vim \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# CycloneDDS
RUN apt-get update && apt-get install -y \
    ros-humble-rmw-cyclonedds-cpp \
    && rm -rf /var/lib/apt/lists/*

# 시리얼 통신
RUN apt-get update && apt-get install -y \
    python3-serial \
    && rm -rf /var/lib/apt/lists/*

# 카메라 & OpenCV
RUN apt-get update && apt-get install -y \
    ros-humble-cv-bridge \
    ros-humble-image-transport \
    ros-humble-image-transport-plugins \
    python3-opencv \
    v4l-utils \
    && rm -rf /var/lib/apt/lists/*

# 네트워크 도구
RUN apt-get update && apt-get install -y \
    net-tools \
    iputils-ping \
    iproute2 \
    tcpdump \
    && rm -rf /var/lib/apt/lists/*

# udev
RUN apt-get update && apt-get install -y \
    udev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /root/ros2_ws

# ROS2 환경
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc && \
    echo "if [ -f /root/ros2_ws/install/setup.bash ]; then source /root/ros2_ws/install/setup.bash; fi" >> ~/.bashrc && \
    echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc && \
    echo "export ROS_DOMAIN_ID=0" >> ~/.bashrc && \
    echo "export ROS_LOCALHOST_ONLY=0" >> ~/.bashrc

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/bash"]
