# VEDA_QT_1 FE

Windows 기반 Qt Widgets 프론트엔드입니다.  
하나의 데스크톱 애플리케이션에서 다음 기능을 통합합니다.

- 로그인 / 회원가입 / 비밀번호 재설정
- CCTV 4채널 + RC Car 카메라 + SLAM 지도 모니터링
- 로봇 주행 모드 전환과 ROS2 WebSocket 제어
- 녹화 시작/정지, 녹화 목록 조회, 다운로드, 재생
- 카메라/로봇 설정 관리
- 쓰러짐 감지 로그 표시

이 문서는 현재 `FE/` 코드 상태를 기준으로 작성되었습니다.

---

## 1. 프로젝트 개요

앱의 기본 실행 흐름은 다음과 같습니다.

1. `main.cpp`에서 `QApplication`을 생성합니다.
2. `ApplicationInitializer`가 폰트, SSL 인증서, 런타임 경로를 초기화합니다.
3. `ConfigManager`가 `settings.ini` 기본값을 보장합니다.
4. `LoginDialog`가 먼저 열립니다.
5. 로그인 성공 시 `MainWindow`가 열리고 Live / Playback / Settings 화면을 제공합니다.

핵심 포인트:

- GStreamer는 필수입니다.
- OpenCV는 선택 사항이며, 없으면 FRUC만 비활성화됩니다.
- 설정 파일은 실행 파일과 같은 폴더의 `settings.ini`를 사용합니다.

---

## 2. 주요 기능

### 2.1 로그인 / 인증

- 로그인 서버 IP는 `192.168.0.110` 같은 호스트/IP만 입력하면 됩니다.
- 앱이 내부적으로 `http://<host>:8080` 형태로 정규화합니다.
- 회원가입은 2단계 UI이지만 서버 IP 입력은 1단계에만 있습니다.
- 로컬 마스터 우회 로그인 `admin / admin`을 지원합니다.
- 인증 성공 시 JWT 세션을 유지하고, 필요한 경우 RTSP 인증 헤더에 활용합니다.

### 2.2 Live 모니터링

- CCTV 4채널과 RC Car 카메라를 동시에 표시합니다.
- SLAM 지도와 로봇 경로 / 목표점 / 패트롤 포인트를 오버레이로 보여줍니다.
- 각 영상 카드에서 전체화면, 녹화 제어, 목표 지정 등을 연결합니다.

### 2.3 로봇 모드

사용자에게는 다음 4개 모드가 표시됩니다.

- `Manual Mode`
- `Tracking Mode`
- `Control Mode`
- `Patrol Mode`

현재 동작 규칙:

| UI 모드 | `mode_control` | `tracking_enable` | 비고 |
|---|---|---|---|
| Manual | `manual` | `false` | WASD 수동 주행 |
| Tracking | `auto` | `true` | 추적 활성화 |
| Control | `auto` | `false` | 목표 지정 제어 |
| Patrol | `auto` | `false` | 순찰 포인트 지정 |

추가 규칙:

- 모드가 `Manual`이 아니면 수동 주행은 즉시 정지됩니다.
- 즉시 정지 버튼은 모든 모드에서 항상 활성화됩니다.
- 즉시 정지 시 `cmd_vel 0`, `nav cancel`, 예상 경로, 패트롤 포인트/선이 함께 정리됩니다.

### 2.4 Patrol 모드

`Patrol Mode`에서는 사이드바에 다음 버튼이 표시됩니다.

- `Add Point`
- `설정 완료`

동작 방식:

1. `Add Point`를 켜면 Live 화면의 SLAM 지도 클릭으로 포인트를 추가할 수 있습니다.
2. 포인트는 초록 점으로 찍히고, 직전 포인트와 초록 선으로 연결됩니다.
3. `설정 완료`를 누르면 현재 포인트 목록을 `nav_command / queue` JSON으로 전송합니다.
4. 각 웨이포인트의 `yaw`는 “현재 점에서 다음 점을 향하는 방향”으로 자동 계산됩니다.

중요:

- 현재 UI는 Patrol 모드 진입 시 예전의 `nav_command / patrol` 메시지를 보내지 않습니다.
- `RosBridgeClient` 안에 `sendNavPatrol()` 헬퍼는 남아 있지만, 현재 UI 흐름에서는 사용하지 않습니다.

