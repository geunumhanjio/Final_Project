# websocket_bridge

ROS2와 외부 WebSocket 클라이언트 사이에서 양방향 메시지를 중계하는 패키지.
클라이언트는 ROS2를 설치하지 않아도 WebSocket JSON 메시지로 로봇을 조종하고 상태를 수신할 수 있다.

---

## 패키지 구조

```
websocket_bridge/
├── include/websocket_bridge/
│   ├── ros_bridge.hpp        ← ROS2 노드 (토픽 pub/sub 담당)
│   └── websocket_server.hpp  ← WebSocket 서버 (websocketpp 기반)
├── src/
│   ├── main.cpp
│   ├── ros_bridge.cpp        ← ROS2 ↔ JSON 변환 로직
│   └── websocket_server.cpp  ← WebSocket 연결 관리
└── config/
    └── websocket_bridge_params.yaml
```

---

## 빌드 및 실행

```bash
colcon build --packages-select websocket_bridge
source install/setup.bash

# 단독 실행
ros2 launch wsl_bringup websocket_bridge.launch.py

# wsl_bringup 통합 실행 (권장)
ros2 launch wsl_bringup wsl_bringup.launch.py  # use_websocket 기본값 true
```

WebSocket 서버는 기본 **포트 9090**에서 대기한다.

---

## 토픽 전체 목록

### 클라이언트 → ROS2 (퍼블리시)

| 토픽 | 타입 | `type` 필드 | 설명 |
|------|------|-------------|------|
| `/nav/command` | `std_msgs/String` (JSON) | `"nav_command"` | Nav2 목표·순찰·취소 명령 |
| `/cmd_vel` | `geometry_msgs/Twist` | `"cmd_vel"` | 직접 속도 제어 |
| `/mode_control` | `std_msgs/String` | `"mode_control"` | 동작 모드 전환 |
| `/emergency_stop` | `std_msgs/Bool` | `"emergency_stop"` | 비상 정지 (즉시 cmd_vel=0) |
| `/tracking/enable` | `std_msgs/Bool` | `"tracking_enable"` | 사람 추종 ON/OFF |
| `/camera/tilt` | `std_msgs/Float32` | `"camera_tilt"` | 카메라 틸트 각도 (deg) |
| `/fall_detection/enable` | `std_msgs/Bool` | `"fall_detection_enable"` | 낙상 감지 ON/OFF |

### ROS2 → 클라이언트 (브로드캐스트)

| 토픽 | 타입 | `type` 필드 | 주기 |
|------|------|-------------|------|
| `/odom` or TF | `nav_msgs/Odometry` | `"odom"` | `odom_publish_interval` (기본 10 Hz) |
| `/map` | `nav_msgs/OccupancyGrid` | `"map"` | 맵 업데이트 시 |
| `/plan` | `nav_msgs/Path` | `"path"` | 경로 업데이트 시 |
| `/nav/status` | `std_msgs/String` | `"nav_status"` | 1 Hz |
| `/nav/feedback` | `std_msgs/String` | `"nav_feedback"` | 이동 중 |
| `/tracking/status` | `std_msgs/String` | `"tracking_status"` | 1 Hz |
| `/fall_detection/alert` | `std_msgs/String` | `"fall_alert"` | 낙상 감지 시 (이벤트) |
| `/fall_detection/status` | `std_msgs/String` | `"fall_detection_status"` | 1 Hz |

---

## WebSocket 메시지 형식

모든 메시지는 JSON이며 `type` 필드로 종류를 구분한다.

### 클라이언트 → 서버

```json
// 자율주행 목표 지점 (x, y, yaw 단위: m, m, rad)
{ "type": "nav_command", "data": { "cmd": "goto", "x": 1.0, "y": 0.5, "yaw": 0.0 } }

// 저장된 웨이포인트로 이동
{ "type": "nav_command", "data": { "cmd": "goto_wp", "name": "wp1" } }

// 순찰 시작
{ "type": "nav_command", "data": { "cmd": "patrol", "route": "default" } }

// 자율주행 취소
{ "type": "nav_command", "data": { "cmd": "cancel" } }

// 속도 직접 제어
{ "type": "cmd_vel", "data": { "linear_x": 0.2, "linear_y": 0.0, "angular_z": 0.0 } }

// 비상 정지
{ "type": "emergency_stop", "data": { "stop": true } }

// 사람 추종 ON/OFF
{ "type": "tracking_enable", "data": { "enable": true } }

// 카메라 틸트 (deg)
{ "type": "camera_tilt", "data": { "angle": 45.0 } }

// 낙상 감지 ON/OFF
{ "type": "fall_detection_enable", "data": { "enable": true } }
```

