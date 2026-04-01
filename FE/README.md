# 🤖 누비고 — VEDA Qt Frontend

> **RC 로봇 통합 관제 시스템** — 실시간 CCTV 스트리밍, 자율주행 네비게이션, 원격 제어를 하나의 Qt 데스크톱 앱으로 통합한 프론트엔드입니다.

---

## 📋 목차

- [프로젝트 개요](#-프로젝트-개요)
- [주요 기능](#-주요-기능)
- [시스템 아키텍처](#-시스템-아키텍처)
- [디렉터리 구조](#-디렉터리-구조)
- [의존성](#-의존성)
- [빌드 방법](#-빌드-방법)
- [실행 환경 설정](#-실행-환경-설정)
- [네트워크 프로토콜](#-네트워크-프로토콜)

---

## 🎯 프로젝트 개요

**누비고 프로그램**은 RC 로봇 차량을 운용하기 위한 Qt6 기반 Windows 데스크톱 애플리케이션입니다.  
GStreamer를 통해 최대 4채널 RTSP 영상을 실시간으로 수신하고, ROS2 Bridge를 통해 로봇을 원격 제어합니다.

| 항목 | 내용 |
|------|------|
| **언어** | C++17 |
| **프레임워크** | Qt 6 (Widgets, WebSockets, Network) |
| **빌드 시스템** | CMake 3.16+ |
| **영상 처리** | GStreamer 1.0 (RTSP), OpenCV (FRUC, 선택) |
| **통신** | WebSocket (ROS2 Bridge, Camera Control) |
| **플랫폼** | Windows 10/11 (MSVC / MinGW) |

---

## ✨ 주요 기능

### 📹 실시간 영상 (Live View)
- **4채널 동시 RTSP 스트리밍** — GStreamer 파이프라인으로 저지연 재생
- **풀스크린 뷰** — 선택 채널 전체화면 확장, OSD 오버레이 지원
- **스트림 품질 모니터링** — FPS / 비트레이트 / 레이턴시 실시간 표시
- **RTSP 핑어** — 채널별 연결 상태 자동 감지

### 🚗 로봇 제어
| 모드 | 설명 |
|------|------|
| **Manual** | WASD 키보드로 직접 속도 제어 (cmd_vel 발행) |
| **Auto** | 맵 클릭으로 목표 지점 설정, Nav2 자율주행 |
| **Control** | 영상 위에 화살표 드래그로 이동 방향 지시 |
| **Patrol** | 다중 웨이포인트 순찰 경로 설정 및 실행 |

- **카메라 틸트** — 방향키로 틸트 각도 실시간 조절 (–30° ~ +30°)
- **긴급 정지** — 사이드바 버튼으로 즉각 정지 명령 발행

### 🗺️ SLAM 맵 뷰
- ROS2로부터 수신한 점유 격자 맵 실시간 렌더링
- 로봇 현재 위치(odometry) 및 계획된 경로 시각화
- 맵 클릭으로 Nav2 Goal 설정

### 📼 영상 녹화 및 다운로드
- 채널별 녹화 시작 / 정지
- 서버 저장 녹화 목록 조회 및 파일 다운로드
- FRUC(OpenCV DNN 보간)를 이용한 프레임레이트 업스케일 (선택 기능)

### 🔐 인증
- 로그인 다이얼로그 (TLS WebSocket, 서버 인증서 검증)
- 세션 관리 / 자동 재연결 / 로그아웃

---

## 🏛️ 시스템 아키텍처

```
┌─────────────────────────────────────────────────────┐
│                   MainWindow (Qt)                   │
│  ┌──────────┐  ┌──────────────────┐  ┌───────────┐  │
│  │  TopBar  │  │  CentralStack    │  │  Sidebar  │  │
│  └──────────┘  │  ┌────────────┐  │  └───────────┘  │
│                │  │  LiveView  │  │                  │
│                │  │ (4x VideoCard)│                  │
│                │  ├────────────┤  │                  │
│                │  │FullScreen  │  │                  │
│                │  │    View    │  │                  │
│                │  ├────────────┤  │                  │
│                │  │Playback    │  │                  │
│                │  │    View    │  │                  │
│                │  └────────────┘  │                  │
│                └──────────────────┘                  │
└─────────────────────────────────────────────────────┘
         │                        │
         ▼                        ▼
  RosBridgeClient        CameraControlClient
  (ws://robot:9090)      (ws://camera:9000)
         │                        │
         ▼                        ▼
    ROS2 Bridge              Camera Server
   (Nav2, SLAM, Odom)     (RTSP, 녹화, 캘리브레이션)
```

---

## 📁 디렉터리 구조

```
FE/
├── main.cpp                    # 앱 진입점, GStreamer DLL 초기화
├── mainwindow.{h,cpp}          # 메인 윈도우, 키 입력 처리
├── mainwindow_ui.cpp           # UI 레이아웃 구성
├── mainwindow_session.cpp      # 세션/인증 로직
│
├── Components/                 # 재사용 UI 컴포넌트
│   ├── topbar                  # 상단 타이틀바, 테마 전환
│   ├── sidebar                 # 좌측 사이드바 (모드/채널 선택)
│   ├── videocard               # 단일 CCTV 카드 (OSD, 설정)
│   ├── slammapwidget           # SLAM 맵 렌더링 위젯
│   ├── osdwidget               # OSD 오버레이 (FPS/비트레이트)
│   ├── settingswidget          # 전체 설정 페이지
│   └── logindialog             # 로그인 다이얼로그
│
├── Views/                      # 메인 페이지 뷰
│   ├── liveview                # 4채널 라이브 뷰
│   ├── fullscreenview          # 풀스크린 + 제어 오버레이
│   └── playbackview            # 녹화 영상 재생 뷰
│
├── Video/                      # 영상 처리 레이어
│   ├── videowidget             # GStreamer 파이프라인 관리
│   ├── livevideowidget         # 라이브 스트림 위젯
│   ├── recordedvideowidget     # 녹화 재생 위젯
│   ├── rtsppinger              # RTSP 연결 상태 감지
│   ├── frucvideoprocessor      # OpenCV FRUC 처리 (선택)
│   └── Gst/
│       ├── GstQualityMonitor   # GStreamer 품질 모니터
│       └── GstStatsCollector   # 스트림 통계 수집
│
├── Network/                    # 네트워크 클라이언트
│   ├── rosbridgeclient         # ROS2 Bridge WebSocket 클라이언트
│   ├── cameracontrolclient     # 카메라 서버 제어 클라이언트
│   ├── authmanager             # 인증/세션 관리
│   └── streammanager           # 스트림 URL 관리
│
├── Utils/                      # 유틸리티
│   ├── configmanager           # 앱 설정 (JSON 기반)
│   ├── constants               # 전역 상수 정의
│   ├── channelcatalog          # 채널 정보 카탈로그
│   ├── goaloverlaycontroller   # 목표 지점 오버레이 제어
│   ├── applicationinitializer  # 앱 초기화 시퀀스
│   ├── jsonutils               # JSON 파싱 헬퍼
│   └── framelessconfirmdialog  # 프레임리스 확인 다이얼로그
│
├── assets/
│   ├── fonts/                  # Pretendard 폰트
│   └── icons/                  # 앱 아이콘
├── style/                      # QSS 테마 파일 (Light / Dark)
├── env/                        # 환경설정 (서버 인증서 등)
└── CMakeLists.txt              # 빌드 설정
```

---

## 📦 의존성

### 필수

| 라이브러리 | 버전 | 용도 |
|-----------|------|------|
| [Qt](https://www.qt.io/) | 6.x | UI 프레임워크, WebSocket, Network |
| [GStreamer](https://gstreamer.freedesktop.org/) | 1.0 | RTSP 영상 수신 및 디코딩 |

### 선택 (FRUC 기능)

| 라이브러리 | 버전 | 용도 |
|-----------|------|------|
| [OpenCV](https://opencv.org/) | 4.x | 프레임 보간(FRUC) 처리 |

---

## 🔨 빌드 방법

### 1. 사전 요구사항 설치

- **Qt 6** — [Qt Online Installer](https://www.qt.io/download)에서 설치
  - 컴포넌트: `Qt Widgets`, `Qt WebSockets`, `Qt Network`
- **GStreamer 1.0** — [공식 사이트](https://gstreamer.freedesktop.org/download/)에서 Windows 바이너리 설치
  - MSVC 빌드: `C:/Program Files/gstreamer/1.0/msvc_x86_64`
  - MinGW 빌드: `C:/Program Files/gstreamer/1.0/mingw_x86_64`
- **CMake** 3.16 이상
- **컴파일러**: MSVC 2022 또는 MinGW

### 2. CMake 구성 및 빌드

```bash
# 소스 디렉터리로 이동
cd FE

# CMake 구성 (MSVC 예시)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 빌드
cmake --build build --config Release
```

### 3. OpenCV(FRUC) 포함 빌드 (선택)

```bash
cmake -B build \
  -DVEDA_ENABLE_FRUC=ON \
  -DOPENCV_ROOT="C:/opencv" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release
```

> FRUC 없이 빌드하려면 `-DVEDA_ENABLE_FRUC=OFF`

### 4. Qt Creator에서 빌드

1. Qt Creator에서 `FE/CMakeLists.txt` 열기
2. Kit 선택 (Qt 6 + MSVC 또는 MinGW)
3. **빌드** → **실행**

---

## ⚙️ 실행 환경 설정

앱 실행 전 아래 환경변수 또는 경로가 설정되어 있어야 합니다.

### GStreamer PATH 설정

```powershell
# PowerShell (MSVC)
$env:PATH += ";C:\Program Files\gstreamer\1.0\msvc_x86_64\bin"

# 또는 시스템 환경변수에 영구 등록
[Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\Program Files\gstreamer\1.0\msvc_x86_64\bin", "Machine")
```

### 서버 연결 설정

앱 실행 시 로그인 다이얼로그에서 아래 정보를 입력합니다.

| 항목 | 설명 | 예시 |
|------|------|------|
| 서버 IP | 로봇 서버(ROS2 Bridge) 주소 | `192.168.1.100` |
| 카메라 IP | 카메라 서버 주소 | `192.168.1.101` |
| 사용자 ID / PW | 인증 정보 | — |

---

## 🌐 네트워크 프로토콜

### ROS2 Bridge (포트 9090)

`RosBridgeClient`가 `ws://[robot_ip]:9090`에 연결하여 ROS2 토픽을 송수신합니다.

| 방향 | 토픽 / 기능 | 설명 |
|------|------------|------|
| 발행 | `/cmd_vel` | 선속도 / 각속도 (WASD 제어) |
| 발행 | `/camera_tilt` | 카메라 틸트 각도 |
| 발행 | `/nav/goto` | 단일 목표 지점 Nav2 |
| 발행 | `/nav/patrol` | 순찰 경로 |
| 발행 | `/nav/cancel` | 내비게이션 취소 |
| 구독 | `/map` | SLAM 점유 격자 맵 |
| 구독 | `/odom` | 로봇 위치/속도 |
| 구독 | `/plan` | 계획된 경로 |
| 구독 | `/nav_status` | 내비게이션 상태 |
| 구독 | `/fall_alert` | 낙상 감지 알림 |

### Camera Control Server (포트 9000)

`CameraControlClient`가 `ws://[camera_ip]:9000`에 연결합니다.

| 명령 | 설명 |
|------|------|
| `RECORD_START` / `RECORD_STOP` | 채널별 녹화 제어 |
| `REQUEST_RECORDINGS` | 녹화 목록 조회 |
| `REQUEST_DOWNLOAD` | 파일 다운로드 (바이너리) |
| `CALIBRATION_CLICK` | 캘리브레이션 좌표 전송 |
| `REQUEST_STREAM_STATS` | FPS / 비트레이트 / 레이턴시 요청 |

---

## 🎨 테마

- **Dark / Light** 테마 전환 지원 (상단바 버튼)
- QSS 스타일시트 기반 (`style/` 디렉터리)
- Pretendard 폰트 번들 포함

---

## 🏫 프로젝트 정보

본 프로젝트는 **VEDA** 교육 과정의 최종 프로젝트로 개발되었습니다.

---

*2026 VEDA Final Project*
