# Nav2 자율 주행 시스템 가이드

> 이 문서는 현재 프로젝트에서 실제로 사용 중인 Nav2 구성 전체를 정리한 레퍼런스다.
> 하드웨어: 차동구동 로봇, YDLiDAR X4, wheel_base=0.16m, wheel_radius=0.033m

---

## 1. 전체 아키텍처

```
[라즈베리파이]                         [WSL]
  ydlidar  ──────── /scan ──────────▶ slam_toolbox ──▶ /map, TF(map→odom)
  serial_bridge ─── /wheel_odom ───▶ ekf_filter ─────▶ /odom, TF(odom→base_footprint)
  serial_bridge ◀── /cmd_vel ────── controller_server ◀── planner_server
                                                            ▲
                                                     navigation_manager
                                                            ▲
                                                     /nav/command (JSON)
```

---

## 2. 두 가지 운용 모드

### 2-1. SLAM 모드 (맵 생성)

처음 공간을 탐색해서 지도를 만드는 단계.

```bash
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=slam use_rviz:=true
```

- `slam_toolbox` (`async_slam_toolbox_node`) 실행
- 로봇을 움직이면서 실시간으로 지도 생성
- 완료 후 지도 저장:

```bash
ros2 run nav2_map_server map_saver_cli -f ~/ros2_ws/src/robot_navigation/maps/my_map
```

저장 결과물:
| 파일 | 용도 |
|------|------|
| `my_map.pgm` | 점유 격자 지도 이미지 |
| `my_map.yaml` | 해상도, 원점 메타데이터 |
| `my_map.posegraph` | localization 모드에 필요한 포즈 그래프 |
| `my_map.data` | localization 모드에 필요한 데이터 |

### 2-2. Localization 모드 (자율 주행)

저장된 지도로 자기 위치를 파악하고 자율 주행하는 단계.

```bash
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true
```

**⚠ 반드시 초기 위치 설정 필요**
RViz2 상단 툴바 → `2D Pose Estimate` → 맵 위 로봇 실제 위치에 화살표 찍기.
이 작업을 하지 않으면 `map → odom` TF가 정렬되지 않아 경로 계획이 불가능하다.

---

## 3. localization 모드에서 실행되는 노드 전체 목록

| 노드 | 패키지 | 역할 |
|------|--------|------|
| `slam_toolbox` | slam_toolbox | 저장된 맵 로드, map→odom TF 발행 |
| `controller_server` | nav2_controller | 로컬 플래너 (RPP) + 로컬 costmap |
| `smoother_server` | nav2_smoother | 글로벌 경로 스무딩 |
| `planner_server` | nav2_planner | 글로벌 플래너 (NavFn) + 글로벌 costmap |
| `behavior_server` | nav2_behaviors | Recovery behavior (spin, backup, wait) |
| `bt_navigator` | nav2_bt_navigator | BT 기반 네비게이션 실행기 |
| `waypoint_follower` | nav2_waypoint_follower | 다중 웨이포인트 순차 실행 |
| `lifecycle_manager_navigation` | nav2_lifecycle_manager | 위 Nav2 노드 lifecycle 관리 |
| `navigation_manager` | navigation_manager | 웨이포인트/순찰 미션 컨트롤러 |

---

## 4. TF 트리

```
map
 └── odom                ← slam_toolbox (localization 모드)가 발행
      └── base_footprint ← ekf_filter_node가 발행
           ├── base_link
           ├── laser_frame
           ├── imu_link
           ├── camera_link
           ├── left_wheel_link
           ├── right_wheel_link
           └── ...
```

---

## 5. SLAM Toolbox 설정 (`slam_params.yaml`)

### 핵심 파라미터

| 파라미터 | 값 | 설명 |
|----------|-----|------|
| `resolution` | 0.05 m | 맵 해상도 (5cm/픽셀) |
| `minimum_laser_range` | 0.15 m | 근접 노이즈 제거 (로봇 몸체 반사 방지) |
| `max_laser_range` | 8.0 m | 실내 환경 기준 |
| `minimum_travel_distance` | 0.05 m | 새 노드 추가 최소 이동 거리 |
| `minimum_travel_heading` | 0.1 rad | 새 노드 추가 최소 회전 각도 |
| `transform_publish_period` | 0.02 s | TF 발행 주기 (50Hz) |

### 루프 클로저 튜닝 (이중 복도 대응)