### 서버 → 클라이언트

```json
// 연결 확인 (접속 시 1회)
{ "type": "connection", "data": { "status": "connected" } }

// 오도메트리 (10 Hz)
{
  "type": "odom",
  "timestamp": 1743000000.0,
  "data": {
    "position":    { "x": 1.2, "y": 0.5, "z": 0.0 },
    "orientation": { "theta": 0.78 },
    "velocity":    { "linear_x": 0.1, "angular_z": 0.0 },
    "source":      "slam"
  }
}

// 지도 (맵 업데이트 시)
{
  "type": "map",
  "timestamp": 1743000000.0,
  "data": {
    "info": { "width": 200, "height": 200, "resolution": 0.05,
              "origin": { "x": -5.0, "y": -5.0 } },
    "data": [0, 0, -1, 100, "..."]
  }
}

// 경로 (경로 갱신 시)
{
  "type": "path",
  "timestamp": 1743000000.0,
  "data": {
    "poses": [
      { "x": 0.0, "y": 0.0, "theta": 0.0 },
      { "x": 0.5, "y": 0.1, "theta": 0.1 }
    ]
  }
}

// 자율주행 상태 (1 Hz)
{
  "type": "nav_status",
  "timestamp": 1743000000.0,
  "data": { "state": "NAVIGATING", "target": "wp1", "... " : "..." }
}

// 자율주행 피드백 (이동 중)
{
  "type": "nav_feedback",
  "timestamp": 1743000000.0,
  "data": { "distance_remaining": 1.23, "..." : "..." }
}

// 사람 추종 상태 (1 Hz)
{
  "type": "tracking_status",
  "timestamp": 1743000000.0,
  "data": { "enabled": true, "tilt_deg": 32.5, "model_ok": true }
}

// 낙상 감지 경고 (이벤트, 20초 쿨다운)
{
  "type": "fall_alert",
  "timestamp": 1743000000.0,
  "data": { "detected": true, "angle_deg": 72.3, "timestamp": 1743000000.123 }
}

// 낙상 감지 상태 (1 Hz)
{
  "type": "fall_detection_status",
  "timestamp": 1743000000.0,
  "data": { "enabled": true, "model_ok": true, "cooldown_remaining": 14.5 }
}
```

---

## 파라미터 (`config/websocket_bridge_params.yaml`)

| 파라미터 | 기본값 | 설명 |
|----------|--------|------|
| `odom_source` | `"slam"` | 위치 소스: `"slam"` = map→base_footprint TF 사용 (권장), `"odom"` = `/odom` 토픽 사용 |
| `odom_publish_mode` | `"timer"` | 전송 방식: `"timer"` = 고정 주기 (권장), `"topic"` = /odom 도착 시 전송 |
| `odom_publish_interval` | `0.1` | 전송 주기 (초). `timer`=고정 주기, `topic`=다운샘플링 간격 |

> `odom_source: "slam"` 권장. SLAM 좌표계(map 프레임)와 동일한 위치를 클라이언트에 전달한다. `"odom"`으로 설정하면 EKF 오도메트리 기준이 되어 map과 오차가 발생할 수 있다.

---

## 트러블슈팅

### 클라이언트에서 연결 불가

```bash
# 포트 9090이 열려 있는지 확인
ss -tlnp | grep 9090

# 방화벽 허용
sudo ufw allow 9090/tcp
```

### 오도메트리 주기 조정

`config/websocket_bridge_params.yaml`의 `odom_publish_interval` 값을 수정한다 (초 단위, 기본 `0.1` = 10 Hz).
