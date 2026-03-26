# VEDA_QT_1 FE

Windows 기반 Qt Widgets 프론트엔드입니다.  
`누비고` UI에서 CCTV 모니터링, RC Car 영상 확인, ROS2 기반 로봇 제어, 녹화/다운로드/재생, 설정 관리까지 한 번에 처리합니다.

핵심 스택:

- Qt Widgets
- Qt Network / WebSockets
- GStreamer 1.0
- OpenCV 선택 지원(FRUC 후처리용)

이 README는 현재 `FE/` 코드 상태를 기준으로 작성되었습니다.

---

## 1. 프로젝트 개요

앱은 다음 순서로 동작합니다.

1. `main.cpp`에서 `QApplication`을 생성합니다.
2. `ApplicationInitializer`가 OpenCV, GStreamer, 폰트, SSL 인증서를 초기화합니다.
3. `ConfigManager`가 `settings.ini` 기본값을 보장합니다.
4. `LoginDialog`를 먼저 띄웁니다.
5. 로그인 성공 시 `MainWindow`를 열고 라이브/재생/설정 UI를 구동합니다.

중요한 점:

- 초기화 일부가 실패해도 앱은 경고 로그만 남기고 계속 실행될 수 있습니다.
- GStreamer가 없으면 스트림/재생이 제한됩니다.
- OpenCV가 없으면 FRUC만 비활성화되고 앱 자체는 빌드/실행 가능합니다.

---

## 2. 핵심 기능

- 로그인, 회원가입, 비밀번호 재설정
- CCTV 4채널 + RC Car 전면 카메라 + SLAM 맵 표시
- RTSP / RTSPS 스트림 재생
- 전체화면, 줌, 박스 줌, Goal Overlay
- 로봇 Manual / Auto / Control / Patrol 모드 전환
- 즉시정지 버튼 지원
  - 사이드바와 전체화면 퀵 패널 모두에서 사용 가능
  - 현재는 Manual / Auto / Control / Patrol 모든 모드에서 활성화됨
- 녹화 시작/정지, 녹화 목록 조회, 다운로드, 로컬 재생
- OpenCV 기반 FRUC 후처리
  - `_FRUC_FAST`
  - `_FRUC_HQ`
- 다크/라이트 테마
- 카메라/로봇 설정 저장

---

## 3. 현재 아키텍처

### 3.1 엔트리 포인트

- [main.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/main.cpp)
  - `QApplication` 생성
  - `ApplicationInitializer::initializeEnvironment()`
  - `ConfigManager::loadDefaults()`
  - `LoginDialog` 실행
  - `MainWindow` 실행

### 3.2 MainWindow 분리 구조

`MainWindow` 관련 코드는 3개 파일로 나뉘어 있습니다.

- [mainwindow.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/mainwindow.cpp)
  - 키 입력 처리
  - 수동 조종
  - 카메라 틸트 입력
  - Goal 추적
  - 긴급 정지 공통 처리
- [mainwindow_ui.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/mainwindow_ui.cpp)
  - 메인 화면 조립
  - 시그널/슬롯 연결
  - 페이지 전환
  - 라이브/재생/전체화면 UI 연동
- [mainwindow_session.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/mainwindow_session.cpp)
  - 테마 전환
  - 재로그인
  - 로그아웃
  - FRUC 후처리
  - 백그라운드 작업 종료 처리

### 3.3 폴더 역할

```text
FE/
  main.cpp
  mainwindow.h
  mainwindow.cpp
  mainwindow_ui.cpp
  mainwindow_session.cpp
  CMakeLists.txt
  resources.qrc
  README.md

  assets/
    fonts/

  style/
    theme_dark.qss
    theme_light.qss
    icons/

  deploy/
    package_release.ps1

  Components/
    full_underbar.*
    logindialog.*
    osdwidget.*
    settingswidget.*
    sidebar.*
    slammapwidget.*
    topbar.*
    videocard.*

  Views/
    fullscreenview.*
    liveview.*
    playbackview.*

  Video/
    frucvideoprocessor.*
    livevideowidget.*
    recordedvideowidget.*
    rtsppinger.*
    videowidget.*
    Gst/
      GstQualityMonitor.*
      GstStatsCollector.hpp

  Network/
    authmanager.*
    cameracontrolclient.*
    rosbridgeclient.*
    streammanager.*

  Utils/
    applicationinitializer.*
    channelcatalog.*
    configmanager.h
    constants.h
    framelessconfirmdialog.*
    goaloverlaycontroller.*
    jsonutils.*
```

### 3.4 공용 유틸

