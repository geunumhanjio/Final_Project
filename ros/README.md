# CCTV-SLAM 순찰 로봇 — ROS2 시스템

> ROS2 Humble 기반의 CCTV 순찰 로봇 소프트웨어 스택.
> 라즈베리파이(하드웨어 제어)와 WSL(고연산 처리)로 역할을 분리하여 운영한다.

---

## 디렉터리 구조

```
ros/
├── rsvp/                       ← 라즈베리파이 측 (실제 로봇 하드웨어)
│   ├── docker/
│   │   ├── Dockerfile          ← ARM64 + libcamera + YDLiDAR SDK 환경
│   │   ├── docker-compose.yml  ← 하드웨어 디바이스 마운트 설정
│   │   ├── cyclonedds.xml      ← CycloneDDS 피어 설정 (WSL 주소 지정)
│   │   └── entrypoint.sh
│   └── ros2_ws/src/
│       ├── rpi_bringup/        ← RPi 전체 시스템 런처
│       ├── rpi_serial_bridge/  ← STM32 시리얼 통신 & 오도메트리
│       ├── camera_ros/         ← IMX219 카메라 드라이버 (libcamera)
│       └── ydlidar_ros2_driver/← YDLiDAR X4 드라이버
│
└── WSL/                        ← PC/WSL 측 (고연산 처리)
    ├── docker/
    │   ├── Dockerfile          ← ROS2 Humble Desktop + Nav2 + SLAM 환경
    │   ├── docker-compose.yml  ← X11 포워딩 설정
    │   └── cyclonedds.xml      ← CycloneDDS 피어 설정 (RPi 주소 지정)
    ├── mediamtx.yml            ← RTSP 미디어 서버 설정
    └── src/
        ├── wsl_bringup/        ← WSL 전체 시스템 런처
        ├── camera_bridge/      ← MJPEG → ROS2 토픽 변환
        ├── rtsp_bridge/        ← ROS2 이미지 → RTSP 스트리밍
        ├── websocket_bridge/   ← ROS2 ↔ WebSocket (클라이언트용)
        ├── robot_description/  ← URDF 로봇 모델
        ├── robot_localization_config/ ← EKF 센서 융합 설정
        ├── robot_navigation/   ← SLAM / Navigation2 설정
        ├── hector_mapping/     ← HectorSLAM (scan-only SLAM)
        ├── navigation_manager/ ← 자율주행 미션 컨트롤러 (웨이포인트 / 순찰 / 큐)
        ├── person_tracker/     ← MediaPipe Pose 기반 사람 추종 노드
        └── fall_detection/     ← MediaPipe Pose 기반 낙상(누워있는 상태) 감지 노드
```

---

## 왜 시스템을 RPi와 WSL로 분리했는가

### 라즈베리파이 5의 하드웨어 제약

라즈베리파이 5는 RAM 8GB, 쿼드코어 ARM CPU를 탑재하고 있어 단독으로도 상당한 연산이 가능하다. 그러나 **SLAM + Nav2 + EKF + 카메라 스트리밍을 동시에 실행**하면 CPU 부하가 급격히 올라가고, 실시간성이 요구되는 하드웨어 제어(시리얼 통신, 라이다 드라이버)에 악영향을 준다.

이 프로젝트는 다음 원칙으로 역할을 분리했다.

| 구분 | 라즈베리파이 (`rsvp/`) | WSL / PC (`WSL/`) |
|------|----------------------|-------------------|
| 역할 | **실시간 하드웨어 I/O** | **고연산 소프트웨어 처리** |
| 담당 노드 | 시리얼 브릿지, 라이다 드라이버, 카메라 드라이버 | SLAM, Nav2, EKF, RTSP 서버, WebSocket 서버, 사람 추종, 낙상 감지 |
| 특성 | 지연 시간 민감 (엔코더, IMU) | 연산량 큼, 지연 허용 가능 |
| 아키텍처 | ARM64 (aarch64) | x86_64 |
| 전용 하드웨어 의존성 | libcamera, YDLiDAR SDK, /dev/serial0 | RViz2 (X11), Nav2, SLAM Toolbox |

