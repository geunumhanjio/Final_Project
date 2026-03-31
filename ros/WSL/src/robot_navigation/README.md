# robot_navigation

Nav2 기반 자율 주행 스택 설정 패키지. **HectorSLAM 모드**, SLAM Toolbox SLAM 모드, Localization 모드를 지원한다.

---

## 왜 이 패키지가 필요한가

자율 주행 로봇은 두 가지 지식이 필요하다.

1. **지도**: 환경의 장애물 위치를 나타내는 격자 지도 (OccupancyGrid)
2. **자기 위치**: 지도 안에서 로봇이 현재 어디 있는가

이 패키지는 Nav2 스택 전체의 파라미터와 런처를 관리한다.

| 모드 | 지도 소스 | 위치 추정 | 주요 용도 |
|------|----------|----------|----------|
| `nav_mode:=hector` | HectorSLAM 실시간 생성 | scan-to-map ICP | **실사용 권장** |
| `nav_mode:=slam` | SLAM Toolbox 실시간 생성 | scan + odom 융합 | 맵 생성 보조 |
| `nav_mode:=localization` | 저장된 맵 로드 | SLAM Toolbox localization | 별도 맵 파일 필요 |

> hector 모드가 권장되는 이유: WiFi 지연과 EKF jitter 영향 없이 scan-only로 가장 깨끗한 맵을 생성하고 실시간 자율 주행이 가능하다.

---

## 패키지 구조

```
robot_navigation/
├── config/
│   ├── slam_params.yaml        ← SLAM Toolbox 파라미터 (slam 모드용)
│   └── nav2_params.yaml        ← Nav2 전체 파라미터 (hector/localization 모드)
├── launch/
│   ├── slam.launch.py          ← SLAM Toolbox SLAM 모드 런처
│   ├── localization.launch.py  ← SLAM Toolbox localization 런처
│   └── nav2.launch.py          ← Nav2 스택 런처 (hector/localization 공용)
└── maps/
    ├── my_map.pgm              ← 저장된 점유 격자 지도 이미지
    ├── my_map.yaml             ← 지도 메타데이터
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

### HectorSLAM 모드 (권장)

```bash
# HectorSLAM + Nav2 + navigation_manager
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector

# RViz2 포함
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector use_rviz:=true
```

HectorSLAM이 실시간으로 맵을 생성하면서 동시에 자율 주행을 수행한다. 별도의 맵 저장/로드 단계가 없다.

RViz2에서 **2D Goal Pose** 툴로 목표 지점을 클릭하면 즉시 이동한다.

맵을 저장하려면:
```bash
ros2 run nav2_map_server map_saver_cli -f ~/ros2_ws/src/robot_navigation/maps/my_map
```

### SLAM Toolbox SLAM 모드 — 새 지도 생성

```bash
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=slam use_rviz:=true
```

로봇을 조종하면서 공간을 탐색한다. 지도가 완성되면 저장한다.

### Localization 모드 — 저장된 지도로 자율 주행

```bash
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true
```

실행 후 **RViz2에서 2D Pose Estimate로 초기 위치를 반드시 설정**해야 한다.

---

## Nav2 스택 구성 (`nav2.launch.py`)

### cmd_vel 흐름

```
controller_server ──┐
                    ├──→ /cmd_vel_raw ──→ velocity_smoother ──→ /cmd_vel ──→ 로봇
