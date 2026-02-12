# VEDA_QT_1 - CCTV 통합 관제 및 로봇 제어 시스템

**VEDA_QT_1**은 **Qt 6 (C++)**와 **GStreamer**를 활용하여 개발된 **고성능 CCTV 관제 시스템**입니다. 다수의 RTSP 카메라 스트림을 실시간으로 모니터링하고, **ROS2** 기반의 이동 로봇을 원격으로 제어할 수 있는 통합 솔루션을 제공합니다.

## ✨ 주요 기능

### 1. **다채널 영상 관제 (Live View)**
- **4분할 라이브 그리드**: 4개의 고정형 CCTV 카메라 영상을 동시에 모니터링합니다.
- **스마트 줌 & 팬**: 특정 채널을 더블 클릭하여 전체 화면으로 전환하며, 마우스 드래그(Pan)와 휠(Zoom)을 통해 세밀한 관찰이 가능합니다.
- **GStreamer 가속**: 하드웨어 가속을 지원하는 GStreamer 파이프라인을 통해 지연 시간(Low Latency)을 최소화했습니다.

### 2. **영상 다시보기 및 관리 (Playback View)**
- **녹화 영상 목록**: 카테고리별로 정렬된 녹화 파일을 탐색하고 재생합니다.
- **상황별 사이드바**: 현재 화면(라이브 채널 vs 다시보기 카테고리)에 따라 사이드바 내용이 동적으로 변경됩니다.
- **테마 최적화**:
  - **다크 모드**: 저조도 환경에 최적화된 테마.
  - **화이트 모드**: 밝은 환경에서 가독성을 극대화하기 위해 텍스트를 진한 검정색(Black)으로 처리.

### 3. **ROS2 로봇 원격 제어**
- **WebSocket 통신**: `rosbridge_server`를 통해 로봇과 실시간으로 데이터를 주고받습니다.
- **수동 주행 제어**: 키보드 방향키를 사용하여 로봇의 이동(`cmd_vel`)을 직접 제어할 수 있습니다.
- **자율 주행 (Goal Pose)**: 맵 상의 목적지를 지정하여 로봇을 자율 주행시킬 수 있습니다 (구현 중).
- **비상 정지 (Emergency Stop)**: 위급 상황 시 로봇을 즉시 멈추는 기능을 제공합니다.

### 4. **유연한 설정 관리**
- **설정 UI**: 프로그램 내에서 카메라 IP, 포트, ROS2 브릿지 주소 등을 손쉽게 변경할 수 있습니다.
- **설정 저장**: 변경된 설정은 `settings.ini` 파일에 저장되어 재실행 시에도 유지됩니다.
- **듀얼 인증 모드**: 일반 RTSP 연결과 인증 정보(ID/PW)가 포함된 보안 연결 모드를 모두 지원합니다.

---

## 🛠 시스템 요구 사항 (Prerequisites)

이 프로젝트를 빌드하고 실행하기 위해서는 다음 환경이 필요합니다.

- **OS**: Windows 10 / 11 (64-bit)
- **Compiler**: MSVC (Visual Studio 2019 이상 권장) 또는 MinGW 64-bit
- **Qt Version**: Qt 6.x (Widgets, WebSockets 모듈 필수)
- **CMake**: 3.16 버전 이상

### 필수 의존성 (Dependencies)
- **GStreamer 1.0 (MSVC 버전)**
  - [GStreamer 공식 다운로드](https://gstreamer.freedesktop.org/download/)에서 **MSVC 64-bit** 버전을 설치해야 합니다.
  - **Runtime**과 **Development** 패키지 두 가지를 모두 설치해주세요.
  - 권장 설치 경로: `C:\Program Files\gstreamer\1.0\msvc_x86_64`

---

## 🚀 빌드 및 실행 방법

### 1. 프로젝트 클론
```bash
git clone https://github.com/your-repo/VEDA_QT_1.git
cd VEDA_QT_1/FE
```

### 2. Qt Creator 설정
1. **Qt Creator**에서 `CMakeLists.txt`를 엽니다.
2. `Desktop Qt %VERSION% MSVC2019 64bit` 키트를 선택합니다.
3. **Run CMake** 버튼을 클릭합니다.
   - *참고: CMake가 자동으로 시스템에 설치된 GStreamer 경로를 탐색합니다.*

### 3. 빌드 및 실행
1. 좌측 하단의 **Build (망치 아이콘)**를 클릭합니다.
2. 빌드 완료 후 **Run (초록색 재생 아이콘)**을 클릭합니다.

---

## 📂 프로젝트 구조

 유지보수와 확장성을 위해 코드가 모듈화되어 있습니다:

```
FE/
├── main.cpp                # 프로그램 진입점
├── mainwindow.cpp          # 메인 윈도우 및 전체 UI 관리
├── CMakeLists.txt          # 빌드 설정
├── style/                  # QSS 테마 (다크/라이트)
│
├── Views/                  # 주요 메인 화면
│   ├── liveview.cpp        # 4채널 CCTV 그리드
│   ├── playbackview.cpp    # 녹화 영상 목록 및 플레이어
│   └── fullscreenview.cpp  # 전체 화면 및 제어
│
├── Components/             # UI 구성 요소
│   ├── sidebar.cpp         # 컨텍스트 사이드바
│   ├── topbar.cpp          # 상단 네비게이션 및 테마 토글
│   ├── full_underbar.cpp   # 전체 화면 하단 컨트롤 (타임라인/줌)
│   ├── videocard.cpp       # 비디오 오버레이 래퍼
│   └── settingswidget.cpp  # 설정 화면
│
├── Video/                  # 비디오 위젯 및 처리
│   ├── videowidget.cpp     # GStreamer 렌더링 엔진
│   ├── livevideowidget.cpp # 라이브 스트림 처리
│   └── recordedvideowidget.cpp # 녹화 파일 재생 처리
│
├── Network/                # 통신 로직
│   ├── streammanager.cpp   # RTSP URL 생성 및 관리
│   ├── rosbridgeclient.cpp # ROS2 WebSocket 클라이언트
│   └── cameracontrolclient.cpp # PTZ 카메라 제어
│
└── Utils/
    └── configmanager.h     # 설정 관리자 (INI)
```

## 📝 라이선스