- [applicationinitializer.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Utils/applicationinitializer.cpp)
  - GStreamer/OpenCV PATH 설정
  - Pretendard 폰트 로드
  - `env/server.crt` 등록
- [configmanager.h](/d:/work/QT_prac/VEDA_QT_1/FE/Utils/configmanager.h)
  - `settings.ini` 읽기/쓰기
  - 기본값 보장
  - 파생 URL 계산
- [channelcatalog.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Utils/channelcatalog.cpp)
  - 채널 번호 / 카테고리 / 녹화 채널 ID 매핑
- [goaloverlaycontroller.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Utils/goaloverlaycontroller.cpp)
  - Goal 오버레이 정규화 좌표 / 재투영 처리
- [jsonutils.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Utils/jsonutils.cpp)
  - odom / nav feedback / nav status 파싱 보조

---

## 4. 화면 구성

### 4.1 LoginDialog

- 로그인 / 회원가입 / 비밀번호 재설정 UI를 하나의 다이얼로그 안에서 스택 페이지로 전환합니다.
- 별도 `signupdialog.*` 파일은 현재 사용하지 않습니다.
- 앱 이름은 `누비고`로 통일되어 있습니다.

관련 파일:

- [logindialog.h](/d:/work/QT_prac/VEDA_QT_1/FE/Components/logindialog.h)
- [logindialog.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Components/logindialog.cpp)

### 4.2 LiveView

- CCTV 4분할 + RC Car + SLAM 뷰를 관리합니다.
- 각 카드에서 전체화면, 녹화, Goal Overlay 조작을 연결합니다.

관련 파일:

- [liveview.h](/d:/work/QT_prac/VEDA_QT_1/FE/Views/liveview.h)
- [liveview.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Views/liveview.cpp)
- [videocard.h](/d:/work/QT_prac/VEDA_QT_1/FE/Components/videocard.h)
- [videocard.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Components/videocard.cpp)

### 4.3 FullScreenView

- 라이브 / 녹화 재생 공용 전체화면 화면입니다.
- 줌, 박스 줌, 리셋, 컨트롤 모드, Goal 드래그, 긴급 정지를 처리합니다.
- 전체화면 우측 퀵 패널에서도 로봇 모드 전환과 즉시정지가 가능합니다.

관련 파일:

- [fullscreenview.h](/d:/work/QT_prac/VEDA_QT_1/FE/Views/fullscreenview.h)
- [fullscreenview.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Views/fullscreenview.cpp)
- [full_underbar.h](/d:/work/QT_prac/VEDA_QT_1/FE/Components/full_underbar.h)
- [full_underbar.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Components/full_underbar.cpp)

### 4.4 PlaybackView

- 녹화 목록 표시
- 카테고리 필터
- 다운로드 진행률 반영
- 더블 클릭 재생
- 로컬 파일 추가

관련 파일:

- [playbackview.h](/d:/work/QT_prac/VEDA_QT_1/FE/Views/playbackview.h)
- [playbackview.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Views/playbackview.cpp)

### 4.5 SettingsWidget

카메라와 로봇 설정을 2개 섹션으로 분리해서 관리합니다.

- Camera Settings
  - Camera IP
  - RTSP Port
  - Use Secure RTSPS
  - Use Custom CCTV URL
  - Custom CCTV ID / Password
- Robot Car Settings
  - ROS2 Bridge Host
  - Enable Manual Control
  - Linear X
  - Angular Z
  - Auto Speed

관련 파일:

- [settingswidget.h](/d:/work/QT_prac/VEDA_QT_1/FE/Components/settingswidget.h)
- [settingswidget.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Components/settingswidget.cpp)

---

## 5. 네트워크 / 스트림 규칙

### 5.1 로그인 서버

`AuthManager`가 로그인/회원가입/비밀번호 재설정/세션 갱신을 담당합니다.

정규화 규칙:

- 스킴이 없으면 `http://`를 자동으로 붙입니다.
- 포트가 없으면 기본 `8080`을 사용합니다.
- 마지막 `/`는 제거합니다.

예:

- 입력: `192.168.0.110`
- 실제 요청 기준: `http://192.168.0.110:8080`

관련 파일:

- [authmanager.h](/d:/work/QT_prac/VEDA_QT_1/FE/Network/authmanager.h)
- [authmanager.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/authmanager.cpp)

### 5.2 Camera Control WebSocket

카메라 제어 서버 주소:

- `ws://<camera-ip>:9000`

주요 요청:

- `CALIBRATION_CLICK`
- `RECORD_CONTROL`
- `GET_RECORDINGS`
- `DOWNLOAD_FILE`