behavior_server   ──┘
```

> `velocity_smoother`를 중간에 두는 이유: controller/behavior_server가 발행하는 cmd_vel의 급격한 angular velocity 스텝(0 → 목표값 즉각 전환)을 완만하게 만들어 **회전 오버슈트 방지**. 오버슈트가 발생하면 HectorSLAM의 scan matching이 실패해 맵이 깨진다.

### 노드 구성

| 노드 | 역할 | 비고 |
|------|------|------|
| `controller_server` | 로컬 플래너 (RPP) + 로컬 costmap | cmd_vel → `/cmd_vel_raw` |
| `smoother_server` | 경로 스무딩 | BT에서 선택적 사용 |
| `planner_server` | 글로벌 플래너 (NavFn/Dijkstra) + 글로벌 costmap | — |
| `behavior_server` | Recovery behavior (spin, backup, wait) | cmd_vel → `/cmd_vel_raw` |
| `velocity_smoother` | cmd_vel 가속도 제한 | `/cmd_vel_raw` → `/cmd_vel` |
| `bt_navigator` | BT 기반 고수준 네비게이션 실행기 | `/goal_pose` → `/bt_nav_goal_pose` remap |
| `lifecycle_manager` | 위 노드들의 lifecycle 관리 (autostart) | — |

> `waypoint_follower` 제거 이유: navigation_manager가 `NavigateToPose` 액션만 사용하므로 `FollowWaypoints` 액션 서버가 불필요하다.

> `bt_navigator` `/goal_pose` remap 이유: navigation_manager가 `/goal_pose`를 처리한다. bt_navigator가 직접 구독하면 충돌이 발생하므로 `/bt_nav_goal_pose`로 remap.

---

## 주요 파라미터 (`nav2_params.yaml`)

### Controller Server (RPP)

| 파라미터 | 값 | 변경 이유 |
|----------|-----|----------|
| `controller_frequency` | `5.0 Hz` | 10→5Hz: WSL CPU 과부하 방지 |
| `failure_tolerance` | `0.5` | 0.3→0.5: WiFi 지연으로 인한 주기 누락 허용 |
| `desired_linear_vel` | `0.15 m/s` | 소형 로봇 기준 |
| `lookahead_dist` | `0.4 m` | — |
| `min_lookahead_dist` | `0.25 m` | — |
| `max_lookahead_dist` | `0.7 m` | — |
| `rotate_to_heading_angular_vel` | `0.07 rad/s` | 제자리 회전 속도 (velocity_smoother로 완만하게 가속) |
| `max_angular_accel` | `0.2 rad/s²` | 경로 추종 중 각가속도 |
| `transform_tolerance` | `0.5 s` | 0.2→0.5: WiFi jitter 흡수 |
| `movement_time_allowance` | `15.0 s` | 10→15s: 저속 로봇 + WSL 지연 고려 |
| `use_rotate_to_heading` | `true` | 경로 방향으로 먼저 회전 후 전진 |
| `rotate_to_heading_min_angle` | `0.785 rad` | 45° 이상이면 제자리 회전 |

### Velocity Smoother

| 파라미터 | 값 | 설명 |
|----------|-----|------|
| `smoothing_frequency` | `10.0 Hz` | 20→10Hz: CPU 절감 |
| `feedback` | `OPEN_LOOP` | odom 피드백 불필요 |
| `max_velocity` | `[0.15, 0.0, 0.07]` | [vx, vy, vyaw] 최대값 |
| `min_velocity` | `[-0.15, 0.0, -0.07]` | — |
| `max_accel` | `[0.08, 0.0, 0.08]` | angular accel 0.15→**0.08**: 완만한 회전 가속 |
| `max_decel` | `[-0.08, 0.0, -0.08]` | angular decel 동일: 급정지 방지 |

> `max_accel[2]` 0.15→0.08 변경 이유: 목표 angular velocity 0.07 rad/s까지 ~0.9초에 걸쳐 완만하게 가속. 기존에는 smoother가 아예 실행되지 않아 즉각 스텝으로 인한 오버슈트가 발생했음.

### Goal Checker

| 파라미터 | 값 | 설명 |
|----------|-----|------|
| `xy_goal_tolerance` | `0.15 m` | 목표 도달 XY 허용 오차 15cm |
| `yaw_goal_tolerance` | `0.25 rad` | 목표 도달 yaw 허용 오차 ~14° |

### Local Costmap

| 파라미터 | 값 | 변경 이유 |
|----------|-----|----------|
| `update_frequency` | `5.0 Hz` | 10→5Hz: WSL CPU 절감 |
| `publish_frequency` | `3.0 Hz` | 5→3Hz |
| `width / height` | `3 m` | 로컬 윈도우 크기 |
| `resolution` | `0.05 m` | 5cm/픽셀 |
| `robot_radius` | `0.12 m` | 로봇 반경 12cm |
| `inflation_radius` | `0.20 m` | 장애물 팽창 반경 (robot_radius + 마진) |

### Global Costmap

| 파라미터 | 값 | 설명 |
|----------|-----|------|
| `update_frequency` | `1.0 Hz` | — |
| `robot_radius` | `0.12 m` | — |
| `inflation_radius` | `0.20 m` | — |

### Global Planner (NavFn)

| 파라미터 | 값 | 설명 |
|----------|-----|------|
| `use_astar` | `false` | Dijkstra 사용 (안정적) |
| `allow_unknown` | `true` | 미탐색 영역 통과 허용 |
| `tolerance` | `0.3 m` | 목표 근처 도달 허용 오차 |

### Behavior Server

| 파라미터 | 값 | 변경 이유 |
|----------|-----|----------|
| `transform_tolerance` | `0.5 s` | WiFi jitter 흡수 |
| `max_rotational_vel` | `0.07 rad/s` | — |
| `min_rotational_vel` | `0.04 rad/s` | — |
| `rotational_acc_lim` | `0.15 rad/s²` | — |

---

## 토픽

### 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/cmd_vel` | `geometry_msgs/Twist` | velocity_smoother 출력 → 로봇 |
| `/cmd_vel_raw` | `geometry_msgs/Twist` | controller/behavior 출력 → velocity_smoother 입력 |
| `/plan` | `nav_msgs/Path` | Nav2가 계산한 경로 |
| `/local_costmap/costmap` | `nav_msgs/OccupancyGrid` | 로컬 costmap |

### 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/scan` | `sensor_msgs/LaserScan` | 라이다 데이터 (costmap용) |
| `/odom_relay` | `nav_msgs/Odometry` | 오도메트리 (controller 피드백용) |
| `/map` | `nav_msgs/OccupancyGrid` | 글로벌 costmap static layer |
| `/navigate_to_pose` | action | navigation_manager가 사용하는 Nav2 액션 |

---

## 지도 형식 (maps/)

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
