# fall_detection

MediaPipe Pose 기반 누워있는 상태(낙상) 감지 노드.
카메라 프레임에서 어깨→힙 벡터의 기울기를 분석해 사람이 누워있는지 판단하고,
감지 시 `/fall_detection/alert` 토픽으로 경고를 발행한다.
WebSocket 브릿지를 통해 클라이언트(앱/웹)에 JSON 경고 메시지가 전달된다.

---

## 패키지 구조

```
fall_detection/
├── fall_detection/
│   └── fall_detection_node.py  ← 낙상 감지 노드
└── launch/
    └── fall_detection.launch.py
```

---

## 감지 원리

MediaPipe Pose Lite 모델로 추출한 키포인트에서 **어깨 중점 → 힙 중점** 벡터를 계산한다.

```
서있을 때:  어깨-힙 벡터가 수직 방향 → 각도 ≈ 0°
누워있을 때: 어깨-힙 벡터가 수평 방향 → 각도 > 45°
```

false positive 억제를 위해 **연속 3프레임** 판정 후 경고를 발행하며,
**20초 쿨다운**으로 중복 경고를 억제한다.

---

## 토픽

### 구독

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/camera/image_raw/compressed` | `sensor_msgs/CompressedImage` | 카메라 프레임 (설정 가능) |
| `/fall_detection/enable` | `std_msgs/Bool` | 감지 ON/OFF |

### 발행

| 토픽 | 타입 | 주기 | 설명 |
|------|------|------|------|
| `/fall_detection/alert` | `std_msgs/String` (JSON) | 이벤트 | 낙상 감지 경고 |
| `/fall_detection/status` | `std_msgs/String` (JSON) | 1 Hz | 현재 상태 |

---

## JSON 메시지 형식

### `/fall_detection/alert`
```json
{
  "detected":  true,
  "angle_deg": 72.3,
  "timestamp": 1743000000.123
}
```

### `/fall_detection/status`
```json
{
  "enabled":            true,
  "model_ok":           true,
  "cooldown_remaining": 14.5
}
```

### WebSocket (`type: "fall_alert"`)
클라이언트가 WebSocket으로 받는 메시지:
```json
{
  "type":      "fall_alert",
  "timestamp": 1743000000.0,
  "data": {
    "detected":  true,
    "angle_deg": 72.3,
    "timestamp": 1743000000.123
  }
}
```

---

## Launch Arguments

| Argument | Default | 설명 |
|----------|---------|------|
| `max_fps` | `5.0` | 최대 추론 FPS (낮을수록 CPU 절약) |
| `image_topic` | `/camera/image_raw/compressed` | 구독할 카메라 토픽 |
| `lying_angle_threshold` | `45.0` | 누워있음 판정 각도 임계값 (deg) |
| `cooldown_sec` | `20.0` | 경고 재발행 최소 간격 (초) |
| `confirm_count` | `3` | 연속 판정 횟수 |
| `enabled_on_start` | `true` | 시작 시 감지 활성화 여부 |

---

## 실행

### 단독 실행
```bash
ros2 launch fall_detection fall_detection.launch.py

# 시작 시 비활성화
ros2 launch fall_detection fall_detection.launch.py enabled_on_start:=false

# 파라미터 조정
ros2 launch fall_detection fall_detection.launch.py cooldown_sec:=30.0 max_fps:=3.0
```

### wsl_bringup 통합 실행
```bash
ros2 launch wsl_bringup wsl_bringup.launch.py use_fall_detection:=true

# 사람 추종과 동시에
ros2 launch wsl_bringup wsl_bringup.launch.py use_fall_detection:=true use_tracker:=true
```

### 런타임 ON/OFF
```bash
# 비활성화 (성능 확보)
ros2 topic pub --once /fall_detection/enable std_msgs/Bool "{data: false}"

# 활성화
ros2 topic pub --once /fall_detection/enable std_msgs/Bool "{data: true}"
```

### WebSocket 클라이언트에서 ON/OFF
```json
{ "type": "fall_detection_enable", "data": { "enable": true } }
{ "type": "fall_detection_enable", "data": { "enable": false } }
```

---

## 빌드

```bash
colcon build --packages-select fall_detection
source install/setup.bash
```

---

## 의존성

- `rclpy`, `std_msgs`, `sensor_msgs`
- `mediapipe` — `pip install mediapipe`
- `opencv-python-headless` — `pip install opencv-python-headless`
