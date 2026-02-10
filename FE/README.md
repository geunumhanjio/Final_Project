# VEDA_QT_1 - CCTV 통합 관제 시스템

이 프로젝트는 Qt와 GStreamer를 활용하여 개발된 CCTV 관제 시스템입니다.
4개의 RTSP 카메라 스트림을 동시에 모니터링하고, 특정 채널을 전체 화면으로 확대하여 제어할 수 있는 기능을 제공합니다.

## 📂 프로젝트 구조 및 모듈 설명

### 1. **Core (핵심 기능)**
- **`main.cpp`**: 프로그램의 진입점입니다. `QApplication`을 시작하고 `MainWindow`를 실행합니다.
- **`mainwindow.cpp / .h`**: 메인 윈도우 클래스입니다.
  - 전체 레이아웃(`QStackedWidget`)을 관리하며, 상단바, 사이드바, 그리고 각 페이지(라이브 뷰, 전체 화면, 설정 등)를 제어합니다.

### 2. **UI Components (화면 구성 요소)**
- **`liveview.cpp / .h`**: **실시간 관제 화면**입니다.
  - 4분할 그리드(`QGridLayout`)로 CCTV 영상을 보여줍니다.
  - `VideoCard` 위젯을 사용하여 각 채널을 표시합니다.
- **`fullscreenview.cpp / .h`**: **전체 화면 보기**입니다.
  - 특정 채널을 더블 클릭했을 때 전환되는 화면입니다.
  - 상단바(타이틀, 닫기), 비디오 영역, 하단바(컨트롤)로 구성되어 있습니다.
  - 줌, 팬(이동), 사각형 확대 기능을 제공합니다.
- **`sidebar.cpp / .h`**: **좌측 사이드바**입니다.
  - 채널 목록(Camera 1~4)을 표시하고, 체크박스를 통해 각 카메라의 가시성을 토글(ON/OFF)할 수 있습니다.
- **`topbar.cpp / .h`**: **상단 네비게이션 바**입니다.
  - 사이드바 열기/닫기, 테마 변경(Dark/Light), 페이지 전환 탭(라이브 뷰, 기록, 설정) 기능을 제공합니다.
- **`full_underbar.cpp / .h`**: 전체 화면 모드 하단에 표시되는 **컨트롤 바**입니다.
  - 줌 인/아웃, 리셋 버튼 등을 포함합니다.

### 3. **Video Processing (영상 처리)**
- **`videowidget.cpp / .h`**: **영상 재생의 핵심 엔진**입니다.
  - `GStreamer` 파이프라인을 직접 생성하고 관리합니다 (`rtspsrc` -> `rtph264depay` -> `avdec_h264` ...).
  - 영상의 크롭(Crop) 및 확대를 위해 `videocrop` 요소를 사용합니다.
- **`videocard.cpp / .h`**: `VideoWidget`을 감싸는 **래퍼(Wrapper) 위젯**입니다.
  - 영상 위에 오버레이되는 채널 이름, 상태 표시 등의 UI 요소를 추가로 관리합니다.

### 4. **Configuration & Management (설정 및 관리)**
- **`configmanager.h`**: **설정 관리자 (싱글톤)**입니다.
  - `settings.ini` 파일에 IP와 포트 번호를 영구적으로 저장하고 불러옵니다.
- **`streammanager.h / .cpp`**: **스트림 주소 관리자 (싱글톤)**입니다.
  - `ConfigManager`의 설정값을 바탕으로 각 채널의 RTSP URL(`rtsp://IP:Port/ch...`)을 생성하여 제공합니다.
- **`settingswidget.h`**: **설정 변경 화면**입니다.
  - 사용자가 GUI 환경에서 카메라 자산 IP와 포트를 변경하고 저장할 수 있도록 합니다.

---

## ⚙️ 빌드 및 실행 방법

### 요구 사항
- **Qt 6.x** (Widgets 모듈)
- **GStreamer 1.0 (MinGW 64bit)**
- **CMake 3.16+**

### 빌드 단계
1. 프로젝트 루트에서 `build` 디렉토리 생성
2. `cmake ..` 명령어로 프로젝트 구성 (GStreamer 경로 확인 필요)
3. `cmake --build .` 명령어로 컴파일 및 실행 파일 생성

## 📝 설정 파일 (settings.ini)
프로그램을 한 번 실행하면 실행 파일과 같은 경로에 `settings.ini`가 생성됩니다.
```ini
[Network]
CameraIP=192.168.0.39
CameraPort=8554
```
이 파일을 직접 수정하거나, 프로그램 내 **[설정 화면]** 탭에서 변경할 수 있습니다.
