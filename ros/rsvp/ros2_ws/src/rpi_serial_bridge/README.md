# rpi_serial_bridge

STM32와 라즈베리파이 사이의 시리얼 통신을 담당하는 ROS2 패키지.
STM32가 전송하는 엔코더/IMU 데이터를 수신하여 ROS2 토픽으로 퍼블리시하고,
`/cmd_vel` 토픽을 받아 모터 제어 명령을 STM32로 전송한다.

---

## 왜 이 패키지가 필요한가

이 로봇은 모터 제어와 IMU/엔코더 처리를 STM32 마이크로컨트롤러에 위임한다.
라즈베리파이와 STM32는 UART 시리얼로 연결되어 있으며, ROS2 생태계와 임베디드 MCU 사이의 데이터 변환이 필요하다.

`rpi_serial_bridge`는 이 변환 레이어 역할을 한다.

- **STM32 → RPi**: 엔코더 틱 카운트 + MPU6050 IMU 6축 데이터 수신
- **RPi → STM32**: 좌/우 바퀴 속도 명령 전송 (mm/s)
- **오도메트리 계산**: 엔코더 틱 → 위치(x, y) + 방향(θ) 누적 계산
- **TF 발행**: `odom` → `base_footprint` 좌표 변환

---

## 패키지 구조

```
rpi_serial_bridge/
├── include/rpi_serial_bridge/
│   ├── serial_bridge_node.hpp   ← 노드 클래스 선언
│   ├── serial_protocol.hpp      ← 바이너리 프로토콜 파서
│   └── odometry_calculator.hpp  ← 차동 구동 오도메트리 계산
├── src/
│   ├── main.cpp
│   ├── serial_bridge_node.cpp   ← 메인 노드 구현
│   ├── serial_protocol.cpp      ← 패킷 파싱 / 생성
│   └── odometry_calculator.cpp  ← 엔코더 → 위치 계산
├── config/
│   └── serial_bridge_params.yaml ← 시리얼 포트, 로봇 물리 파라미터
└── launch/
    └── serial_bridge.launch.py
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select rpi_serial_bridge
source install/setup.bash
```

---

## 사용법

### 단독 실행

```bash
ros2 launch rpi_serial_bridge serial_bridge.launch.py
```

### rpi_bringup을 통한 실행 (권장)

```bash
ros2 launch rpi_bringup rpi_bringup.launch.py
```

---

## 토픽

### 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/wheel_odom` | `nav_msgs/Odometry` | 엔코더 기반 휠 오도메트리 |
| `/imu/data` | `sensor_msgs/Imu` | MPU6050 6축 IMU 데이터 |
| TF: `odom` → `base_footprint` | — | 오도메트리 좌표 변환 |

### 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/cmd_vel` | `geometry_msgs/Twist` | 선속도(linear.x), 각속도(angular.z) 명령 |
| `/emergency_stop` | `std_msgs/Bool` | 비상 정지 / 해제 |
| `/serial_bridge/cmd_log` | `std_msgs/Bool` | STM32 커맨드 로그 시작/정지 |

---

## 파라미터 (`serial_bridge_params.yaml`)

| 파라미터 | 기본값 | 설명 |
|----------|--------|------|
| `serial_port` | `/dev/serial0` | UART 포트 |
| `baud_rate` | `115200` | 통신 속도 |
| `wheel_base` | `0.16` m | 좌우 바퀴 간격 |
| `wheel_radius` | `0.033` m | 바퀴 반지름 |
| `encoder_ticks_per_rev` | `20` | 엔코더 한 바퀴당 틱 수 |
| `imu_accel_scale` | `0.005875` | 가속도계 스케일 (m/s² per LSB) |
| `imu_gyro_scale` | `0.0001332` | 자이로 스케일 (rad/s per LSB) |
| `cmd_vel_repeat_rate` | `5.0` Hz | STM32 타임아웃 방지용 명령 재전송 주기 |
| `min_angular_vel` | `0.30` rad/s | angular.z 최소값 강제 적용 (정지 마찰 극복) |
| `odom_data_type` | `"ticks"` | `"ticks"` 또는 `"velocity"` |
| `odom_frame_id` | `"odom"` | 오도메트리 기준 프레임 |
| `base_frame_id` | `"base_footprint"` | 로봇 기준 프레임 |
| `imu_frame_id` | `"imu_link"` | IMU 프레임 |

---

## angular.z 최소값 강제 적용

로봇의 정지 마찰로 인해 작은 angular.z 값으로는 회전이 불가능하다.
`cmdVelCallback`에서 `min_angular_vel` 미만의 non-zero angular.z는 자동으로 최소값으로 올려서 STM32에 전송한다.

```
|angular.z| ≤ 0.001       → 0 (정지 유지)
0.001 < |angular.z| < 0.30 → ±0.30
|angular.z| ≥ 0.30        → 그대로
```

`min_angular_vel` 파라미터로 조정 가능하다.

---

## STM32 커맨드 로그

STM32에 실제로 전송되는 명령을 CSV 파일로 기록한다.

### 로그 시작
```bash
ros2 topic pub --once /serial_bridge/cmd_log std_msgs/Bool "{data: true}"
```

### 로그 정지
```bash
ros2 topic pub --once /serial_bridge/cmd_log std_msgs/Bool "{data: false}"
```

로그 파일 위치: `/root/ros2_ws/log/cmd_stm32_YYYYMMDD_HHMMSS.csv`

CSV 컬럼: `timestamp, linear_x, angular_z, left_mm_s, right_mm_s`

---

## 비상 정지

```bash
# 비상 정지
ros2 topic pub --once /emergency_stop std_msgs/Bool "{data: true}"

# 해제
ros2 topic pub --once /emergency_stop std_msgs/Bool "{data: false}"
```

---

## 시리얼 프로토콜

STM32와의 통신은 커스텀 바이너리 프로토콜을 사용한다.

- **STM32 → RPi**: 오도메트리 패킷 (좌/우 엔코더 틱 또는 속도), IMU 패킷 (accel 3축 + gyro 3축)
- **RPi → STM32**: 속도 명령 패킷 (좌/우 바퀴 mm/s, int16_t), 비상 정지 패킷

---

## 트러블슈팅

### 시리얼 포트 권한 오류

```bash
sudo usermod -aG dialout $USER   # 호스트에서
# 또는 Docker에서는 privileged: true 확인
```

### 오도메트리가 누적되지 않음

`odom_data_type` 파라미터가 STM32 펌웨어 전송 방식과 일치하는지 확인한다.
펌웨어가 틱 카운트를 보내면 `"ticks"`, 속도를 보내면 `"velocity"`로 설정한다.
