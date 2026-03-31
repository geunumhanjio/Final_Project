# rpi_bringup

라즈베리파이 전체 시스템 런처 패키지 (CCTV-SLAM 순찰 로봇 - 근엄한조)

---

## 패키지 구조

```
rpi_bringup/
├── launch/
│   ├── rpi_bringup.launch.py     ← 메인 런처
│   ├── camera.launch.py          ← IMX219 카메라 단독
│   ├── serial_bridge.launch.py   ← STM32 시리얼 브릿지 단독
│   └── lidar.launch.py           ← YDLiDAR X4 단독 (포트 오버라이드 지원)
├── config/
│   ├── rpi_params.yaml           ← 카메라 / 시리얼 파라미터
│   └── ydlidar_params.yaml       ← 라이다 파라미터 (포트 설정)
└── scripts/
    └── find_lidar_port.sh        ← 라이다 USB 포트 자동 감지 / udev 설정
```

---

## 설치

```bash
cp -r rpi_bringup ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select rpi_bringup
source install/setup.bash
```

---

## 사용법

### 전체 실행 (기본)
```bash
ros2 launch rpi_bringup rpi_bringup.launch.py
```

### 라이다 포트 지정 (yaml 수정 없이)
```bash
ros2 launch rpi_bringup rpi_bringup.launch.py lidar_port:=/dev/ttyUSB1
```

### 라이다 없이 실행
```bash
ros2 launch rpi_bringup rpi_bringup.launch.py use_lidar:=false
```

### 각 노드 단독 실행
```bash
ros2 launch rpi_bringup camera.launch.py
ros2 launch rpi_bringup serial_bridge.launch.py
ros2 launch rpi_bringup lidar.launch.py lidar_port:=/dev/ttyUSB0
```

---

## 라이다 USB 포트 문제 해결

USB 포트가 `/dev/ttyUSB0` ↔ `/dev/ttyUSB1` 사이에서 바뀌는 문제를 해결하는 방법 3가지입니다.

### 방법 1 (권장) — udev rule로 포트 영구 고정
라이다를 연결한 뒤 아래 스크립트를 실행하면 `/dev/ydlidar` 심볼릭 링크가 생성되어 포트가 바뀌어도 항상 같은 경로로 접근할 수 있습니다.

```bash
chmod +x ~/ros2_ws/install/rpi_bringup/share/rpi_bringup/scripts/find_lidar_port.sh
~/ros2_ws/install/rpi_bringup/share/rpi_bringup/scripts/find_lidar_port.sh --setup-udev
```

설정 후 실행:
```bash
ros2 launch rpi_bringup rpi_bringup.launch.py lidar_port:=/dev/ydlidar
```

또는 `ydlidar_params.yaml`에서 `port: "/dev/ydlidar"` 로 한 번만 수정해두면 이후 argument 없이도 동작합니다.

Docker 환경에서는 `docker-compose.yml`에 아래 매핑 추가:
```yaml
devices:
  - /dev/ydlidar:/dev/ydlidar
```

### 방법 2 — 런타임 argument 오버라이드
```bash
# 포트가 바뀐 경우 바로 지정
ros2 launch rpi_bringup rpi_bringup.launch.py lidar_port:=/dev/ttyUSB1
ros2 launch rpi_bringup lidar.launch.py lidar_port:=/dev/ttyUSB1
```

### 방법 3 — yaml 직접 수정 (기존 방식 개선)
`ydlidar_params.yaml`의 `port` 값만 수정하면 됩니다.
install 경로의 yaml을 수정하면 **colcon build 없이** 바로 반영됩니다.

```bash
# install 경로에서 직접 수정
nano ~/ros2_ws/install/rpi_bringup/share/rpi_bringup/config/ydlidar_params.yaml
```

---

## Launch Arguments

| Argument | Default | 설명 |
|----------|---------|------|
| `use_camera` | `true` | IMX219 카메라 활성화 |
| `use_serial` | `true` | STM32 시리얼 브릿지 활성화 |
| `use_lidar` | `true` | YDLiDAR X4 활성화 |
| `lidar_port` | `/dev/ttyUSB0` | 라이다 USB 포트 |

---

## 새 패키지 추가 방법

`rpi_bringup.launch.py`에 아래 패턴으로 추가합니다.

```python
# 1. Argument 선언
DeclareLaunchArgument('use_new_sensor', default_value='false'),

# 2. Node 추가
Node(
    package='new_sensor_pkg',
    executable='new_sensor_node',
    parameters=[params_file],
    condition=IfCondition(LaunchConfiguration('use_new_sensor')),
    output='screen',
),
```

