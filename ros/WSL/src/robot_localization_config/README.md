# robot_localization_config

`robot_localization` 패키지의 **Extended Kalman Filter (EKF)** 를 로봇에 맞게 설정하는 패키지.
휠 오도메트리(`/wheel_odom`)와 IMU(`/imu/data`) 두 센서를 융합하여 더 정밀한 오도메트리(`/odom`)를 생성한다.

---

## 왜 이 패키지가 필요한가

`rpi_serial_bridge`가 발행하는 `/wheel_odom`은 엔코더만으로 계산된 오도메트리다.
휠 오도메트리는 슬립(미끄러짐)이나 노면 불균일에 취약하고, 특히 **회전 방향**의 오차가 누적되면 SLAM 지도가 왜곡된다.

MPU6050 IMU의 자이로 데이터는 짧은 시간 동안의 회전을 정밀하게 측정하지만,
바이어스 드리프트로 인해 장시간 단독 사용 시 오차가 커진다.

**EKF(확장 칼만 필터)** 는 두 센서의 장점을 결합한다.
- 직선 이동: 휠 오도메트리의 위치(x, y)를 주로 참조
- 회전 이동: IMU 자이로의 각속도(yaw rate)를 추가로 반영
- 결과: 슬립에 강하고 회전 정확도가 개선된 `/odom` 토픽 생성

SLAM Toolbox와 Nav2는 이 `/odom` 토픽을 로봇 위치 추정의 기초로 사용한다.

---

## 패키지 구조

```
robot_localization_config/
├── config/
│   └── ekf.yaml        ← EKF 파라미터 설정
└── launch/
    └── ekf.launch.py   ← ekf_filter_node 런처
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select robot_localization_config
source install/setup.bash
```

---

## 사용법

```bash
# 단독 실행
ros2 launch robot_localization_config ekf.launch.py

# wsl_bringup robot_core를 통한 실행 (권장)
ros2 launch wsl_bringup robot_core.launch.py
```

---

## 토픽

### 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/wheel_odom` | `nav_msgs/Odometry` | 엔코더 기반 휠 오도메트리 |
| `/imu/data` | `sensor_msgs/Imu` | MPU6050 6축 IMU 데이터 |

### 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/odom` | `nav_msgs/Odometry` | EKF 융합 오도메트리 |
| TF: `odom` → `base_footprint` | — | 융합된 좌표 변환 |

---

## EKF 설정 (`ekf.yaml`)

### 센서 융합 설정

**`odom0` (휠 오도메트리)**
```yaml
odom0: /wheel_odom
odom0_config: [true,  true,  false,   # x, y, z 위치
               false, false, true,    # roll, pitch, yaw 방향
               true,  false, false,   # vx, vy, vz 속도
               false, false, true,    # vroll, vpitch, vyaw 각속도
               false, false, false]   # ax, ay, az 가속도
```

**`imu0` (IMU)**
```yaml
imu0: /imu/data
imu0_config: [false, false, false,   # 위치 (IMU만으로는 불가)
              false, false, false,   # 방향 (orientation 없음)
              false, false, false,   # 선속도
              true,  true,  true,    # angular velocity x,y,z 사용
              true,  true,  false]   # linear acceleration x,y 사용
```

### 주요 파라미터

| 파라미터 | 값 | 설명 |
|----------|----|------|
| `frequency` | 50 Hz | EKF 업데이트 주기 |
| `two_d_mode` | true | 2D 평면 이동 가정 (z, roll, pitch 무시) |
| `world_frame` | `odom` | 기준 좌표계 |
| `base_link_frame` | `base_footprint` | 로봇 기준 프레임 |
| `imu0_remove_gravitational_acceleration` | true | 중력 가속도 제거 |

---

## 트러블슈팅

### `/odom`이 발행되지 않음

```bash
# 입력 토픽이 들어오는지 확인
ros2 topic hz /wheel_odom
ros2 topic hz /imu/data
```

### 오도메트리 드리프트가 심함

`ekf.yaml`의 `process_noise_covariance`와 각 센서 `*_config`를 조정한다.
IMU 바이어스가 큰 경우 `imu0_config`에서 가속도 항목을 비활성화하는 것을 검토한다.
