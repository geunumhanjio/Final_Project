# wsl_bringup

WSL 전체 시스템 런처 패키지 (CCTV-SLAM 순찰 로봇 - 근엄한조)

---

## 패키지 구조

```
wsl_bringup/
├── launch/
│   ├── wsl_bringup.launch.py       ← 메인 런처
│   ├── rtsp_bridge.launch.py       ← RTSP 브릿지 단독
│   ├── websocket_bridge.launch.py  ← WebSocket 브릿지 단독
│   ├── robot_core.launch.py        ← robot_description + EKF 단독
│   ├── slam_mapping.launch.py      ← SLAM 새 맵 생성 단독
│   ├── navigation.launch.py        ← 기존 맵 Localization 단독
│   └── rviz2.launch.py             ← RViz2 / rqt (선택)
└── config/
    └── rviz/
        ├── slam.rviz               ← SLAM 모드용 RViz2 설정
        └── navigation.rviz         ← Navigation 모드용 RViz2 설정
```

### 연동 패키지 (외부 launch 포함)

| 인자 | 패키지 | 설명 |
|------|--------|------|
| `use_tracker:=true` | `person_tracker` | MediaPipe 사람 추종 노드 |
| `use_fall_detection:=true` | `fall_detection` | 낙상(누워있는 상태) 감지 노드 |

---

## 설치

```bash
cp -r wsl_bringup ~/ros2_ws/src/
cd ~/ros2_ws
colcon build --packages-select wsl_bringup
source install/setup.bash
```

---

## 사용법

### SLAM 모드 — 새 맵 생성 (기본, SLAM Toolbox)
```bash
ros2 launch wsl_bringup wsl_bringup.launch.py
```

### HectorSLAM 모드 — scan-only 맵 생성 (odom/IMU 불필요)
```bash
# scan-only (odom 없이 순수 라이다만으로 SLAM + Nav2)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector use_rviz:=true

# odom 연동 (map→odom TF 발행, 네비게이션 권장)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector hector_odom_frame:=odom use_rviz:=true
```

`nav_mode:=hector` 시 다음이 자동으로 함께 실행된다.
- `hector_mapping`: HectorSLAM (scan-only SLAM)
- `robot_navigation/nav2`: Nav2 스택 (HectorSLAM이 map→odom TF 발행)
- `navigation_manager`: 웨이포인트 / 순찰 미션 컨트롤러 (`replan_on_map_update=false`)
- `scan_relay`: `publish_tf:=true` 모드 (odom→base_footprint TF 직접 발행)

> `hector_odom_frame` 기본값은 `odom`. `base_footprint`로 설정하면 map→odom TF 없이 순수 scan-only 테스트 가능.

### Localization 모드 — 기존 맵 사용
```bash
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization
```

`nav_mode:=localization` 시 다음이 자동으로 함께 실행된다.
- `robot_navigation`: localization + Nav2 스택
- `navigation_manager`: 웨이포인트 / 순찰 미션 컨트롤러

### RViz2 함께 실행
```bash
# SLAM + RViz2
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=slam use_rviz:=true

# HectorSLAM + RViz2
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=hector use_rviz:=true

# Localization + RViz2
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true

# RViz2 + rqt 둘 다
ros2 launch wsl_bringup wsl_bringup.launch.py use_rviz:=true use_rqt:=true
```

### 낙상 감지 활성화
```bash
# 낙상 감지만 추가
ros2 launch wsl_bringup wsl_bringup.launch.py use_fall_detection:=true

# 사람 추종 + 낙상 감지 동시에
ros2 launch wsl_bringup wsl_bringup.launch.py use_tracker:=true use_fall_detection:=true
```

### 자주 쓰는 조합
```bash
# 통신 레이어만 (디버깅)
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=none

# RTSP/웹소켓 없이 네비만
ros2 launch wsl_bringup wsl_bringup.launch.py \
  use_rtsp:=false use_websocket:=false \
  nav_mode:=localization use_rviz:=true
```

### 각 노드 단독 실행
```bash
ros2 launch wsl_bringup rtsp_bridge.launch.py
ros2 launch wsl_bringup websocket_bridge.launch.py
ros2 launch wsl_bringup robot_core.launch.py
ros2 launch wsl_bringup slam_mapping.launch.py
ros2 launch wsl_bringup navigation.launch.py
ros2 launch wsl_bringup rviz2.launch.py use_rviz:=true rviz_mode:=slam
```

---

## Launch Arguments

| Argument | Default | 설명 |
|----------|---------|------|
| `use_rtsp` | `true` | RTSP 브릿지 활성화 |
| `use_websocket` | `true` | WebSocket 브릿지 활성화 |
| `use_robot_core` | `true` | robot_description + EKF 활성화 |
| `nav_mode` | `slam` | `slam` / `hector` / `localization` / `none` |
| `use_rviz` | `false` | RViz2 실행 |
| `use_rqt` | `false` | rqt 실행 |
| `use_tracker` | `false` | MediaPipe 사람 추종 노드 활성화 |
| `use_fall_detection` | `false` | 낙상(누워있는 상태) 감지 노드 활성화 |
| `hector_odom_frame` | `odom` | HectorSLAM odom 프레임 (`odom` = map→odom TF 발행, `base_footprint` = scan-only) |

---

## RViz2 설정

`nav_mode` 값에 따라 자동으로 적합한 설정 파일이 선택됩니다.

| nav_mode | 사용되는 설정 파일 | 표시 항목 |
|----------|------------------|-----------|
| `slam` | `config/rviz/slam.rviz` | Grid, RobotModel, LaserScan, Map, Odometry |
| `hector` | `config/rviz/slam.rviz` | Grid, RobotModel, LaserScan, Map, Odometry |
| `localization` | `config/rviz/navigation.rviz` | 위 항목 + Costmap (Local/Global), Global/Local Plan |

설정 파일을 수정하려면 `config/rviz/` 안의 `.rviz` 파일을 직접 편집하거나, RViz2에서 수정 후 `File > Save Config As`로 덮어씁니다.

---

## 새 패키지 추가 방법

### 1. 단독 런처 파일 생성
`wsl_bringup/launch/new_module.launch.py` 를 아래 템플릿으로 작성합니다.

```python
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.substitutions import FindPackageShare
from launch.substitutions import PathJoinSubstitution

def generate_launch_description():
    return LaunchDescription([
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource([
                PathJoinSubstitution([
                    FindPackageShare('new_package'),
                    'launch', 'new_package.launch.py',
                ])
            ]),
        )
    ])
```

### 2. 메인 런처에 등록
`wsl_bringup.launch.py`에 아래 패턴으로 추가합니다.

```python
# Argument 선언
DeclareLaunchArgument('use_new_module', default_value='false'),

# Include 추가
IncludeLaunchDescription(
    PythonLaunchDescriptionSource([
        PathJoinSubstitution([pkg_share, 'launch', 'new_module.launch.py'])
    ]),
    condition=IfCondition(LaunchConfiguration('use_new_module')),
),
```

### 3. package.xml에 의존성 추가
```xml
<exec_depend>new_package</exec_depend>
```