주요 응답:

- 녹화 목록
- 다운로드 바이너리
- 다운로드 진행률
- SLAM mapping error

관련 파일:

- [cameracontrolclient.h](/d:/work/QT_prac/VEDA_QT_1/FE/Network/cameracontrolclient.h)
- [cameracontrolclient.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/cameracontrolclient.cpp)

### 5.3 ROS2 rosbridge WebSocket

로봇 제어 서버 주소:

- `ws://<robot-host>:9090`

주요 송신:

- `cmd_vel`
- `mode_control`
- `nav queue`
- `goal pose`
- `nav cancel`
- `camera tilt`

주요 수신:

- map
- odom
- path
- nav status
- nav feedback

관련 파일:

- [rosbridgeclient.h](/d:/work/QT_prac/VEDA_QT_1/FE/Network/rosbridgeclient.h)
- [rosbridgeclient.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/rosbridgeclient.cpp)

### 5.4 CCTV 스트림 URL 규칙

`StreamManager`가 CCTV 스트림 주소를 계산합니다.

기본 모드:

- Low: `rtsp://<ip>:<port>/ch1` ~ `ch4`
- High: `rtsp://<ip>:<port>/ch1_fhd` ~ `ch4_fhd`

RTSPS 모드:

- 스킴을 `rtsps://`로 변경합니다.
- 포트는 설정값과 관계없이 `8322`를 사용합니다.

Custom CCTV 모드:

- 형식: `rtsp(s)://<id>:<password>@<ip>:<port>/<index>/H.264/media.smp`
- `<index>`는 `0 ~ 3`
- ID / 비밀번호는 percent-encoding 후 삽입됩니다.

관련 파일:

- [streammanager.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/streammanager.cpp)

### 5.5 RC Car 스트림 URL

`ConfigManager`에서 로봇 호스트를 기준으로 파생합니다.

- 라이브: `rtsp://<robot-host>:9554/camera`
- ISP/전체화면: `rtsp://<robot-host>:9554/camera_isp`

### 5.6 Calibration Click 대상 서버

캘리브레이션 클릭은 고정 IP를 사용하지 않습니다.  
현재 설정된 `Camera IP`를 그대로 사용해서 다음 주소로 전송합니다.

- `ws://<camera-ip>:9000`

---

## 6. 채널 / 녹화 매핑

`ChannelCatalog` 기준 매핑입니다.

### 6.1 라이브 / 전체화면 녹화 채널

| View Index | 이름 | Live Record ID | FullScreen Record ID |
|---|---|---:|---:|
| 0 | Channel 01 - Camera | 1 | 5 |
| 1 | Channel 02 - Camera | 2 | 6 |
| 2 | Channel 03 - Camera | 3 | 7 |
| 3 | Channel 04 - Camera | 4 | 8 |
| 4 | RC Car - Front Cam | 9 | 9 |

### 6.2 Playback 카테고리

| Category ID | 이름 |
|---|---|
| 0 | All Recordings |
| 1 | CCTV 1 (Low) |
| 2 | CCTV 2 (Low) |
| 3 | CCTV 3 (Low) |
| 4 | CCTV 4 (Low) |
| 5 | CCTV 1 (High) |
| 6 | CCTV 2 (High) |
| 7 | CCTV 3 (High) |
| 8 | CCTV 4 (High) |
| 9 | RC Car Camera |
| 10 | Lidar Map |

---

## 7. 녹화 / 다운로드 / 재생 / FRUC 흐름

### 7.1 녹화

1. 라이브 카드 또는 전체화면에서 녹화 시작
2. `CameraControlClient::sendRecordCommand()`
3. 녹화 완료 응답 수신
4. 녹화 목록 새로고침

### 7.2 다운로드

1. 녹화 URL 또는 파일명 확보
2. 로컬 Downloads 폴더 존재 여부 확인
3. 없으면 `DOWNLOAD_FILE` 요청
4. 바이너리 수신 후 파일 저장
5. Playback 목록에 반영

### 7.3 재생

1. Playback 항목 더블 클릭
2. 로컬 파일이 있으면 즉시 재생
3. 없으면 다운로드 후 재생

### 7.4 FRUC

OpenCV가 활성화된 빌드에서만 동작합니다.

출력 파일:

- `*_FRUC_FAST.mp4`
- `*_FRUC_HQ.mp4`

조건:

- 입력 파일이 MP4여야 함
- 이미 FRUC 파생 파일이면 제외
- 동일 결과 파일이 이미 존재하면 다시 만들지 않음

종료 안정성:

