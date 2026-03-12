# camera_isp

ROS 2 패키지로, RTSP 카메라 스트림에 경량화 ISP(Image Signal Processor) 알고리즘을 실시간으로 적용하여 화질을 개선하고, 그 효과를 객관적인 지표로 측정합니다.

---

## 패키지 구성

```
camera_isp/
├── src/
│   └── isp_node.cpp          # 핵심 ISP C++ 노드
├── scripts/
│   └── isp_metrics_node.py   # 영상 품질 지표 측정 Python 노드
├── launch/
│   └── dual_rtsp.launch.py   # 전체 스택(원본/ISP/지표) 동시 실행 런치 파일
├── CMakeLists.txt
├── package.xml
└── README.md
```

---

## 노드 설명

### 1. `isp_node` (C++)

카메라 원본 압축 영상(`CompressedImage`)을 구독하여 ISP 처리를 수행한 후 결과를 발행합니다.

**적용 알고리즘:**
| 단계 | 기법 | 목적 |
|------|------|------|
| 1 | Bilateral Filter (`d=5, σColor=25, σSpace=25`) | 에지를 보존하며 노이즈 제거 |
| 2 | CLAHE (`clipLimit=2.0, grid=8×8`) on L-channel | 조도 및 명암비 개선 (색상 왜곡 없음) |
| 3 | Unsharp Mask (`α=1.5, β=-0.5`) | 윤곽선 선명화 (복원) |

**토픽:**
| 방향 | 토픽 | 타입 |
|------|------|------|
| Subscribe | `/camera/image_raw/compressed` | `sensor_msgs/CompressedImage` |
| Publish | `/camera/image_isp/compressed` | `sensor_msgs/CompressedImage` |

---

### 2. `isp_metrics_node.py` (Python)

원본 영상과 ISP 영상을 타임스탬프 기준으로 동기화(`TimeSynchronizer`)하여 수신한 뒤, 3가지 객관적 품질 지표를 실시간으로 터미널에 출력합니다.

**토픽:**
| 방향 | 토픽 |
|------|------|
| Subscribe | `/camera/image_raw/compressed` |
| Subscribe | `/camera/image_isp/compressed` |

**출력 형식:**
```
---- ISP Metrics (vs Raw) ----
[Contrast]  Raw:  62.87 | ISP:  63.14 | Diff:   +0.4%
[Sharpness] Raw: 124.38 | ISP: 158.87 | Diff:  +27.7%
[SNR (dB)]  Raw:  12.05 | ISP:  13.20 | Diff:   +9.5%
```

---

## 지표 의미

| 지표 | 계산 방식 | 의미 |
|------|-----------|------|
| **Contrast** | 픽셀 밝기의 표준편차 (RMS) | 수치 증가 → 명암비 개선, 밝고 어두운 부분의 구분 명확 |
| **Sharpness** | Laplacian 분산 | 수치 증가 → 에지/윤곽선/디테일 선명화 |
| **SNR (dB)** | `20 × log10(Mean / StdDev)` | 수치 증가 → 노이즈 대비 신호량 증가, 화질의 청정도 향상 |

---

## 빌드 방법

Docker 컨테이너 내부에서 실행합니다.

```bash
docker exec -it wsl_ros2 bash
cd /root/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select camera_isp
```

---

## 실행 방법

```bash
# Docker 컨테이너 내부에서
source /opt/ros/humble/setup.bash
source /root/ros2_ws/install/setup.bash

# 전체 스택 실행 (원본 RTSP 스트림 + ISP 스트림 + 지표 측정 노드)
ros2 launch camera_isp dual_rtsp.launch.py
```

**노드별 단독 실행:**
```bash
# ISP 처리 노드만 실행
ros2 run camera_isp isp_node

# 지표 측정 노드만 실행
ros2 run camera_isp isp_metrics_node.py
```

---

## 의존성

| 패키지 | 용도 |
|--------|------|
| `rclcpp` / `rclpy` | ROS 2 C++ / Python 클라이언트 라이브러리 |
| `sensor_msgs` | `CompressedImage` 메시지 타입 |
| `OpenCV` | 영상처리 (Bilateral, CLAHE, Unsharp Mask) |
| `message_filters` | 멀티 토픽 타임스탬프 동기화 |
| `rtsp_bridge` | RTSP 스트림 발행 (외부 패키지, `dual_rtsp.launch.py` 의존) |
