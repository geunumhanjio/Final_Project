# rtsp_bridge

ROS2 `CompressedImage` 토픽을 수신하여 RTSP 스트림으로 내보내는 패키지.
외부 클라이언트(Qt 앱, VLC, 웹 브라우저 등)가 RTSP URL로 실시간 영상을 시청할 수 있게 한다.

---

## 왜 이 패키지가 필요한가

`camera_bridge`를 통해 ROS2 토픽으로 변환된 카메라 영상을 외부 클라이언트에게 제공하려면
ROS2 밖의 표준 스트리밍 프로토콜이 필요하다.

**RTSP(Real-Time Streaming Protocol)** 는 VLC, FFmpeg, 웹 브라우저(HLS 경유), 그리고 Qt의 멀티미디어 모듈에서 모두 지원하는 범용 프로토콜이다.

`rtsp_bridge`는 다음 역할을 한다.
- ROS2 `/camera/image_raw/compressed` 토픽 구독
- GStreamer 파이프라인으로 H.264 인코딩
- **mediamtx** RTSP 미디어 서버에 스트림 발행
- 클라이언트는 `rtsp://<서버IP>:8554/camera` 로 접속하여 시청

---

## 패키지 구조

```
rtsp_bridge/
├── rtsp_bridge/
│   ├── rtsp_publisher.py  ← CompressedImage → GStreamer → RTSP 변환 노드
│   └── __init__.py
└── launch/
    └── rtsp_publisher.launch.py
```

---

## 의존성: mediamtx

`rtsp_bridge`는 단독으로 RTSP 서버 역할을 하지 않는다.
GStreamer의 `rtspclientsink`를 통해 **mediamtx**에 스트림을 push한다.
mediamtx가 먼저 실행되어 있어야 한다.

```bash
# Windows 호스트에서 mediamtx 실행 (WSL/mediamtx.yml 사용)
./mediamtx WSL/mediamtx.yml
```

mediamtx 설정 파일(`WSL/mediamtx.yml`)에서 RTSP 포트는 **9554**로 설정되어 있다.

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select rtsp_bridge
source install/setup.bash
```

---

## 사용법

```bash
# 단독 실행
ros2 launch rtsp_bridge rtsp_publisher.launch.py

# wsl_bringup을 통한 실행 (권장)
ros2 launch wsl_bringup wsl_bringup.launch.py use_rtsp:=true
```

---

## 토픽

### 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/camera/image_raw/compressed` | `sensor_msgs/CompressedImage` | 입력 JPEG 이미지 |

---

## 파라미터

| 파라미터 | 기본값 | 설명 |
|----------|--------|------|
| `input_topic` | `/camera/image_raw/compressed` | 구독할 이미지 토픽 |
| `rtsp_host` | `host.docker.internal` | mediamtx 호스트 주소 |
| `rtsp_port` | `8554` | mediamtx RTSP 포트 |
| `stream_name` | `camera` | 스트림 경로 이름 |
| `width` | `640` | 출력 해상도 너비 |
| `height` | `480` | 출력 해상도 높이 |
| `fps` | `30` | 출력 프레임레이트 |
| `bitrate` | `2000` | H.264 인코딩 비트레이트 (kbps) |

---

## 스트림 시청

```bash
# VLC
vlc rtsp://192.168.0.237:9554/camera

# FFmpeg
ffplay rtsp://192.168.0.237:9554/camera

# HLS (웹 브라우저) - mediamtx HLS 포트 8888
http://192.168.0.237:8888/camera
```

---

## 동작 원리

1. ROS2 `CompressedImage`(JPEG) 구독
2. `numpy`로 JPEG → OpenCV BGR 이미지 디코딩
3. 목표 해상도로 리사이즈 (필요한 경우)
4. GStreamer 파이프라인: `appsrc → videoconvert → x264enc (zerolatency) → rtspclientsink`
5. mediamtx가 클라이언트에게 RTSP / HLS / WebRTC로 재배포

---

## 트러블슈팅

### VideoWriter가 열리지 않음

```bash
# GStreamer 플러그인 확인
gst-inspect-1.0 x264enc
gst-inspect-1.0 rtspclientsink

# mediamtx가 실행 중인지 확인
curl http://localhost:9997/v3/paths/list
```

### 지연이 심함

`x264enc speed-preset=ultrafast tune=zerolatency`가 적용되어 있다.
비트레이트를 낮추거나 해상도를 줄이면 지연을 추가로 줄일 수 있다.