- FRUC는 백그라운드 `QThread`로 동작합니다.
- 현재 코드는 앱 종료 시 실행 중인 FRUC 작업에 중단 요청을 보내고 정리하도록 되어 있습니다.

---

## 8. settings.ini

설정 파일 위치:

- `QCoreApplication::applicationDirPath()/settings.ini`

주요 키:

| Key | Default | 설명 |
|---|---|---|
| `Network/CameraIP` | `192.168.0.39` | CCTV IP |
| `Network/CameraPort` | `8554` | 기본 RTSP 포트 |
| `Network/UseCustomCCTV` | `false` | Custom CCTV URL 사용 여부 |
| `Network/CustomCCTVUsername` | `admin` | Custom CCTV ID |
| `Network/CustomCCTVPassword` | `5hanwha!` | Custom CCTV 비밀번호 |
| `Network/UseRtsps` | `false` | RTSPS 사용 여부 |
| `Network/RobotIP` | `192.168.0.237` | 로봇 호스트 또는 URL 입력값 |
| `Auth/LoginServerUrl` | `192.168.0.110` | 로그인 서버 입력값 |
| `Auth/ActiveUserId` | empty | 현재 로그인 사용자 ID |
| `Auth/ActiveUserEmail` | empty | 현재 로그인 사용자 이메일 |
| `Auth/ActiveAuthMode` | empty | 현재 인증 모드 |
| `Auth/RememberUser` | `false` | Remember device 여부 |
| `Auth/RememberedUserId` | empty | 저장된 사용자 ID |
| `UI/DarkTheme` | `true` | 다크 테마 사용 여부 |
| `Control/ManualControl` | `false` | WASD 수동 제어 사용 여부 |
| `Control/LinearX` | `0.30` | 수동 선속도 |
| `Control/AngularZ` | `0.50` | 수동 각속도 |
| `Navigation/AutoSpeed` | `0.15` | 자율주행 속도 |

파생 규칙:

- `Network/RobotIP`에 호스트만 넣어도 됩니다.
- 내부에서 자동으로 다음 주소를 파생합니다.
  - `ws://<robot-host>:9090`
  - `rtsp://<robot-host>:9554/camera`
  - `rtsp://<robot-host>:9554/camera_isp`

---

## 9. 빌드

### 9.1 요구 사항

- Windows
- CMake 3.16+
- Qt 6 Widgets / WebSockets / Network
- GStreamer 1.0
- OpenCV 선택 사항
- Visual Studio 2022 또는 호환 MSVC 툴체인

### 9.2 GStreamer 경로

현재 CMake는 다음 순서로 GStreamer를 찾습니다.

- `GST_ROOT` CMake cache 값
- `GSTREAMER_1_0_ROOT_MSVC_X86_64`
- `GSTREAMER_ROOT_X86_64`
- 기본 후보 경로
  - `C:/Program Files/gstreamer/1.0/msvc_x86_64`
  - `C:/gstreamer/1.0/msvc_x86_64`
  - `D:/gstreamer/1.0/msvc_x86_64`

GStreamer를 찾지 못하면 configure 단계에서 실패합니다.

### 9.3 OpenCV 경로

OpenCV는 선택 사항입니다.

- `OPENCV_ROOT`
- `OpenCV_DIR`

를 지정할 수 있습니다.

찾지 못하면:

- 앱은 빌드 가능
- FRUC만 비활성화

### 9.4 예시 빌드

Visual Studio 2022 기준:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64"
cmake --build build --config Debug
```

OpenCV 없이 FRUC만 끄고 빌드:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64" -DVEDA_ENABLE_FRUC=OFF
cmake --build build --config Debug
```

실행 파일:

```text
build/Debug/VEDA_QT_1.exe
```

---

## 10. 실행 / 배포

### 10.1 개발 PC에서 실행

개발 PC에서는 Qt / GStreamer 런타임이 이미 PATH에 잡혀 있으면 `build/Debug/VEDA_QT_1.exe`를 바로 실행할 수 있습니다.

### 10.2 다른 PC에서 실행

`build/Debug` 폴더의 exe만 복사하면 Qt DLL / plugin / GStreamer DLL 부족으로 실행이 실패할 수 있습니다.  
다른 PC로 옮길 때는 릴리스 패키징 스크립트를 사용하는 것을 권장합니다.

### 10.3 릴리스 패키징

[package_release.ps1](/d:/work/QT_prac/VEDA_QT_1/FE/deploy/package_release.ps1)는 다음을 자동으로 수행합니다.

