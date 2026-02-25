# robot_navigation

SLAM 맵 생성과 Nav2 기반 자율 주행 로컬라이제이션 설정 패키지.
두 가지 모드를 지원한다: **SLAM 모드** (새 지도 생성)와 **Localization 모드** (기존 지도로 자율 주행).

---

## 왜 이 패키지가 필요한가

자율 주행 로봇은 두 가지 지식이 필요하다.

1. **지도**: 환경의 장애물 위치를 나타내는 격자 지도 (OccupancyGrid)
2. **자기 위치**: 지도 안에서 로봇이 현재 어디 있는가

이 패키지는 두 단계를 모두 다룬다.

- **SLAM 모드** (`slam_toolbox`): 라이다 데이터를 실시간으로 처리하여 지도를 생성하면서 동시에 자기 위치도 추정한다. 처음 공간을 탐색할 때 사용한다.
- **Localization 모드** (`nav2_amcl` + `nav2_bringup`): 이미 만들어진 지도를 불러와 로봇이 자기 위치를 파악하고 목적지까지 자율 주행한다. 실제 순찰 임무에 사용한다.

---

## 패키지 구조

```
robot_navigation/
├── config/
│   └── slam_params.yaml        ← SLAM Toolbox 파라미터
├── launch/
│   ├── slam.launch.py          ← SLAM 모드 런처
│   └── localization.launch.py  ← Localization + Nav2 런처
└── maps/
    ├── my_map.pgm              ← 저장된 점유 격자 지도 이미지
    └── my_map.yaml             ← 지도 메타데이터 (해상도, 원점 등)
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select robot_navigation
source install/setup.bash
```

---

## 사용법

### SLAM 모드 — 새 지도 생성

```bash
# 단독 실행
ros2 launch robot_navigation slam.launch.py

# wsl_bringup을 통한 실행 (권장)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=slam use_rviz:=true
```

로봇을 직접 조종하거나 키보드로 움직이면서 공간을 탐색한다.
지도가 완성되면 저장한다.

```bash
# 지도 저장 (SLAM 중에 실행)
ros2 run nav2_map_server map_saver_cli -f WSL/src/robot_navigation/maps/my_map
```

### Localization 모드 — 기존 지도로 자율 주행

```bash
# 단독 실행
ros2 launch robot_navigation localization.launch.py

# wsl_bringup을 통한 실행 (권장)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true
```

---

## 토픽

### SLAM 모드 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/map` | `nav_msgs/OccupancyGrid` | 실시간으로 생성되는 점유 격자 지도 |
| TF: `map` → `odom` | — | SLAM이 추정하는 위치 보정 변환 |

### Localization 모드 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/map` | `nav_msgs/OccupancyGrid` | 불러온 기존 지도 |
| `/plan` | `nav_msgs/Path` | Nav2가 계산한 경로 |
| TF: `map` → `odom` | — | AMCL 위치 추정 보정 |

### 주요 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/scan` | `sensor_msgs/LaserScan` | YDLiDAR 라이다 데이터 |
| `/odom` | `nav_msgs/Odometry` | EKF 융합 오도메트리 |
| `/goal_pose` | `geometry_msgs/PoseStamped` | 목적지 (Nav2 입력) |

---

## SLAM 파라미터 (`slam_params.yaml`)

SLAM Toolbox의 `async_slam_toolbox_node`를 사용한다 (온라인 비동기 모드).
주요 설정은 `config/slam_params.yaml`에서 조정한다.

---

## 지도 형식

`maps/my_map.yaml` 예시:
```yaml
image: my_map.pgm
resolution: 0.05          # 1픽셀 = 5cm
origin: [-5.0, -5.0, 0.0] # 지도 원점 (m)
negate: 0
occupied_thresh: 0.65
free_thresh: 0.196
```

`my_map.pgm`은 그레이스케일 PNG/PGM 이미지다.
- 흰색(255): 빈 공간 (free)
- 검정(0): 장애물 (occupied)
- 회색(128): 미탐색 (unknown)

---

## 트러블슈팅

### SLAM이 지도를 그리지 않음

```bash
# 라이다 토픽 확인
ros2 topic hz /scan

# TF 연결 확인 (odom → base_footprint → laser_frame 연결 필요)
ros2 run tf2_tools view_frames
```

### Localization에서 로봇 위치를 못 잡음 (AMCL)

RViz2에서 **2D Pose Estimate** 툴로 로봇의 초기 위치를 지도 위에 직접 지정한다.
또는 로봇을 약간 움직이면 파티클 필터가 수렴한다.
