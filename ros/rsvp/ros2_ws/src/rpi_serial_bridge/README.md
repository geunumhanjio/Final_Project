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

---

## 파라미터 (`serial_bridge_params.yaml`)

| 파라미터 | 기본값 | 설명 |
|----------|--------|------|
| `serial_port` | `/dev/serial0` | UART 포트 (`/dev/ttyAMA0` 동일) |
| `baud_rate` | `115200` | 통신 속도 |
| `wheel_base` | `0.16` m | 좌우 바퀴 간격 |
| `wheel_radius` | `0.033` m | 바퀴 반지름 |
| `encoder_ticks_per_rev` | `20` | 엔코더 한 바퀴당 틱 수 |
| `imu_accel_scale` | `0.005875` | 가속도계 스케일 (m/s² per LSB) |
| `imu_gyro_scale` | `0.0001332` | 자이로 스케일 (rad/s per LSB) |
| `cmd_vel_repeat_rate` | `5.0` Hz | STM32 타임아웃 방지용 명령 재전송 주기 |
| `odom_data_type` | `"ticks"` | `"ticks"` (엔코더 틱) 또는 `"velocity"` (mm/s) |
| `odom_frame_id` | `"odom"` | 오도메트리 기준 프레임 |
| `base_frame_id` | `"base_footprint"` | 로봇 기준 프레임 |
| `imu_frame_id` | `"imu_link"` | IMU 프레임 |

---

## 시리얼 프로토콜

STM32와의 통신은 커스텀 바이너리 프로토콜을 사용한다.

- **STM32 → RPi**: 오도메트리 패킷 (좌/우 엔코더 틱 또는 속도), IMU 패킷 (accel 3축 + gyro 3축)
- **RPi → STM32**: 속도 명령 패킷 (좌/우 바퀴 mm/s, int16_t), 비상 정지 패킷

---

## 비상 정지

`/emergency_stop` 토픽에 `True`를 발행하면 즉시 STM32에 정지 명령을 전송하고, 이후 들어오는 `/cmd_vel`을 무시한다. `False`를 발행하면 해제된다.

```bash
# 비상 정지
ros2 topic pub /emergency_stop std_msgs/Bool "data: true" --once

# 해제
ros2 topic pub /emergency_stop std_msgs/Bool "data: false" --once
```

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