| 파라미터 | 값 | 조정 이유 |
|----------|-----|---------|
| `loop_match_minimum_chain_size` | 5 | 기본 10에서 완화 → 더 자주 루프 클로저 시도 |
| `loop_match_minimum_response_coarse` | 0.25 | 0.35에서 완화 → coarse 통과 기준 낮춤 |
| `loop_match_minimum_response_fine` | 0.35 | 0.45에서 완화 → fine 통과 기준 낮춤 |
| `correlation_search_space_dimension` | 0.5 m | 1.0m에서 축소 → false optimum 방지 |
| `coarse_search_angle_offset` | 0.524 rad (30°) | 기본 20°에서 확대 → PID yaw 편향 대응 |

---

## 6. Nav2 파라미터 (`nav2_params.yaml`)

### 6-1. 글로벌 플래너 (`planner_server`)

```yaml
GridBased:
  plugin: "nav2_navfn_planner/NavfnPlanner"
  tolerance: 0.3        # 목표 근처 허용 오차
  use_astar: false      # Dijkstra 사용 (안정적)
  allow_unknown: true   # 미탐색 영역 통과 허용
```

NavFn/Dijkstra를 사용한다. A*보다 느리지만 실내 환경에서 안정적이다.

### 6-2. 로컬 플래너 (`controller_server`) — Regulated Pure Pursuit

```yaml
FollowPath:
  plugin: "nav2_regulated_pure_pursuit_controller::RegulatedPurePursuitController"
  desired_linear_vel: 0.37          # 기본 주행 속도 (m/s)
  min_approach_linear_velocity: 0.28 # 목표 근처 최소 선속도 (정지 마찰 극복)
  rotate_to_heading_angular_vel: 0.45 # 방향 전환 각속도 (rad/s)
  lookahead_dist: 0.4               # 전방 추적 거리
  use_rotate_to_heading: true       # 경로 방향으로 먼저 회전 후 전진
  rotate_to_heading_min_angle: 0.785 # 45° 이상 차이 나면 제자리 회전
  allow_reversing: false
  use_collision_detection: true
```

**RPP 동작 방식**: 경로 위의 lookahead point를 추적하며 곡률을 계산해 angular velocity를 결정한다. 목표 방향과 45° 이상 차이 나면 `rotate_to_heading`으로 먼저 제자리 회전 후 전진한다.

### 6-3. Goal Checker / Progress Checker

```yaml
general_goal_checker:
  xy_goal_tolerance: 0.15  # 목표 도달 허용 오차 15cm
  yaw_goal_tolerance: 0.25 # ~14도

progress_checker:
  required_movement_radius: 0.5  # 10초 안에 50cm 이동 못하면 실패
  movement_time_allowance: 10.0
```

### 6-4. Recovery Behavior (`behavior_server`)

| Behavior | 동작 | 트리거 조건 |
|---------|------|-----------|
| `spin` | 제자리 360° 회전 | 경로 막힘 |
| `backup` | 후진 | 앞쪽 장애물 |
| `wait` | 대기 | 동적 장애물 |

```yaml
min_rotational_vel: 0.45  # recovery spin 최소 각속도 (정지 마찰 극복)
max_rotational_vel: 0.8
```

### 6-5. 경로 스무더 (`smoother_server`)

```yaml
simple_smoother:
  plugin: "nav2_smoother::SimpleSmoother"
  tolerance: 1.0e-10
  max_its: 1000
  do_refinement: true
```

### 6-6. Costmap

**로컬 costmap** (controller_server용, odom 프레임)

```yaml
width: 3m, height: 3m, resolution: 0.05m
rolling_window: true        # 로봇 중심으로 이동
robot_radius: 0.12m
plugins: voxel_layer + inflation_layer
inflation_radius: 0.20m     # 장애물 팽창 반경
```

**글로벌 costmap** (planner_server용, map 프레임)

```yaml
resolution: 0.05m
robot_radius: 0.12m
plugins: static_layer + obstacle_layer + inflation_layer
inflation_radius: 0.20m
```

`inflation_radius`는 RViz에서 빨간/파란 영역으로 표시된다. 값이 클수록 장애물을 넓게 피하고 좁은 공간을 못 지나간다.

### 6-7. cmd_vel 흐름

```
controller_server ──▶ /cmd_vel ──▶ serial_bridge ──▶ STM32
behavior_server   ──▶ /cmd_vel ──▶ serial_bridge ──▶ STM32
```