이 분리를 통해 라즈베리파이는 센서 데이터를 안정적으로 퍼블리시하는 역할에 집중하고, 무거운 연산은 PC에서 처리한다. 두 시스템은 **같은 LAN 안에서 CycloneDDS를 통해 ROS2 토픽을 실시간으로 공유**한다.

---

## 왜 Docker를 사용하는가

### 라즈베리파이 측 (rsvp/docker/)

라즈베리파이에서 Docker를 쓰는 핵심 이유는 **의존성 충돌 방지와 재현 가능한 빌드**다.

- **libcamera v0.3.0**: 라즈베리파이 공식 libcamera를 소스에서 빌드해야 IMX219 카메라가 동작한다. 호스트 OS의 기본 libcamera 버전과 충돌하지 않도록 컨테이너 안에 격리한다.
- **YDLiDAR SDK**: 별도 C++ SDK를 빌드·설치해야 하는데, 이를 컨테이너 안에 캡슐화하면 호스트 환경을 오염시키지 않는다.
- **ARM64 패키지 종속성**: `ros-humble-*` 패키지들의 ARM64 빌드 환경을 Dockerfile 한 장으로 고정하여, 팀원 누구나 동일한 환경을 재현할 수 있다.
- **디바이스 마운트**: `docker-compose.yml`에서 `/dev/serial0`, `/dev/ttyUSB0`, `/dev/video0` 등을 컨테이너에 직접 마운트해 하드웨어 접근을 유지한다.

```yaml
# rsvp/docker/docker-compose.yml (주요 설정)
network_mode: host      # DDS UDP 멀티캐스트를 위해 필수
privileged: true        # udev 디바이스 접근
devices:
  - /dev/serial0        # STM32 UART
  - /dev/ttyUSB0        # YDLiDAR
  - /dev/video0         # IMX219 카메라
```

### WSL 측 (WSL/docker/)

WSL 환경에서 Docker를 쓰는 이유는 **ROS2 Humble 환경 일관성과 무거운 패키지 격리**다.

- **Nav2 / SLAM Toolbox / robot_localization**: 이 패키지들은 상호 의존성이 복잡하다. Dockerfile로 한 번에 설치하면 CI/CD 없이도 팀원 전원이 동일한 환경에서 실행할 수 있다.
- **X11 포워딩**: RViz2 GUI를 WSL 내 Docker 컨테이너에서 Windows 화면으로 띄우기 위해 `/tmp/.X11-unix`를 마운트하고 `DISPLAY=127.0.0.1:0`을 설정한다.
- **GStreamer 의존성**: RTSP 스트리밍에 필요한 GStreamer 플러그인들을 Dockerfile에서 관리한다.

```yaml
# WSL/docker/docker-compose.yml (주요 설정)
network_mode: host      # DDS UDP 통신을 위해 필수
environment:
  - DISPLAY=127.0.0.1:0 # RViz2 X11 포워딩
volumes:
  - /tmp/.X11-unix:/tmp/.X11-unix:rw
```

---

## DDS 통신과 네트워크 설정

### 구조

RPi와 WSL은 같은 LAN 안에 있으며, **CycloneDDS**를 미들웨어로 사용해 ROS2 토픽을 주고받는다. DDS는 자동 노드 디스커버리를 지원하지만, Docker의 `network_mode: host`를 사용하더라도 **서브넷 환경이나 WSL의 가상 네트워크 인터페이스** 때문에 자동 디스커버리가 실패하는 경우가 있다. 이를 방지하기 위해 양쪽 모두 상대방 IP를 `cyclonedds.xml`에 명시한다.

```
LAN (192.168.0.0/24)
  ├── 라즈베리파이  192.168.0.33   (rsvp/docker/cyclonedds.xml)
  └── WSL PC       192.168.0.237  (WSL/docker/cyclonedds.xml)
```

### cyclonedds.xml 설명

