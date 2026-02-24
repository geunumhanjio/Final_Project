# VEDA_QT_1 - CCTV 통합 관제 및 지능형 로봇 제어 시스템

**VEDA_QT_1**은 **Qt 6 (C++)**와 **GStreamer** 미디어 프레임워크를 기반으로 개발된 **고성능 CCTV 관제 및 로봇 제어 통합 시스템**입니다.  
다수의 RTSP 카메라 스트림을 실시간으로 모니터링하며, 카메라 서버 WebSocket에서 수신한 성능 지표를 OSD로 표시하고, **ROS2** 기반 이동 로봇을 원격으로 수동·자율 제어할 수 있는 올인원(All-in-One) 솔루션입니다.

---

## ✨ 핵심 기능

### 1. **다채널 실시간 영상 관제 (Live View)**
- **4분할 라이브 그리드**: 최대 4개의 CCTV 영상을 동시 스트리밍합니다.
- **RC카 카메라 + SLAM 라이다 맵**: 우측 패널에 5번째 채널(로봇 전방캠) 및 SLAM 지도를 함께 표시합니다.
- **스마트 줌 & 팬**: 채널 더블클릭으로 전체화면 진입 후, 마우스 휠(Zoom)·드래그(Pan)·영역 줌(Rectangle Zoom)으로 정밀 감시합니다.
- **RTSP 자동 재연결**: 서버 다운이나 네트워크 단절 시 내부 워치독(Watchdog)이 감지하여 3초마다 재시도, 서버 복구 시 화면을 자동 복원합니다.

### 2. **실시간 OSD 성능 지표 오버레이**
- **5가지 지표 On/Off**: 영상 우측 상단에 반투명 OSD로 **Packet Loss, Jitter, Bitrate, FPS, Latency**를 표시하며, 톱니바퀴 버튼 팝업 메뉴에서 항목별로 On/Off 가능합니다.
- **이중 데이터 소스 연동**:
  - **Packet Loss & Jitter**: GStreamer 파이프라인 내부의 `rtpjitterbuffer` 요소를 재귀탐색하여 실제 수치를 수학적으로 계산합니다.
  - **Bitrate / FPS / Latency**: 카메라 제어 서버(`ws://[IP]:9000`)와 상시 연결된 WebSocket에서 `STREAM_STATS` JSON 메시지를 파싱하여 OSD에 실시간 반영합니다. Bitrate는 `Mbps` 단위로 환산하고, Latency는 소수점 3자리로 표시합니다.
- **OSD 위치 동기화**: 60Hz QTimer 및 `ApplicationStateChanged` 이벤트 훅으로 메인 윈도우와 OSD 오버레이의 위치·가시성을 항상 일치시킵니다.

### 3. **영상 녹화 및 다운로드 (Playback View)**
- UI 상단의 **Record 버튼**으로 채널별 녹화를 시작/중지하며, 완료 후 서버에서 파일 목록을 가져옵니다.
- 영상 마우스 오른쪽 클릭 등 메뉴를 통해 로컬에 다운로드가 가능하며, 로컬 저장 파일은 즉시 재생(Playback View)할 수 있습니다.

### 4. **ROS2 지능형 로봇 원격 제어**
- **수동 주행 (WASD)**: WASD 키보드 입력을 10Hz로 `cmd_vel` JSON 메시지로 변환하여 ROS2 Bridge에 전송합니다. 입력 중단 시 자동으로 정지 명령을 보냅니다.
- **목표 지점 설정 (Goal Pose)**: 전체화면에서 Control Mode 활성화 후 마우스 드래그로 방향 화살표를 그리면 `goal_pose`를 전송합니다.
- **비상 정지 & 모드 전환**: 사이드바에서 `emergency_stop` 전송 및 수동/자율/인물추종/순찰 모드(`mode_control`) 전환이 가능합니다.

### 5. **유연한 시스템 설정**
- **동적 설정 (`settings.ini`)**: 카메라 IP/포트, ROS2 Bridge WebSocket 주소(`ws://…`), 수동 제어 허용 여부 등을 앱 내 설정 화면에서 수정하면 **재시작 없이 즉시 네트워크 연결을 갱신**합니다.
- **다크/라이트 테마**: 상단바 버튼 클릭으로 `theme_dark.qss` ↔ `theme_light.qss`를 실시간 전환합니다.

---

## 🛠 시스템 요구 사항

| 항목 | 요구 사항 |
|---|---|
| OS | Windows 10 / 11 (64-bit) |
| 컴파일러 | MSVC 2019 이상 (64-bit) |
| Qt 버전 | Qt 6.x (Widgets, WebSockets 모듈 필수) |
| CMake | 3.16 이상 |
| GStreamer | 1.16 이상, MSVC 64-bit (Runtime + Development 모두 설치) |