- Release 빌드
- `windeployqt` 실행
- GStreamer DLL 복사
- `gstreamer/lib/gstreamer-1.0`
- `gstreamer/libexec/gstreamer-1.0`
- `style/` 폴더 복사
- zip 생성

예:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\package_release.ps1 -BuildDir .\build -Config Release
```

결과물:

- `deploy/VEDA_QT_1_release_x64/`
- `deploy/VEDA_QT_1_release_x64.zip`

---

## 11. 리소스

`resources.qrc`에 포함된 항목:

- `style/theme_dark.qss`
- `style/theme_light.qss`
- `style/icons/*.svg`
- `assets/fonts/Pretendard-*.otf`
- `env/server.crt`

즉, 테마와 폰트의 핵심 리소스는 exe 내부 리소스로도 접근 가능합니다.

---

## 12. 현재 코드에서 중요한 포인트

- 앱 이름은 `누비고`로 통일되어 있습니다.
- 회원가입은 `LoginDialog` 내부 페이지 전환 방식입니다.
- Calibration 대상 서버는 고정 IP가 아니라 현재 `Camera IP` 설정값을 사용합니다.
- 즉시정지 버튼은 현재 모든 로봇 모드에서 활성화됩니다.
- FRUC 스레드는 종료 시 중단 요청 후 정리됩니다.

---

## 13. 트러블슈팅

### 13.1 GStreamer를 찾지 못함

확인할 것:

- `GST_ROOT` 또는 `GSTREAMER_*` 환경변수
- GStreamer 설치 경로
- `bin`, `lib`, `libexec` 구조가 정상인지

### 13.2 OpenCV를 찾지 못함

확인할 것:

- `OPENCV_ROOT`
- `OpenCV_DIR`

OpenCV가 없어도 앱은 빌드되지만 FRUC는 비활성화됩니다.

### 13.3 다른 PC에서 exe가 실행되지 않음

가장 흔한 원인:

- Qt DLL 누락
- Qt plugin 누락
- GStreamer DLL / plugin 누락

해결:

- `deploy/package_release.ps1`로 패키징
- 또는 `windeployqt` + GStreamer runtime 수동 복사

### 13.4 RTSPS가 재생되지 않음

확인할 것:

- `Use Secure RTSPS` 활성화 여부
- 실제 포트가 `8322`인지
- `env/server.crt`와 서버 인증서 일치 여부
- 로그인 세션이 유효한지

### 13.5 Custom CCTV가 연결되지 않음

확인할 것:

- `Use Custom CCTV URL` 활성화 여부
- ID / 비밀번호 오입력 여부
- 장비가 `/<index>/H.264/media.smp` 규칙을 따르는지
- 계정 정보에 특수문자가 포함된 경우 percent-encoding이 필요한지

### 13.6 종료 시 `QThread: Destroyed while thread is still running`

최근 코드에서는 FRUC 작업 종료 경로를 정리했습니다.  
그래도 같은 오류가 보이면:

- 구버전 exe를 실행 중인지 확인
- 종료 시 FRUC 작업이 오래 걸리는지 확인
- 최신 빌드로 다시 확인

---

## 14. 빠른 파일 가이드

처음 읽기 좋은 순서:

1. [main.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/main.cpp)
2. [mainwindow.h](/d:/work/QT_prac/VEDA_QT_1/FE/mainwindow.h)
3. [mainwindow_ui.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/mainwindow_ui.cpp)
4. [mainwindow.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/mainwindow.cpp)
5. [mainwindow_session.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/mainwindow_session.cpp)
6. [authmanager.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/authmanager.cpp)
7. [streammanager.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/streammanager.cpp)
8. [rosbridgeclient.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/rosbridgeclient.cpp)
9. [cameracontrolclient.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Network/cameracontrolclient.cpp)
10. [liveview.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Views/liveview.cpp)
11. [fullscreenview.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Views/fullscreenview.cpp)
12. [playbackview.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Views/playbackview.cpp)
13. [settingswidget.cpp](/d:/work/QT_prac/VEDA_QT_1/FE/Components/settingswidget.cpp)

---

## 15. 요약

이 프론트엔드는 다음을 하나의 Qt 앱 안에 통합합니다.

- 로그인과 세션 관리
- CCTV / RC Car 라이브 모니터링
- 전체화면 조작과 Goal Overlay
- ROS2 기반 로봇 제어
- 녹화 / 다운로드 / 재생 / FRUC
- 설정과 테마 관리

현재 구조는 `MainWindow + Views + Components + Video + Network + Utils` 형태로 정리되어 있으며, 문서와 코드가 최대한 같은 기준을 보도록 갱신해두었습니다.
