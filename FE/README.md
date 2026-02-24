# VEDA_QT_1 - CCTV 통합 관제 및 지능형 로봇 제어 시스템

**Qt 6 (C++)** + **GStreamer** 기반의 고성능 CCTV 관제 및 ROS2 로봇 제어 통합 시스템입니다.
다수의 RTSP 카메라 스트림을 실시간으로 모니터링하고, 영상 성능 지표를 WebSocket으로 수신하며, ROS2 로봇을 원격으로 수동·자율 제어합니다.

---

## ✨ 핵심 기능

### 1. 다채널 실시간 영상 관제 (Live View)
- **4분할 CCTV 그리드 + RC카 카메라**: 총 5채널을 동시에 스트리밍
- **스마트 줌 & 팬**: 더블클릭 → 전체화면, 마우스 휠(Zoom) / 드래그(Pan) / 사각형 선택(Rect Zoom)
- **RTSP Auto-Reconnect**: 서버 다운 또는 네트워크 단절 시 Watchdog 타이머가 감지 후 **3초 간격으로 무한 자동 재연결** — 서버 복구 시 화면 자동 복원
- **녹화 제어 & 다운로드**: 채널별 원격 녹화 시작/정지, 완료 후 로컬 다운로드 및 재생

### 2. 실시간 OSD 성능 지표 오버레이
- **5가지 지표**: Packet Loss · Jitter · FPS · Bitrate · Latency 를 영상 위에 반투명 오버레이로 표시
- **⚙ 팝업 메뉴 선택**: 각 영상 카드의 톱니바퀴 버튼 클릭 → 체크박스로 원하는 지표 개별 On/Off (`All` 일괄 전환 포함)
- **즉시 요청 방식**: 지표를 하나라도 체크하는 순간 서버에 **`REQUEST_STREAM_STATS` JSON 요청**을 자동으로 보내고, 이후 서버로부터 실시간 데이터를 수신해 즉시 표시

```json
// 요청 JSON 예시 (체크 시 자동 전송)
{ "type": "REQUEST_STREAM_STATS", "payload": { "channel_id": 1, "action": "start" } }

// 수신 JSON 예시 (STREAM_STATS)
{
  "type": "STREAM_STATS",
  "payload": { "channel_id": 1, "fps": 30.01, "bitrate_kbps": 2450.75, "proxy_latency_ms": 0.005 }
}
```

- **듀얼 소스 하이브리드**:
  - `Packet Loss & Jitter` → GStreamer `rtpjitterbuffer` 내부 통계 실시간 연산
  - `FPS · Bitrate · Latency` → WebSocket `STREAM_STATS` 서버 수신값 파싱 (Mbps 환산, `0.00x` ms 포맷)
- **오버레이 위치 동기화**: 창 이동/포커스 변경 시 OSD가 영상 위에 정확히 따라붙음

### 3. ROS2 로봇 원격 제어 (WebSocket & JSON)
- **수동 주행 (WASD)**: 키보드 WASD → 10Hz 주기 `cmd_vel` 발행 (키를 떼면 자동 정지)
- **자율 주행 (Goal Pose)**: 전체화면에서 마우스 드래그로 목적지·방향(빨간 화살표) 지정 → `goal_pose` 발행
- **모드 전환**: 수동 / 자율 / 인물추종 / 순찰 모드 (`mode_control`)
- **비상 정지**: 클릭 한 번으로 `emergency_stop` 발행
- **동적 IP 설정**: 설정 창에서 ROS2 브릿지 주소 변경 시 앱 재시작 없이 즉시 재연결 (`onConfigChanged`)

### 4. 시스템 설정 & 테마
- **`settings.ini` 기반** (싱글톤 `ConfigManager`): 카메라 IP·포트, ROS2 WebSocket 주소, 수동 제어 허용 여부 등 UI에서 편집 후 영구 저장
- **다크 / 라이트 테마**: 상단 바에서 실시간 토글, `style/theme_dark.qss` / `style/theme_light.qss`로 분리

---

## 🛠 시스템 요구 사항

| 항목 | 요구 사항 |
|------|-----------|
| OS | Windows 10 / 11 (64-bit) |
| Compiler | MSVC 2019+ 또는 MinGW 64-bit |
| Qt | Qt 6.x — Widgets, WebSockets 모듈 필수 |
| CMake | 3.16 이상 |
| GStreamer | 1.16 이상 (MSVC 64-bit) — Runtime + Development 패키지 모두 설치 |

> **GStreamer 다운로드**: https://gstreamer.freedesktop.org/download/
> MSVC 64-bit 버전 (`gstreamer-1.0-msvc-x86_64-*.msi`) Runtime + Development 둘 다 설치하세요.

---

## 🚀 빌드 및 실행

```bash
# 1. 클론
git clone https://github.com/geunumhanjio/Final_Project.git
cd Final_Project/FE
```

