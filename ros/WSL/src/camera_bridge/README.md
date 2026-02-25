# camera_bridge

라즈베리파이의 카메라 HTTP MJPEG 스트림을 수신하여 ROS2 CompressedImage 토픽으로 변환하는 패키지.

---

## 왜 이 패키지가 필요한가

라즈베리파이의 `camera_ros` 패키지는 IMX219 카메라 영상을 ROS2 토픽으로 퍼블리시한다.
그러나 이 토픽을 WSL까지 DDS로 전달하면 **비압축 원본 이미지 데이터가 LAN을 과도하게 점유**한다
(640×480 RGB → 약 900 KB/frame × 30fps ≈ 27 MB/s).

대신 라즈베리파이에서 MJPEG HTTP 서버(libcamera-vid 또는 별도 MJPEG 서버)로 영상을 제공하면,
WSL의 `camera_bridge`가 이를 구독하여 ROS2 `CompressedImage` 토픽으로 변환한다.
JPEG 압축을 거치면 동일 해상도/프레임레이트 기준 **5–10배 이상 대역폭을 절감**할 수 있다.

---

## 패키지 구조

```
camera_bridge/
├── camera_bridge/
│   ├── mjpeg_bridge.py     ← MJPEG HTTP 스트림 → ROS2 CompressedImage 변환 노드
│   └── __init__.py
├── config/
│   ├── camera_params.yaml  ← 스트림 URL 파라미터
│   └── rtsp_params.yaml
└── launch/
    ├── camera_bridge.launch.py        ← MJPEG 브릿지 단독
    └── camera_with_rtsp.launch.py     ← MJPEG 브릿지 + RTSP 브릿지 함께
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select camera_bridge
source install/setup.bash
```

---

## 사용법

```bash
# 기본 실행 (스트림 URL은 파라미터로 지정)
ros2 launch camera_bridge camera_bridge.launch.py

# 스트림 URL 런타임 오버라이드
ros2 run camera_bridge mjpeg_bridge \
  --ros-args -p stream_url:=http://192.168.0.33:8000/video
```

---

## 토픽

### 퍼블리시

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/camera/image_raw/compressed` | `sensor_msgs/CompressedImage` | JPEG 압축 카메라 이미지 |

---

## 파라미터

| 파라미터 | 기본값 | 설명 |
|----------|--------|------|
| `stream_url` | `http://192.168.0.33:8000/video` | RPi MJPEG HTTP 스트림 URL |

---

## 동작 원리

1. 노드 시작 시 `stream_url`에 HTTP 연결을 맺는다.
2. 100Hz 타이머로 스트림 데이터를 읽으며 JPEG 프레임을 탐색한다.
   JPEG는 `0xFF 0xD8` (SOI)로 시작하고 `0xFF 0xD9` (EOI)로 끝난다.
3. 완전한 JPEG 프레임을 찾으면 `CompressedImage` 메시지로 래핑하여 퍼블리시한다.
   OpenCV 디코딩 없이 raw JPEG 바이트를 그대로 전송하므로 CPU 부하가 낮다.

---

## 트러블슈팅

### 스트림에 연결되지 않음

```bash
# RPi에서 HTTP 서버가 실행 중인지 확인
curl http://192.168.0.33:8000/video --output /tmp/test.jpg

# ping으로 네트워크 확인
ping 192.168.0.33
```

### 이미지가 끊김

- 네트워크 대역폭 부족: JPEG 품질 또는 해상도를 낮춘다.
- `stream_url` 타임아웃: 노드를 재시작한다.
