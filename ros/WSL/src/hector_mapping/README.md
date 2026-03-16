# hector_mapping

HectorSLAM ROS1 → ROS2 포트. scan-to-map ICP 기반 SLAM으로, **odom/IMU 없이 LiDAR 스캔만으로 동작**한다.

---

## 왜 HectorSLAM인가

기존 SLAM Toolbox를 사용했을 때 다음 문제가 있었다.

| 문제 | 원인 | HectorSLAM 해결 방식 |
|------|------|----------------------|
| WiFi 지연(~200ms)으로 odom 데이터가 scan보다 늦게 도착 | SLAM Toolbox는 odom prior를 필요로 함 | scan-to-map ICP만 사용, odom 불필요 |
| RPi↔WSL 시각 동기화 불완전으로 TF extrapolation 에러 | SLAM이 정확한 시각 정렬을 요구 | 내부에서 `this->now()` 기준으로 TF 발행 |
| EKF jitter가 맵 품질에 직접 영향 | odom prior가 pose 초기값으로 사용됨 | 이전 scan match pose에서 직접 ICP 시작 |
| SLAM Toolbox가 scan-only 모드를 제대로 지원하지 않음 | 내부 구조상 odom TF를 항상 조회 | 설계 자체가 scan-only |

> 실제 테스트 결과 HectorSLAM이 SLAM Toolbox 대비 훨씬 깨끗한 맵을 생성함.

---

## 동작 원리

```
[LiDAR /scan]
     │
     ▼
scan→base TF 변환 (use_tf_scan_transformation)
     │
     ▼
HectorSlamProcessor::update()  ← Gauss-Newton ICP
  ├─ 3단계 multi-resolution 매칭 (coarse→mid→fine)
  │   coarse: 1/4 해상도 → 큰 이동/회전도 수렴
  │   fine:   원본 해상도 → 정밀 정합
  └─ 이전 scan match pose에서 초기화
     │
     ▼
pose (x, y, yaw) 추정
     │
     ├── [navigation 모드] map→odom TF 발행
     │     odom→base_footprint TF (scan_relay 발행)와 합성하여 map→odom 계산
     └── [scan-only 모드] map→base_footprint TF 직접 발행
```

### 맵 발행 전략 (업데이트 트리거 방식)

기존 타이머 방식(2초 주기 무조건 발행) 대신, **맵이 실제로 변경됐을 때만** 발행한다.

```
scan 수신마다:
  map update index 변경됨? NO  → 발행 안 함
                        YES → 마지막 발행 후 4초 경과? NO  → 발행 안 함
                                                       YES → 발행
```

- 시작 시 빈 맵 1회 발행 → `transient_local` QoS 캐시에 저장
- WebSocket 재연결 시 캐시된 맵 즉시 수신 (추가 요청 불필요)
- 로봇이 정지 중이면 맵 발행이 아예 없음

---

## 패키지 구조

```
hector_mapping/
├── src/
│   ├── HectorMappingRos.h       ← ROS2 노드 헤더
│   ├── HectorMappingRos.cpp     ← 핵심 구현 (scan 콜백, TF, 맵 발행)
│   └── main.cpp
├── include/
│   └── hector_slam_lib/         ← 순수 C++ 알고리즘 라이브러리 (ROS 의존성 없음)
│       ├── slam_main/           ← HectorSlamProcessor
│       ├── map/                 ← GridMap
│       └── scan/                ← DataContainer
├── config/
│   └── hector_mapping_params.yaml
└── launch/
    └── hector_mapping.launch.py
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select hector_mapping
source install/setup.bash
```

---

## 사용법

### wsl_bringup을 통한 실행 (권장)

```bash
# HectorSLAM + Nav2 + navigation_manager
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector

# RViz2 포함
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector use_rviz:=true
```

### 단독 실행

```bash
# scan-only 모드 (odom 불필요, map→base_footprint 직접 발행)
ros2 launch hector_mapping hector_mapping.launch.py

# navigation 모드 (map→odom TF 발행, relay_ekf와 연동)
ros2 launch hector_mapping hector_mapping.launch.py odom_frame:=odom
```

### 맵 저장 (SLAM 중에 실행)

```bash
ros2 run nav2_map_server map_saver_cli -f ~/ros2_ws/src/robot_navigation/maps/my_map
```

### 맵 리셋 (서비스 호출)

```bash
ros2 service call /reset_map std_srvs/srv/Trigger
```

### 매핑 일시 정지 / 재개

```bash
ros2 service call /pause_mapping std_srvs/srv/SetBool "{data: true}"   # 정지
ros2 service call /pause_mapping std_srvs/srv/SetBool "{data: false}"  # 재개
```

---

## odom_frame 모드 선택

| `odom_frame` 값 | TF 발행 | 용도 |
|----------------|---------|------|
| `base_footprint` (기본) | `map → base_footprint` 직접 발행 | scan-only 테스트, odom/IMU 없을 때 |
| `odom` | `map → odom` 발행 | **Nav2 + navigation_manager 실사용** |

> **wsl_bringup `nav_mode:=hector`** 는 `odom_frame:=odom` 으로 자동 설정된다.

### nav_mode:=hector TF 체인

```
map ──(HectorSLAM)──→ odom ──(scan_relay 50Hz)──→ base_footprint ──(robot_state_pub)──→ laser_frame
```

- `map→odom`: HectorSLAM이 각 scan마다 계산해서 발행
- `odom→base_footprint`: **scan_relay**가 직접 발행 (50Hz keepalive)
  - relay_ekf 없이 운용: `/odom`은 이미 RPi EKF 출력(필터링 완료)이므로 WSL에서 추가 EKF 불필요