1. **Qt Creator**에서 `CMakeLists.txt` 열기
2. 키트 선택: `Desktop Qt 6.x.x MSVC 64bit`
3. **Run CMake** → 빌드 후 ▶ 실행

---

## 📂 프로젝트 구조

```text
FE/
├── main.cpp                     # 앱 진입점, GStreamer 전역 초기화
├── mainwindow.cpp / .h          # 메인 윈도우 컨트롤러
│                                  • 전역 키 이벤트 필터 (WASD 로봇 제어)
│                                  • LiveView / FullScreenView / Settings 라우팅
│                                  • STREAM_STATS 신호 → UI 라우팅 연결
│                                  • ROS2 IP 동적 재연결 (onConfigChanged)
│                                  • OSD 체크 시 requestStreamStats() 자동 호출
├── CMakeLists.txt               # 빌드 구성, GStreamer / Qt 링킹
├── style/
│   ├── theme_dark.qss           # 다크 테마
│   └── theme_light.qss          # 라이트 테마
│
├── Views/
│   ├── liveview.cpp / .h        # 4채널 CCTV + RC카 분할 그리드
│   │                              • 채널별 streamStatsRequested 신호 상위 전달
│   │                              • STREAM_STATS → 각 VideoCard 라우팅
│   ├── playbackview.cpp / .h    # 녹화 목록 · 다운로드 · 로컬 재생
│   └── fullscreenview.cpp / .h  # 전체화면 뷰
│                                  • Smart Zoom / Pan / Rect Zoom
│                                  • Goal Pose ControlOverlay (마우스 드래그)
│                                  • OSD 체크 시 streamStatsRequested 신호 발행
│                                  • STREAM_STATS OSD 갱신
│
├── Components/
│   ├── videocard.cpp / .h       # 개별 영상 카드 래퍼
│   │                              • 호버 시 상단 오버레이 표시
│   │                              • ⚙ 팝업 메뉴 → OSD 지표 체크박스 (All 포함)
│   │                              • 체크 시 streamStatsRequested(channelId, bool) 발행
│   │                              • updateStreamStats() → OsdWidget 값 갱신
│   ├── osdwidget.cpp / .h       # 반투명 OSD 렌더러
│   │                              • setMetricValue() / setMetricVisible()
│   │                              • Packet Loss · Jitter · FPS · Bitrate · Latency
│   ├── full_underbar.cpp / .h   # 전체화면 하단 바 (재생 제어 / 줌 버튼)
│   ├── sidebar.cpp / .h         # 좌측 채널·카테고리 내비게이션
│   ├── topbar.cpp / .h          # 상단 바 (로고, 시간, 테마 토글, 설정 버튼)
│   └── settingswidget.h         # 시스템 설정 다이얼로그 (Header-only UI)
│                                  • 카메라 IP / RTSP 포트 / ROS2 WS 주소 편집
│
├── Video/
│   ├── videowidget.cpp / .h     # GStreamer 파이프라인 베이스 클래스
│   │                              • GstBus 폴링, Crop, OSD 위치 동기화
│   │                              • Packet Loss / Jitter 실시간 연산
│   ├── livevideowidget.cpp / .h # RTSP 라이브 스트리밍 파이프라인
│   │                              • rtspsrc → rtph264depay → avdec_h264 → videosink
│   │                              • Watchdog + 3초 간격 Auto-Reconnect
│   └── recordedvideowidget.cpp / .h # 로컬 파일(.mp4 등) 재생 파이프라인
│
├── Network/
│   ├── cameracontrolclient.cpp / .h  # 카메라 서버 WebSocket 클라이언트 (포트 9000)
│   │                              • connectToServer(): 상시 연결 유지 + 자동 재연결
│   │                              • requestStreamStats(): OSD 체크 시 즉시 요청
│   │                              • STREAM_STATS JSON 파싱 → streamStatsReceived 신호
│   │                              • 녹화 제어 (RECORD_CONTROL)
│   │                              • 녹화 목록 조회 (GET_RECORDINGS)
│   │                              • 파일 다운로드 (DOWNLOAD_FILE, 바이너리 전송)
│   ├── rosbridgeclient.cpp / .h  # ROS2 rosbridge WebSocket 클라이언트
│   │                              • sendCmdVel() / sendGoalPose()
│   │                              • sendModeControl() / sendEmergencyStop()
│   └── streammanager.cpp / .h   # 카메라별 RTSP URL 동적 생성기
│
└── Utils/
    └── configmanager.h          # QSettings 기반 .ini 설정 관리자
                                   (Header-only Singleton)
                                   • getCameraIp/Port / getRobotIp / setRobotIp
                                   • configChanged 신호 → 실시간 설정 반영
```

---

## 📝 라이선스

본 프로젝트는 **VEDA AIoT 프로젝트 (근엄한조)** 의 일환으로 개발되었습니다.
