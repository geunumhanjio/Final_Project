# websocket_bridge

ROS2와 외부 WebSocket 클라이언트(Qt 앱 등) 사이에서 양방향 메시지를 중계하는 패키지.
클라이언트는 ROS2를 설치하지 않아도 WebSocket JSON 메시지로 로봇을 조종하고 상태를 수신할 수 있다.

---

## 왜 이 패키지가 필요한가

Nav2나 teleop_twist_keyboard 같은 ROS2 클라이언트 없이 **범용 WebSocket 클라이언트**(Qt, 웹앱, Python 스크립트 등)에서 로봇을 제어해야 할 경우, ROS2 네이티브 연결 없이도 로봇과 통신할 수 있는 브릿지가 필요하다.

`websocket_bridge`는 다음을 제공한다.

- **클라이언트 → 로봇**: 목표 지점 설정(`/goal_pose`), 속도 직접 제어(`/cmd_vel`), 모드 전환(`/mode_control`), 비상 정지(`/emergency_stop`)
- **로봇 → 클라이언트**: 오도메트리(`/odom`), 지도(`/map`), 경로(`/plan`), 로봇 상태(`/robot_status`)를 JSON으로 브로드캐스트

---

## 패키지 구조

```
websocket_bridge/
├── include/websocket_bridge/
│   ├── ros_bridge.hpp       ← ROS2 노드 (토픽 pub/sub 담당)
│   └── websocket_server.hpp ← WebSocket 서버 (Boost.Beast 기반)
└── src/
    ├── main.cpp
    ├── ros_bridge.cpp       ← ROS2 ↔ JSON 변환 로직
    └── websocket_server.cpp ← WebSocket 연결 관리
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select websocket_bridge
source install/setup.bash
```

---

## 사용법

```bash
# 단독 실행
ros2 run websocket_bridge websocket_bridge_node

# wsl_bringup을 통한 실행 (권장)
ros2 launch wsl_bringup wsl_bringup.launch.py use_websocket:=true
```

WebSocket 서버는 기본적으로 **포트 9090**에서 대기한다.

---

## 토픽

### 퍼블리시 (클라이언트 메시지 → ROS2)

| 토픽 | 타입 | 클라이언트 메시지 type 필드 |
|------|------|-----------------------------|
| `/goal_pose` | `geometry_msgs/PoseStamped` | `"goal_pose"` |
| `/cmd_vel` | `geometry_msgs/Twist` | `"cmd_vel"` |
| `/mode_control` | `std_msgs/String` | `"mode_control"` |
| `/emergency_stop` | `std_msgs/Bool` | `"emergency_stop"` |

### 서브스크라이브 (ROS2 → 클라이언트 브로드캐스트)

| 토픽 | 타입 | 브로드캐스트 type 필드 | 다운샘플링 |
|------|------|-----------------------|------------|
| `/odom` | `nav_msgs/Odometry` | `"odom"` | 10 Hz |
| `/map` | `nav_msgs/OccupancyGrid` | `"map"` | 0.2 Hz (5초마다) |
| `/plan` | `nav_msgs/Path` | `"path"` | 매 업데이트 |
| `/robot_status` | `std_msgs/String` | `"status"` | 매 업데이트 |

---

## WebSocket 메시지 형식

모든 메시지는 JSON이며, `type`과 `data` 필드를 가진다.

### 클라이언트 → 서버 (송신)

```json
// 목표 지점 설정
{
  "type": "goal_pose",
  "data": { "x": 1.5, "y": 2.0, "theta": 1.57, "frame_id": "map" }
}

// 직접 속도 제어
{
  "type": "cmd_vel",
  "data": { "linear_x": 0.2, "linear_y": 0.0, "angular_z": 0.0 }
}

// 모드 전환
{
  "type": "mode_control",
  "data": { "mode": "auto" }
}

// 비상 정지
{
  "type": "emergency_stop",
  "data": { "stop": true }
}
```

### 서버 → 클라이언트 (수신)

```json
// 오도메트리 (10 Hz)
{
  "type": "odom",
  "timestamp": 1700000000.0,
  "data": {
    "position": { "x": 1.2, "y": 0.5, "z": 0.0 },
    "orientation": { "theta": 0.78 },
    "velocity": { "linear_x": 0.1, "angular_z": 0.0 }
  }
}

// 지도 (5초마다)
{
  "type": "map",
  "timestamp": 1700000000.0,
  "data": {
    "info": { "width": 200, "height": 200, "resolution": 0.05,
              "origin": { "x": -5.0, "y": -5.0 } },
    "data": [0, 0, -1, 100, ...]
  }
}
```

---

## 트러블슈팅

### 클라이언트에서 연결 불가

```bash
# 포트 9090이 열려 있는지 확인
ss -tlnp | grep 9090

# 방화벽 허용 (Linux)
sudo ufw allow 9090/tcp
```

### 오도메트리가 너무 빠르거나 느림

`ros_bridge.cpp`의 `odomCallback` 내 다운샘플링 주기(`0.1`초 = 10Hz)를 수정한다.
