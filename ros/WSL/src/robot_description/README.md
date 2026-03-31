# robot_description

URDF 형식으로 기술된 로봇의 물리적 모델 패키지.
SLAM, Nav2, EKF, RViz2가 로봇의 기하학적 구조를 이해하는 데 사용된다.

---

## 왜 이 패키지가 필요한가

ROS2 생태계에서 SLAM, Nav2, robot_localization 같은 패키지들은 **로봇의 형태와 센서 위치**를 알아야 동작한다.
예를 들어 Nav2는 로봇의 크기(충돌 반경)를 알아야 장애물 회피 경로를 계획하고,
SLAM Toolbox는 라이다 센서가 로봇의 어느 위치에 달려 있는지 알아야 정확한 맵을 생성한다.

이 정보를 제공하는 표준 방법이 **URDF(Unified Robot Description Format)** 이며,
`robot_state_publisher`가 URDF를 파싱하여 각 링크 간의 좌표 변환(TF)을 실시간으로 발행한다.

`robot_description`이 없으면 다음이 동작하지 않는다.
- RViz2에서 로봇 모델 시각화 불가
- SLAM Toolbox의 `base_link` → `laser_frame` 변환 누락
- Nav2의 로봇 크기 기반 충돌 체크 불가
- EKF의 `base_footprint` 프레임 참조 불가

---

## 패키지 구조

```
robot_description/
├── urdf/
│   └── robot.urdf.xacro    ← Xacro 매크로 기반 로봇 모델
├── launch/
│   └── robot_state_publisher.launch.py
└── rviz/
    └── robot.rviz           ← RViz2 기본 설정
```

---

## 로봇 모델 구조

```
base_footprint (지면 기준 프레임)
  └── base_link (로봇 몸체 중심)
        ├── left_wheel_link
        ├── right_wheel_link
        ├── front_caster_link
        ├── rear_caster_link
        ├── laser_frame      ← YDLiDAR X4 (상단 중앙)
        ├── camera_link      ← IMX219 (전면)
        │     └── camera_optical_link
        └── imu_link         ← MPU6050 on STM32 (중앙)
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select robot_description
source install/setup.bash
```

---

## 사용법

```bash
# 단독 실행
ros2 launch robot_description robot_state_publisher.launch.py

# wsl_bringup robot_core를 통한 실행 (권장)
ros2 launch wsl_bringup robot_core.launch.py
```

---

## 주요 TF 프레임

| 프레임 | 설명 |
|--------|------|
| `base_footprint` | 로봇의 지면 접촉 기준점. Nav2, SLAM의 로봇 위치 기준 |
| `base_link` | 로봇 몸체 중심 (바닥에서 `wheel_radius = 0.033m` 높이) |
| `laser_frame` | YDLiDAR X4 위치. `/scan` 토픽의 `frame_id` |
| `odom` | 오도메트리 원점. `rpi_serial_bridge`가 발행하는 TF의 부모 |

---

## 로봇 물리 치수 수정

`robot.urdf.xacro` 상단의 `<xacro:property>` 섹션에서 실측값으로 수정한다.

```xml
<xacro:property name="base_length"      value="0.220"/>  <!-- 앞뒤 220mm -->
<xacro:property name="base_width"       value="0.140"/>  <!-- 좌우 140mm -->
<xacro:property name="wheel_radius"     value="0.0329"/> <!-- 바퀴 반지름 32.9mm (serial_bridge 캘리브레이션 값과 통일) -->
<xacro:property name="wheel_separation" value="0.100"/>  <!-- 좌우 바퀴 중심 간격 100mm (실측) -->
```

수정 후 재빌드 없이 `--symlink-install`로 빌드했다면 런처 재시작만으로 반영된다.