### 2.5 쓰러짐 감지 로그

- ROS 쪽에서 `fall_alert` JSON을 수신하면 상단 경고 아이콘에 로그가 쌓입니다.
- 예전의 빨간 오버레이 패널은 사용하지 않습니다.
- 경고 아이콘 팝업에서 쓰러짐 감지 로그를 빨간색 카드 형태로 확인할 수 있습니다.
- 로그는 최신순으로 보여주며 최대 50개까지 유지합니다.

### 2.6 Playback

- 녹화 목록 조회
- 카테고리 필터링
- 로컬 파일 재생
- 서버 다운로드 후 재생
- OpenCV 기반 FRUC 생성

### 2.7 Settings

설정 화면은 전용 사이드바를 사용하며, 두 섹션으로 나뉩니다.

- `Camera Settings`
- `Robot Car Settings`

`Camera Settings`

- Camera IP
- RTSP Port
- Use Secure RTSPS
- Use Custom CCTV URL
- Custom CCTV ID / Password

`Robot Car Settings`

- ROS2 Bridge Host
- Enable Manual Control (WASD)
- Linear X
- Angular Z
- Auto Speed

저장 시:

- `settings.ini`에 즉시 반영됩니다.
- `StreamManager` 구성이 갱신됩니다.
- `Auto Speed`는 `nav_command / set_speed` JSON으로 즉시 전송됩니다.

---

## 3. 화면 구성

### 3.1 LoginDialog

- 로그인 / 회원가입 / 비밀번호 재설정을 한 다이얼로그 안의 페이지 전환으로 처리합니다.
- 제목 왼쪽에 브랜드 아이콘이 표시됩니다.
- 로그인 서버 입력은 호스트/IP만 받습니다.

관련 파일:

- `Components/logindialog.h`
- `Components/logindialog.cpp`

### 3.2 MainWindow

`MainWindow` 구현은 역할별로 나뉘어 있습니다.

- `mainwindow.cpp`
  - 입력 처리
  - 로봇 모드 동기화
  - 패트롤 완료 처리
  - 즉시 정지
  - 쓰러짐 감지 로그 연결
- `mainwindow_ui.cpp`
  - UI 조립
  - 페이지 전환
  - 시그널 연결
- `mainwindow_session.cpp`
  - 세션 / 테마 / 설정 관련 보조 로직

### 3.3 TopBar

- Live / Playback / Settings 탭
- 날짜 / 시간
- 쓰러짐 감지 경고 버튼
- 테마 토글
- 사용자 메뉴
- 닫기 버튼

### 3.4 Sidebar

모드별로 다른 사이드바를 사용합니다.

- Live: 채널 목록 + 로봇 모드 + 즉시 정지
- Playback: 카테고리 목록
- Settings: Camera Settings / Robot Car Settings 섹션 선택

### 3.5 LiveView

- CCTV 4분할 + RC Car + SLAM 맵
- 목표 지정
- 패트롤 포인트 입력
- 경로 오버레이 표시

### 3.6 FullScreenView

- Live / Playback 공용 전체화면 뷰
- 빠른 모드 전환
- 목표 지정 및 제어 연동
- 즉시 정지 버튼 제공

### 3.7 SettingsWidget

- 스크롤 가능한 설정 화면
- 두 개의 섹션 페이지를 스택으로 관리
- 저장 시 즉시 반영

---

## 4. 디렉터리 구조

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
    icons/

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

---

## 5. 네트워크 연동

### 5.1 로그인 서버

`AuthManager`가 HTTP API를 사용합니다.

- 기본 입력 예시: `192.168.0.110`
- 실제 기본 요청 기준: `http://192.168.0.110:8080`

주요 기능:

- 로그인
- 회원가입
- 비밀번호 재설정
- 현재 사용자 프로필 조회
- 토큰 갱신

특징:

- 서버 IP만 입력해도 됩니다.
- 포트를 직접 넣지 않으면 기본 `8080`을 사용합니다.
- `QNetworkAccessManager` 기반이며 타임아웃은 5초입니다.

### 5.2 Camera Control WebSocket

기본 주소:

- `ws://<camera-ip>:9000`

현재 전송하는 메시지:

#### `CALIBRATION_CLICK`