**rsvp/docker/cyclonedds.xml** (RPi 측)
```xml
<NetworkInterface address="192.168.0.33"/>   <!-- 자신의 IP -->
<Peer address="192.168.0.237"/>              <!-- WSL의 IP -->
<AllowMulticast>true</AllowMulticast>
```

**WSL/docker/cyclonedds.xml** (WSL 측)
```xml
<NetworkInterface address="192.168.0.237"/>  <!-- 자신의 IP -->
<Peer address="192.168.0.33"/>               <!-- RPi의 IP -->
<AllowMulticast>true</AllowMulticast>
```

### 환경 변수

두 컨테이너 모두 아래 환경 변수를 공유한다.

```bash
ROS_DOMAIN_ID=0            # 같은 도메인 ID 필수
ROS_LOCALHOST_ONLY=0       # 외부 네트워크 허용 (반드시 0)
RMW_IMPLEMENTATION=rmw_cyclonedds_cpp  # DDS 구현체 통일
```

> **주의**: `ROS_LOCALHOST_ONLY=1`로 설정하면 외부 노드가 보이지 않는다.

### DDS 포트 포워딩 (라우터/방화벽 환경)

RPi와 WSL이 **같은 LAN**에 있다면 포트 포워딩은 불필요하다.
외부 네트워크를 통해 연결해야 하거나 방화벽이 있는 경우에는 아래 포트를 열어야 한다.

| 포트 범위 | 프로토콜 | 용도 |
|-----------|----------|------|
| 7400–7500 | UDP | DDS RTPS 디스커버리 (Participant Discovery) |
| 7401 | UDP | DDS RTPS 기본 메타트래픽 유니캐스트 |
| 7500+ | UDP | DDS 데이터 유니캐스트 (토픽별 동적 할당) |
| 7651 | UDP | CycloneDDS 멀티캐스트 기본 포트 |

**WSL 환경 특이사항**: Windows WSL2는 기본적으로 Hyper-V 가상 스위치를 통해 통신한다. `network_mode: host`를 사용하면 컨테이너가 WSL2의 네트워크 네임스페이스를 직접 사용하므로, Windows 방화벽에서 위 UDP 포트들을 WSL2 인터페이스에 대해 인바운드 허용해야 한다.

```powershell
# Windows PowerShell (관리자 권한) - WSL2 DDS 포트 허용 예시
netsh advfirewall firewall add rule name="ROS2 DDS UDP" `
  protocol=UDP dir=in localport=7400-7500 action=allow
```

### IP 변경 시 수정 방법

```bash
# 1. cyclonedds.xml 수정
#    rsvp/docker/cyclonedds.xml → <NetworkInterface address="새_RPi_IP"/>
#    WSL/docker/cyclonedds.xml  → <NetworkInterface address="새_WSL_IP"/>
#                                 <Peer address="새_RPi_IP"/>

# 2. 연결 확인
ros2 topic list   # 양쪽에서 같은 토픽이 보이면 성공
```

---

## 빠른 시작

### 1. 라즈베리파이 실행

```bash
cd rsvp/docker
docker compose up -d
docker exec -it rpi_ros2 bash

# 컨테이너 내부
cd /root/ros2_ws
colcon build --symlink-install
source install/setup.bash
ros2 launch rpi_bringup rpi_bringup.launch.py
```

### 2. WSL 실행

```bash
cd WSL/docker
docker compose up -d
docker exec -it wsl_ros2 bash

# 컨테이너 내부
cd /root/ros2_ws
colcon build --symlink-install
source install/setup.bash

# SLAM Toolbox 모드 (기본)
ros2 launch wsl_bringup wsl_bringup.launch.py

# HectorSLAM 모드 (odom/IMU 없이 라이다만으로 SLAM + Nav2)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector use_rviz:=true