---

## 파라미터 (`config/hector_mapping_params.yaml`)

### 좌표계

| 파라미터 | 값 | 설명 |
|----------|----|------|
| `base_frame` | `base_footprint` | 로봇 기준 프레임 |
| `map_frame` | `map` | 지도 프레임 |
| `odom_frame` | `odom` | 런치 파일에서 오버라이드 가능 |
| `scan_topic` | `scan` | 라이다 입력 토픽 |

### TF 관련

| 파라미터 | 값 | 설명 |
|----------|----|------|
| `pub_map_odom_transform` | `true` | map→odom (또는 map→base) TF 발행 |
| `pub_map_scanmatch_transform` | `false` | scanmatcher_frame TF 비활성화 |
| `use_tf_scan_transformation` | `true` | laser_frame→base_footprint TF 조회하여 정확한 포인트 변환 |
| `pub_odometry` | `false` | Nav2가 `/odom_relay`를 직접 사용하므로 비활성화 |

### 맵 설정

| 파라미터 | 값 | 설명 |
|----------|----|------|
| `map_resolution` | `0.05` | 5cm/픽셀 |
| `map_size` | `512` | 512×512 = **25m×25m** (실내 충분) |
| `map_start_x` | `0.5` | 시작 위치: 맵 가로 50% (중앙) |
| `map_start_y` | `0.5` | 시작 위치: 맵 세로 50% (중앙) |
| `map_multi_res_levels` | `3` | **3단계** coarse→mid→fine ICP |
| `map_pub_period` | `4.0` | 맵 발행 최소 간격 (초), 업데이트 없으면 발행 안 함 |

> `map_size: 512` 선택 이유: HectorSLAM은 고정 크기 pre-allocation. 2048(102m)→512(25m)으로 줄여 발행 데이터를 원래의 **1/16**로 감소. SLAM Toolbox는 동적 확장이지만 HectorSLAM은 이 방식을 사용하지 않음.
>
> `map_multi_res_levels: 3` 선택 이유: 빠른 회전 시 coarse 레벨에서 큰 각도 변화도 수렴. 2레벨보다 회전 안정성이 높음.

### 맵 업데이트 조건

| 파라미터 | 값 | 설명 |
|----------|----|------|
| `update_factor_free` | `0.4` | 빈 셀 업데이트 가중치 |
| `update_factor_occupied` | `0.9` | 점유 셀 업데이트 가중치 |
| `map_update_distance_thresh` | `0.05` | 5cm 이동 시 맵 업데이트 |
| `map_update_angle_thresh` | `0.15` | ~9° 회전 시 맵 업데이트 (빠른 회전 중 과도한 업데이트 억제) |

> `map_update_angle_thresh: 0.087→0.15` 변경 이유: 로봇이 빠르게 회전할 때 맵 업데이트를 줄여 ICP 발산으로 인한 맵 깨짐 방지.

### 레이저 설정 (YDLiDAR X4 기준)

| 파라미터 | 값 | 설명 |
|----------|----|------|
| `laser_min_dist` | `0.12` | 최소 유효 거리 (로봇 몸체 반사 제거) |
| `laser_max_dist` | `8.0` | 최대 유효 거리 |

---

## 토픽

### 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/map` | `nav_msgs/OccupancyGrid` | SLAM 지도 (transient_local, 업데이트 시에만 발행) |
| `/map_metadata` | `nav_msgs/MapMetaData` | 지도 메타데이터 |
| `/slam_out_pose` | `geometry_msgs/PoseStamped` | SLAM 추정 포즈 (map 프레임) |
| `/poseupdate` | `geometry_msgs/PoseWithCovarianceStamped` | SLAM 포즈 + 공분산 |
| TF: `map→odom` | — | navigation 모드에서 발행 |
| TF: `map→base_footprint` | — | scan-only 모드에서 발행 |

### 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/scan` | `sensor_msgs/LaserScan` | 라이다 입력 |
| `/initialpose` | `geometry_msgs/PoseWithCovarianceStamped` | 초기 포즈 설정 (RViz 2D Pose Estimate) |

### 서비스

| 서비스 | 타입 | 설명 |
|--------|------|------|
| `/dynamic_map` | `nav_msgs/srv/GetMap` | 현재 맵 요청 |
| `/reset_map` | `std_srvs/srv/Trigger` | 맵 초기화 |
| `/pause_mapping` | `std_srvs/srv/SetBool` | 매핑 일시 정지/재개 |

---

## ROS2 포팅 주요 변경 사항

| 항목 | ROS1 | ROS2 포트 |
|------|------|-----------|
| TF 라이브러리 | `tf::TransformBroadcaster` | `tf2_ros::TransformBroadcaster` |
| TF 타임스탬프 | `scan->header.stamp` (RPi 시각) | `this->now()` (WSL 현재 시각) |
| MapLockerInterface namespace | `hectorslam::` | global namespace |
| 헤더 경로 | ROS1 find_package | `include/hector_slam_lib` 직접 포함 |
| 맵 발행 | 타이머 기반 | scan 업데이트 트리거 기반 |

> TF 타임스탬프를 `this->now()`로 변경한 이유: RPi 시각은 WiFi 지연으로 WSL보다 ~200ms 뒤처짐. scan 타임스탬프 그대로 사용하면 Nav2의 `Lookup would require extrapolation into the future` 에러 발생.