```json
{
  "type": "CALIBRATION_CLICK",
  "payload": {
    "x1": 0.1,
    "y1": 0.2,
    "x2": 0.3,
    "y2": 0.4
  }
}
```

#### `RECORD_CONTROL`

```json
{
  "type": "RECORD_CONTROL",
  "payload": {
    "action": "start",
    "channel_id": 1
  },
  "timestamp": 1234567890.123
}
```

#### `GET_RECORDINGS`

```json
{
  "type": "GET_RECORDINGS",
  "timestamp": 1234567890.123
}
```

#### `DOWNLOAD_FILE`

```json
{
  "type": "DOWNLOAD_FILE",
  "payload": {
    "filename": "example.mp4"
  },
  "timestamp": 1234567890.123
}
```

### 5.3 ROS2 rosbridge WebSocket

기본 주소:

- `ws://<robot-host>:9090`

현재 UI가 실제로 보내는 메시지:

#### `cmd_vel`

```json
{
  "type": "cmd_vel",
  "timestamp": 1234567890.123,
  "data": {
    "linear_x": 0.2,
    "linear_y": 0.0,
    "angular_z": 0.1
  }
}
```

#### `mode_control`

```json
{
  "type": "mode_control",
  "timestamp": 1234567890.123,
  "data": {
    "mode": "manual"
  }
}
```

또는

```json
{
  "type": "mode_control",
  "timestamp": 1234567890.123,
  "data": {
    "mode": "auto"
  }
}
```

#### `tracking_enable`

```json
{
  "type": "tracking_enable",
  "data": {
    "enable": true
  }
}
```

#### `nav_command / goto`

```json
{
  "type": "nav_command",
  "timestamp": 1234567890.123,
  "data": {
    "cmd": "goto",
    "x": 1.0,
    "y": 0.5,
    "yaw": 1.57
  }
}
```

#### `nav_command / queue`

```json
{
  "type": "nav_command",
  "data": {
    "cmd": "queue",
    "waypoints": [
      { "x": 1.0, "y": 0.5, "yaw": 0.0 },
      { "x": 2.0, "y": 1.0, "yaw": 1.57 },
      { "x": 3.0, "y": 0.5, "yaw": 3.14 }
    ]
  }
}
```

#### `nav_command / set_speed`

```json
{
  "type": "nav_command",
  "data": {
    "cmd": "set_speed",
    "speed": 0.15
  }
}
```

#### `nav_command / cancel`

```json
{
  "type": "nav_command",
  "timestamp": 1234567890.123,
  "data": {
    "cmd": "cancel"
  }
}
```

#### `camera_tilt`

```json
{
  "type": "camera_tilt",
  "timestamp": 1234567890.123,
  "data": {
    "angle": 12.0
  }
}
```

현재 수신 처리하는 대표 메시지:

- `map`
- `odom`
- `path`
- `nav_status`
- `nav_feedback`
- `fall_alert`

`fall_alert` 예시:

```json
{
  "type": "fall_alert",
  "timestamp": 1743000000.0,
  "data": {
    "detected": true,
    "angle_deg": 72.3,
    "timestamp": 1743000000.0
  }
}
```

수신 시 동작:

- 상단 경고 버튼에 로그 추가
- 경고 팝업에 누적 기록 표시
- `detected: false`면 새 경고는 추가하지 않음

### 5.4 RTSP / RTSPS 규칙

`StreamManager`와 `ConfigManager`가 스트림 URL을 계산합니다.

기본 CCTV:

- Low: `rtsp://<ip>:<port>/ch1` ~ `ch4`
- High: `rtsp://<ip>:<port>/ch1_fhd` ~ `ch4_fhd`

RTSPS:

- `Use Secure RTSPS`가 켜지면 `rtsps://`를 사용합니다.
- 이때 포트는 `8322`를 사용합니다.

Custom CCTV:

- 형식: `rtsp(s)://<id>:<password>@<ip>:<port>/<index>/H.264/media.smp`
- `<index>`는 `0 ~ 3`

RC Car:

- 기본: `rtsp://<robot-host>:9554/camera`
- ISP: `rtsp://<robot-host>:9554/camera_isp`

---

## 6. 설정 파일

설정 파일 위치:

- `QCoreApplication::applicationDirPath()/settings.ini`

주요 키:

| Key | Default | 설명 |
|---|---|---|
| `Network/CameraIP` | `192.168.0.39` | CCTV IP |
| `Network/CameraPort` | `8554` | 기본 RTSP 포트 |
| `Network/UseCustomCCTV` | `false` | Custom CCTV 사용 여부 |
| `Network/CustomCCTVUsername` | `admin` | Custom CCTV ID |
| `Network/CustomCCTVPassword` | `5hanwha!` | Custom CCTV 비밀번호 |
| `Network/UseRtsps` | `false` | RTSPS 사용 여부 |
| `Network/RobotIP` | `192.168.0.237` | 로봇 호스트/IP |
| `Auth/LoginServerUrl` | `192.168.0.110` | 로그인 서버 호스트/IP |
| `Auth/ActiveUserId` | empty | 현재 로그인 사용자 ID |
| `Auth/ActiveUserEmail` | empty | 현재 로그인 사용자 이메일 |
| `Auth/ActiveAuthMode` | empty | 현재 인증 모드 |
| `Auth/RememberUser` | `false` | 로그인 ID 기억 여부 |
| `Auth/RememberedUserId` | empty | 기억된 로그인 ID |
| `UI/DarkTheme` | `true` | 다크 테마 여부 |
| `Control/ManualControl` | `false` | WASD 수동 제어 사용 여부 |
| `Control/LinearX` | `0.30` | 수동 선속도 |
| `Control/AngularZ` | `0.50` | 수동 각속도 |
| `Navigation/AutoSpeed` | `0.15` | 자동/자율 주행 속도 |

파생 규칙:

- `Network/RobotIP`에는 호스트/IP만 넣는 것을 권장합니다.
- 앱이 내부적으로 다음 주소를 계산합니다.
  - `ws://<robot-host>:9090`
  - `rtsp://<robot-host>:9554/camera`
  - `rtsp://<robot-host>:9554/camera_isp`

---

## 7. 빌드

### 7.1 요구 사항

- Windows
- Visual Studio 2022 또는 호환 MSVC 툴체인
- CMake 3.16+
- Qt 6 Widgets / Network / WebSockets
- GStreamer 1.0
- OpenCV (선택, FRUC용)

### 7.2 GStreamer 탐색 규칙

`CMakeLists.txt`는 다음 순서로 GStreamer를 찾습니다.

- `GST_ROOT` CMake 캐시
- `GSTREAMER_1_0_ROOT_MSVC_X86_64`
- `GSTREAMER_ROOT_X86_64`
- 기본 경로
  - `C:/Program Files/gstreamer/1.0/msvc_x86_64`
  - `C:/gstreamer/1.0/msvc_x86_64`
  - `D:/gstreamer/1.0/msvc_x86_64`

찾지 못하면 configure 단계에서 실패합니다.

### 7.3 OpenCV

OpenCV는 선택 사항입니다.

- `VEDA_ENABLE_FRUC=ON`이고 OpenCV를 찾으면 FRUC 활성화
- 찾지 못하면 앱은 빌드되지만 FRUC는 비활성화

사용 가능한 변수:

- `OPENCV_ROOT`
- `OpenCV_DIR`

### 7.4 예시 빌드

Qt 경로가 이미 잡혀 있다는 가정:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64"
cmake --build build --config Debug
```

FRUC 없이 빌드:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64" -DVEDA_ENABLE_FRUC=OFF
cmake --build build --config Debug
```

실행 파일:

```text
build/Debug/VEDA_QT_1.exe
```

참고:

- 개발 환경에 따라 `cmake`가 PATH에 없을 수 있습니다.
- 이 경우 Visual Studio가 설치한 `cmake.exe`를 직접 사용하거나 `deploy/package_release.ps1`처럼 `vswhere`로 경로를 찾을 수 있습니다.

### 7.5 링크 관련 참고

MSVC `Debug` / `RelWithDebInfo`에서는 `/INCREMENTAL:NO`를 사용하도록 설정되어 있습니다.  
증분 링크 산출물 꼬임으로 `LNK1163`가 보이면 다음을 먼저 확인하세요.

- 최신 CMake 재구성 여부
- `Clean` 또는 `Rebuild` 실행 여부

---

## 8. 실행과 배포

### 8.1 개발 PC에서 실행

개발 PC에서 Qt / GStreamer 런타임이 준비되어 있으면:

```text
build/Debug/VEDA_QT_1.exe
```

를 직접 실행할 수 있습니다.

### 8.2 다른 PC로 전달할 때

단순히 `exe`만 복사하면 Qt DLL / 플러그인 / GStreamer 런타임이 없어 실행되지 않을 수 있습니다.  
배포용 패키지를 만드는 것을 권장합니다.

### 8.3 배포 스크립트

`deploy/package_release.ps1`는 다음 작업을 자동 수행합니다.

- Release 빌드
- `windeployqt` 실행
- GStreamer 런타임 복사
- `style/` 폴더 복사
- `README_RUN.txt` 생성
- ZIP 패키지 생성

사용 예시:

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\package_release.ps1 -BuildDir .\build -Config Release
```

생성 결과:

- `deploy/VEDA_QT_1_release_x64/`
- `deploy/VEDA_QT_1_release_x64.zip`

---

## 9. 자주 보는 파일

처음 읽기 좋은 순서:

1. `main.cpp`
2. `mainwindow.h`
3. `mainwindow_ui.cpp`
4. `mainwindow.cpp`
5. `mainwindow_session.cpp`
6. `Components/logindialog.cpp`
7. `Components/sidebar.cpp`
8. `Components/settingswidget.cpp`
9. `Components/topbar.cpp`
10. `Views/liveview.cpp`
11. `Views/fullscreenview.cpp`
12. `Views/playbackview.cpp`
13. `Network/authmanager.cpp`
14. `Network/streammanager.cpp`
15. `Network/rosbridgeclient.cpp`
16. `Network/cameracontrolclient.cpp`

---

## 10. 트러블슈팅

### 10.1 `cmake` 명령을 찾지 못함

증상:

- PowerShell에서 `cmake`가 없다고 나옴

대응:

- Visual Studio CMake 경로를 직접 사용
- 또는 Visual Studio Developer PowerShell 사용

예:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Debug
```

### 10.2 GStreamer를 찾지 못함

확인 항목:

- `GST_ROOT`
- `GSTREAMER_1_0_ROOT_MSVC_X86_64`
- 실제 설치 경로 존재 여부

### 10.3 OpenCV를 찾지 못함

확인 항목:

- `OPENCV_ROOT`
- `OpenCV_DIR`

영향:

- 앱 자체 빌드는 가능
- FRUC만 비활성화

### 10.4 다른 PC에서 실행되지 않음

주요 원인:

- Qt DLL 누락
- Qt 플러그인 누락
- GStreamer DLL / 플러그인 누락

해결:

- `deploy/package_release.ps1`로 패키징

### 10.5 ROS 연결은 되는데 쓰러짐 감지 로그가 안 쌓임

확인 항목:

- 실제 수신 WebSocket이 `fall_alert` 메시지를 받는지
- 메시지에 `detected`, `angle_deg`, `timestamp`가 포함되는지
- 상단 경고 버튼 로그에 기록이 추가되는지

참고:

- 현재 클라이언트는 텍스트 프레임 / 바이너리 프레임 / 중첩된 `data` / `payload` / `msg` 형태도 폭넓게 파싱합니다.

### 10.6 Patrol 경로가 안 보내짐

확인 항목:

- `Patrol Mode`인지
- `Add Point`로 최소 1개 이상 포인트를 찍었는지
- `설정 완료`를 눌렀는지

### 10.7 즉시 정지 후 패트롤 점이나 경로가 남아 있음

현재 의도된 동작:

- 예상 경로 삭제
- 패트롤 점/선 삭제
- `Add Point` 상태 해제
- `nav cancel` 전송
- `cmd_vel 0` 전송

이와 다르게 보이면 최신 빌드인지 먼저 확인하세요.

---

## 11. 요약

이 프론트엔드는 다음을 하나의 Qt 앱으로 묶습니다.

- 인증과 세션 관리
- CCTV / RC Car 실시간 모니터링
- Tracking / Control / Patrol 포함 로봇 제어
- 녹화 / 다운로드 / 재생 / FRUC
- 카메라 / 로봇 설정 관리
- 쓰러짐 감지 로그 확인

현재 구조는 `MainWindow + Views + Components + Network + Video + Utils`로 정리되어 있으며, README도 그 구조와 현재 동작을 기준으로 유지합니다.