> **GStreamer 다운로드**: [https://gstreamer.freedesktop.org/download/](https://gstreamer.freedesktop.org/download/)  
> MSVC 64-bit 빌드・Runtime + Development 패키지 두 종류를 모두 설치해야 합니다.

---

## 🚀 빌드 및 실행

### 1. 프로젝트 클론
```bash
git clone https://github.com/geunumhanjio/Final_Project.git
cd Final_Project/FE
```

### 2. Qt Creator 설정
1. Qt Creator에서 `CMakeLists.txt` 파일을 엽니다.
2. 시스템에 맞는 키트(`Desktop Qt 6.x.x MSVC2022 64bit`)를 선택합니다.
3. **Run CMake**를 실행하여 GStreamer와 Qt 라이브러리를 연결합니다.

### 3. 빌드 & 실행
- 좌측 하단 **Build(망치)** → **Run(▶)** 버튼 순서로 클릭합니다.

### 4. 초기 설정
- 실행 후 상단 **톱니바퀴 아이콘(System Settings)**을 눌러 카메라 서버 IP와 ROS2 Bridge 주소를 환경에 맞게 입력하고 **Save Settings**를 클릭합니다.

---

## 📂 프로젝트 구조

```text
FE/
├── main.cpp                    # 앱 진입점, GStreamer init
├── mainwindow.cpp/.h           # 메인 윈도우 컨트롤러 (WASD 이벤트 필터, 글로벌 시그널 배선)
├── CMakeLists.txt              # 빌드 설정, GStreamer/Qt 라이브러리 링킹
│
├── style/
│   ├── theme_dark.qss          # 다크 테마 스타일 시트
│   ├── theme_light.qss         # 라이트 테마 스타일 시트
│   └── app_styles.qss          # 공통 베이스 스타일
│
├── Views/                      # 주요 화면(페이지) 단위 뷰
│   ├── liveview.cpp/.h         # 4분할 CCTV + RC카/SLAM 라이브 스트리밍 그리드
│   ├── playbackview.cpp/.h     # 녹화 영상 목록 조회 및 다운로드/재생
│   └── fullscreenview.cpp/.h   # 전체화면: 줌·팬·ControlOverlay(로봇 Goal Pose)
│
├── Components/                 # 재사용 가능한 UI 컴포넌트
│   ├── videocard.cpp/.h        # 개별 채널 비디오 카드 (OSD 팝업 메뉴 포함)
│   ├── osdwidget.cpp/.h        # 반투명 성능 지표 오버레이 렌더러
│   ├── settingswidget.h        # 설정 다이얼로그 (Header-only, QSettins 연동)
│   ├── sidebar.cpp/.h          # 좌측 컨텍스트 내비게이션 (Live/Playback 모드)
│   ├── topbar.cpp/.h           # 상단 바 (테마 토글, 시스템 설정 진입)
│   └── full_underbar.cpp/.h    # 전체화면 하단 제어 바 (줌·팬·REC 버튼)
│
├── Video/                      # GStreamer 기반 미디어 엔진
│   ├── videowidget.cpp/.h      # GStreamer 파이프라인 베이스 클래스 (OSD 동기화, 크롭)
│   ├── livevideowidget.cpp/.h  # RTSP 라이브 파이프라인 (자동 재연결 로직 포함)
│   └── recordedvideowidget.cpp/.h  # 로컬 파일(.mp4 등) 재생 로직
│
├── Network/                    # 비동기 WebSocket 통신 레이어
│   ├── cameracontrolclient.cpp/.h  # 카메라 서버 통신: 녹화 제어, STREAM_STATS 파싱, 파일 다운로드
│   ├── rosbridgeclient.cpp/.h      # ROS2 Bridge 통신: cmd_vel, goal_pose, mode_control 전송
│   └── streammanager.cpp/.h        # 설정 기반 RTSP URL 동적 생성기
│
└── Utils/
    └── configmanager.h         # QSettings 기반 settings.ini 관리자 (Header-only Singleton)
```

---

## 🔌 서버 WebSocket 프로토콜

### 카메라 제어 서버 (`ws://[IP]:9000`)

**클라이언트 → 서버 (송신)**

| 타입 | 설명 |
|---|---|
| `RECORD_CONTROL` | 채널 녹화 시작/중지 |
| `GET_RECORDINGS` | 녹화 목록 요청 |
| `DOWNLOAD_FILE` | 파일 다운로드 요청 |

**서버 → 클라이언트 (수신)**

| 타입 | 설명 |
|---|---|
| `STREAM_STATS` | FPS, Bitrate(kbps), Proxy Latency(ms) 실시간 지표 |
| `RECORDING_LIST` | 녹화 영상 목록 |
| `RECORD_FINISHED` | 녹화 완료 알림 |
| `FILE_TRANSFER_START` / Binary | 파일 전송 시작 및 바이너리 청크 |
| `FILE_TRANSFER_COMPLETE` | 파일 전송 완료 |

**STREAM_STATS 예시:**
```json
{
  "type": "STREAM_STATS",
  "payload": {
    "channel_id": 1,
    "fps": 30.01,
    "bitrate_kbps": 2450.75,
    "proxy_latency_ms": 0.005
  }
}
```

### ROS2 Bridge (`ws://[IP]:9090`)
- `cmd_vel` — 선속도/각속도 제어 (10Hz)
- `goal_pose` — 자율주행 목표 위치 및 방향
- `mode_control` — 주행 모드 전환 (수동/자율/추종/순찰)
- `emergency_stop` — 비상 정지

---

## 📝 라이선스 및 협업

본 프로젝트는 **VEDA AIoT 프로젝트 (근엄한조)**의 일환으로 개발되었습니다.  
GitHub: [https://github.com/geunumhanjio](https://github.com/geunumhanjio)