> velocity_smoother 없음. controller_server와 behavior_server가 `/cmd_vel`에 직접 발행한다.
> (smoother 사용 시 recovery spin 중 /cmd_vel 덮어쓰기 문제 발생했기 때문)

---

## 7. navigation_manager 명령 인터페이스

Nav2 위에서 동작하는 미션 레이어. `/nav/command` 토픽으로 제어한다.

### 명령 형식 (JSON)

```bash
# 좌표 직접 지정
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"goto\", \"x\": 1.0, \"y\": 0.5, \"yaw\": 0.0}"}'

# 웨이포인트 이름으로 이동
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"goto_wp\", \"name\": \"point_a\"}"}'

# 순찰 시작 (루프)
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"patrol\", \"route\": \"default\"}"}'

# 취소
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"cancel\"}"}'
```

### 상태 모니터링

```bash
ros2 topic echo /nav/status   # IDLE / NAVIGATING / RECOVERING / FAILED
ros2 topic echo /nav/feedback  # 남은 거리, ETA
```

### 상태 머신

```
IDLE ──(goto/patrol)──▶ NAVIGATING ──(도착)──▶ IDLE
                               └──(실패)──▶ RECOVERING ──(재시도)──▶ NAVIGATING
                                                       └──(횟수 초과)──▶ FAILED
```

실패 시 최대 2회 재시도, 재시도 간격 2초.

---

## 8. 자율 주행 실행 순서 체크리스트

```
[ ] 1. RPi: ros2 launch rpi_bringup rpi_bringup.launch.py
[ ] 2. WSL: ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true
[ ] 3. 모든 Nav2 노드 active 확인: ros2 lifecycle list bt_navigator  → "active"
[ ] 4. RViz2에서 2D Pose Estimate로 초기 위치 설정
[ ] 5. /nav/status가 "IDLE" 상태인지 확인
[ ] 6. 명령 전송
```

### Nav2 lifecycle 상태 확인

```bash
ros2 lifecycle list bt_navigator        # active 여야 함
ros2 lifecycle list controller_server   # active 여야 함
ros2 lifecycle list planner_server      # active 여야 함
```

---

## 9. 주요 토픽 요약

| 토픽 | 방향 | 타입 | 설명 |
|------|------|------|------|
| `/nav/command` | → navigation_manager | `std_msgs/String` (JSON) | 네비게이션 명령 |
| `/nav/status` | navigation_manager → | `std_msgs/String` (JSON) | 현재 상태 (1Hz) |
| `/nav/feedback` | navigation_manager → | `std_msgs/String` (JSON) | 남은 거리, ETA |
| `/goal_pose` | → bt_navigator | `geometry_msgs/PoseStamped` | RViz 2D Goal Pose |
| `/plan` | planner_server → | `nav_msgs/Path` | 글로벌 경로 |
| `/cmd_vel` | controller/behavior → | `geometry_msgs/Twist` | 모터 명령 |
| `/map` | slam_toolbox → | `nav_msgs/OccupancyGrid` | 점유 격자 지도 |
| `/scan` | → slam_toolbox, costmap | `sensor_msgs/LaserScan` | LiDAR |
| `/odom` | ekf → nav2 | `nav_msgs/Odometry` | 융합 오도메트리 |

---

## 10. Plugin 이름 규칙 (ROS2 Humble)

Nav2 Humble에서 플러그인 이름 표기가 패키지별로 다르다.

| `/` 사용 | `::` 사용 |
|----------|----------|
| `nav2_navfn_planner/NavfnPlanner` | `nav2_costmap_2d::VoxelLayer` |
| `nav2_behaviors/Spin` | `nav2_costmap_2d::InflationLayer` |
| `nav2_behaviors/BackUp` | `nav2_costmap_2d::StaticLayer` |
| `nav2_behaviors/Wait` | `nav2_costmap_2d::ObstacleLayer` |
| `nav2_bt_navigator::NavigateToPoseNavigator` | `nav2_controller::SimpleProgressChecker` |
| | `nav2_regulated_pure_pursuit_controller::RegulatedPurePursuitController` |
| | `nav2_smoother::SimpleSmoother` |
| | `nav2_waypoint_follower::WaitAtWaypoint` |

> 잘못된 표기 사용 시 lifecycle 노드가 FATAL로 종료되며 이후 노드들도 연쇄적으로 실패한다.
> 에러 메시지의 **"Declared types are"** 부분을 확인해서 올바른 표기를 찾는다.
