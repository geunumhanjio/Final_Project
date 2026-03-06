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
- **Localization 모드** (`slam_toolbox` localization): 이미 만들어진 지도를 불러와 로봇이 자기 위치를 파악하고 목적지까지 자율 주행한다. 실제 순찰 임무에 사용한다.

---

## 패키지 구조

```
robot_navigation/
├── config/
│   ├── slam_params.yaml        ← SLAM Toolbox 파라미터
│   └── nav2_params.yaml        ← Nav2 전체 파라미터
├── launch/
│   ├── slam.launch.py          ← SLAM 모드 런처
│   ├── localization.launch.py  ← SLAM Toolbox localization 런처
│   └── nav2.launch.py          ← Nav2 스택 런처
└── maps/
    ├── my_map.pgm              ← 저장된 점유 격자 지도 이미지
    ├── my_map.yaml             ← 지도 메타데이터 (해상도, 원점 등)
    ├── my_map.posegraph        ← SLAM Toolbox 직렬화 포즈 그래프
    └── my_map.data             ← SLAM Toolbox 직렬화 데이터
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
# wsl_bringup을 통한 실행 (권장)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=slam use_rviz:=true
```

로봇을 직접 조종하거나 키보드로 움직이면서 공간을 탐색한다.
지도가 완성되면 저장한다.

```bash
# 지도 저장 (SLAM 중에 실행)
ros2 run nav2_map_server map_saver_cli -f ~/ros2_ws/src/robot_navigation/maps/my_map
```

### Localization 모드 — 기존 지도로 자율 주행

```bash
# wsl_bringup을 통한 실행 (권장)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true
```

실행 후 **RViz2에서 2D Pose Estimate로 초기 위치를 반드시 설정**해야 한다.
초기 위치를 설정하지 않으면 Nav2가 경로를 계획하지 못한다.

---

## Nav2 스택 구성 (`nav2.launch.py`)

| 노드 | 역할 |
|------|------|
| `controller_server` | 로컬 플래너 (RPP) + 로컬 costmap |
| `smoother_server` | 경로 스무딩 |
| `planner_server` | 글로벌 플래너 (NavFn/Dijkstra) + 글로벌 costmap |
| `behavior_server` | Recovery behavior (spin, backup, wait) |
| `bt_navigator` | BT 기반 고수준 네비게이션 실행기 |
| `waypoint_follower` | 다중 웨이포인트 순차 실행 |
| `lifecycle_manager` | 위 노드들의 lifecycle 관리 (autostart) |

> velocity_smoother는 제거됨. controller_server와 behavior_server가 `/cmd_vel`에 직접 발행한다.

---

## 주요 속도 파라미터 (`nav2_params.yaml`)

| 파라미터 | 값 | 설명 |
|----------|-----|------|
| `desired_linear_vel` | 0.37 m/s | 기본 주행 속도 |
| `min_approach_linear_velocity` | 0.28 m/s | 목표 근처 최소 선속도 |
| `rotate_to_heading_angular_vel` | 0.45 rad/s | 방향 전환 각속도 |
| `min_rotational_vel` | 0.45 rad/s | Recovery spin 최소 각속도 |
| `inflation_radius` | 0.20 m | 장애물 팽창 반경 |

---

## 토픽

### 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/map` | `nav_msgs/OccupancyGrid` | 지도 |
| `/plan` | `nav_msgs/Path` | Nav2가 계산한 경로 |
| `/cmd_vel` | `geometry_msgs/Twist` | 모터 제어 명령 |
| TF: `map` → `odom` | — | slam_toolbox localization이 발행 |

### 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/scan` | `sensor_msgs/LaserScan` | YDLiDAR 라이다 데이터 |
| `/odom` | `nav_msgs/Odometry` | EKF 융합 오도메트리 |
| `/navigate_to_pose` | action | Nav2 목표 액션 |

---

## 지도 형식

`maps/my_map.yaml` 예시:
```yaml
image: my_map.pgm
resolution: 0.05          # 1픽셀 = 5cm
origin: [-2.91, -1.69, 0]
negate: 0
occupied_thresh: 0.65
free_thresh: 0.25
```

`my_map.pgm`은 그레이스케일 이미지다.
- 흰색(255): 빈 공간 (free)
- 검정(0): 장애물 (occupied)
- 회색(128): 미탐색 (unknown)

Localization 모드에는 `.posegraph` + `.data` 파일이 추가로 필요하다.