# Localization 모드 (기존 맵 사용)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true
```

### 3. 연결 확인

```bash
# WSL 컨테이너 내부에서
ros2 topic list
# /scan, /wheel_odom, /imu/data, /camera/image_raw/compressed 가 보이면 정상
```

---

## 전체 시스템 토픽 흐름

### SLAM Toolbox / Localization 모드

```
[STM32] ──UART──► [rpi_serial_bridge] ──► /wheel_odom, /imu/data
[YDLiDAR X4] ────► [ydlidar_ros2_driver] ──► /scan
[IMX219] ────────► [camera_ros] ──► /camera/image_raw/compressed
                                             │
                        (DDS / LAN) ─────────┘
                                             │
                                         [WSL]
[/wheel_odom + /imu/data] ──► [robot_localization] ──► /odom
[/scan + /odom] ─────────────► [slam_toolbox] ──────► /map
[/map + /odom] ──────────────► [nav2] ─────────────► /cmd_vel
[/nav/command] ──────────────► [navigation_manager] ──► /navigate_to_pose (Nav2 Action)
[/cmd_vel] ───────────────────────────────────────► [rpi_serial_bridge] ──UART──► [STM32]
[/camera/image_raw/compressed] ──► [rtsp_bridge] ──► RTSP stream
[/camera/image_raw/compressed] ──► [person_tracker] ──► /cmd_vel, /camera/tilt (추종 시)
[/camera/image_raw/compressed] ──► [fall_detection] ──► /fall_detection/alert (낙상 시)
[map→base_footprint TF] ─────────► [websocket_bridge] ──► WebSocket (클라이언트)
[/fall_detection/alert] ─────────► [websocket_bridge] ──► WebSocket "fall_alert"
```

### HectorSLAM 모드 (`nav_mode:=hector`)

```
[YDLiDAR X4] ──► /scan ──► [hector_mapping] ──► /map, map→odom TF
[scan_relay] ──► odom→base_footprint TF (50 Hz keepalive)
[/scan + map→odom TF] ──► [nav2] ──► /cmd_vel
[/nav/command] ──► [navigation_manager] ──► /navigate_to_pose (Nav2 Action)
                   (replan_on_map_update=false: HectorSLAM 고빈도 맵 업데이트 무시)
```

> HectorSLAM 모드는 odom/IMU 없이 라이다 scan만으로 SLAM을 수행한다. `/wheel_odom`, `/imu/data`가 없어도 동작하나, 긴 직선 구간이나 특징이 없는 환경에서 드리프트가 발생할 수 있다.

---

## 관련 패키지 README

| 패키지 | 위치 | 내용 |
|--------|------|------|
| `rpi_bringup` | `rsvp/ros2_ws/src/rpi_bringup/README.md` | RPi 전체 런처 |
| `rpi_serial_bridge` | `rsvp/ros2_ws/src/rpi_serial_bridge/README.md` | STM32 시리얼 통신 |
| `wsl_bringup` | `WSL/src/wsl_bringup/README.md` | WSL 전체 런처 (hector 모드 포함) |
| `camera_bridge` | `WSL/src/camera_bridge/README.md` | MJPEG 브릿지 |
| `rtsp_bridge` | `WSL/src/rtsp_bridge/README.md` | RTSP 스트리밍 |
| `websocket_bridge` | `WSL/src/websocket_bridge/README.md` | WebSocket 브릿지 |
| `robot_description` | `WSL/src/robot_description/README.md` | 로봇 URDF 모델 |
| `robot_localization_config` | `WSL/src/robot_localization_config/README.md` | EKF 센서 융합 |
| `robot_navigation` | `WSL/src/robot_navigation/README.md` | SLAM / Nav2 |
| `hector_mapping` | `WSL/src/hector_mapping/README.md` | HectorSLAM (scan-only SLAM) |
| `navigation_manager` | `WSL/src/navigation_manager/README.md` | 자율주행 미션 컨트롤러 |
| `person_tracker` | `WSL/src/person_tracker/` | MediaPipe 사람 추종 노드 |
| `fall_detection` | `WSL/src/fall_detection/README.md` | 낙상(누워있는 상태) 감지 노드 |
