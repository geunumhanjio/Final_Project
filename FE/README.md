# VEDA_QT_1 FE

Qt Widgets, GStreamer 기반의 CCTV 통합 관제 및 로봇 제어 프론트엔드입니다. OpenCV가 설치된 환경에서는 FRUC 후처리도 함께 활성화됩니다.

이 애플리케이션은 다음 기능을 하나의 데스크톱 앱에서 제공합니다.

- 운영자 로그인, 회원가입, 비밀번호 재설정
- CCTV 4채널 + RC Car 카메라 실시간 모니터링
- RTSP / RTSPS 스트림 재생
- 녹화 시작/정지, 녹화 목록 조회, 다운로드, 로컬 재생
- ROS2 rosbridge 기반 로봇 수동/자율 제어
- Goal Pose 지정 및 상태 추적
- SLAM 맵 표시 및 순찰 경로 포인트 지정
- 스트림 품질 OSD 표시
- 다크/라이트 테마, 카메라/로봇 설정 저장

---

## 1. 프로젝트 개요

프로그램은 `main.cpp`에서 시작됩니다.

1. Qt 애플리케이션, GStreamer, SSL 인증서, 폰트 리소스를 초기화합니다.
2. `settings.ini` 기본값을 로드합니다.
3. `LoginDialog`를 먼저 표시합니다.
4. 로그인 성공 시 `MainWindow`를 열고 라이브/재생/설정 화면을 통합 운영합니다.

현재 구조는 "페이지 + 네트워크 + 비디오 + 공용 유틸" 형태로 정리되어 있으며, 메인 셸 역할은 `MainWindow`가 담당합니다.

---

## 2. 현재 코드 기준 핵심 기능

### 2.1 인증 및 세션

- 로그인 서버 주소를 받아 HTTP API로 로그인합니다.
- 액세스 토큰 / 리프레시 토큰 기반 세션을 유지합니다.
- 토큰 만료 임박 시 자동 갱신을 시도합니다.
- 세션 만료 시 로그인 다이얼로그를 다시 띄웁니다.
- 현재 사용자 ID / 이메일 / 서버 정보를 TopBar에 반영합니다.

관련 코드:

- `main.cpp`
- `Components/logindialog.*`
- `Network/authmanager.*`
- `mainwindow_session.cpp`

### 2.2 실시간 모니터링

- CCTV 1~4와 RC Car 전면 카메라를 라이브로 표시합니다.
- 각 카드에서 녹화, 전체화면, Goal Overlay 조작이 가능합니다.
- RC Car 옆 패널에 SLAM 맵을 함께 표시합니다.
- 설정 변경 시 스트림 URL을 다시 계산해 재연결합니다.

관련 코드:

- `Views/liveview.*`
- `Components/videocard.*`
- `Video/livevideowidget.*`
- `Network/streammanager.*`

### 2.3 전체화면 및 제어 모드

- 선택한 스트림을 전체화면으로 확장 표시합니다.
- 확대/축소, 패닝, 사각형 줌을 지원합니다.
- Control Mode에서 방향 화살표 오버레이로 목표를 지정합니다.
- 라이브 카드와 전체화면에서 같은 Goal Overlay 로직을 공유합니다.

관련 코드:

- `Views/fullscreenview.*`
- `Components/full_underbar.*`
- `Utils/goaloverlaycontroller.*`

### 2.4 로봇 제어

- WASD 기반 수동 제어
- Goal Pose 전송
- Manual / Auto / Control / Patrol 모드 전환
- 비상 정지
- nav status / feedback / odom 기반 목표 상태 추적
- 순찰 포인트 큐 전송

관련 코드:

- `mainwindow.cpp`
- `Network/rosbridgeclient.*`
- `Components/slammapwidget.*`
- `Views/fullscreenview.*`

### 2.5 녹화, 다운로드, 재생, FRUC

- 카메라 제어 WebSocket으로 녹화 시작/정지를 요청합니다.
- 서버 녹화 목록을 받아 PlaybackView에 표시합니다.
- 더블 클릭 시 로컬 파일이 있으면 바로 재생하고, 없으면 다운로드를 요청합니다.
- 다운로드 완료 후 로컬 파일 목록에 추가합니다.
- OpenCV가 활성화된 빌드에서는 원본 MP4에 대해 `_FRUC_FAST`, `_FRUC_HQ` 파생 파일을 자동 생성합니다.

관련 코드:

- `Views/playbackview.*`
- `Network/cameracontrolclient.*`
- `Video/recordedvideowidget.*`
- `Video/frucvideoprocessor.*`

### 2.6 OSD 및 스트림 상태

화면 오버레이로 다음 지표를 표시할 수 있습니다.

- Packet Loss
- Jitter
- FPS
- Bitrate
- Latency

관련 코드:

- `Components/osdwidget.*`
- `Video/videowidget.*`
- `Video/Gst/GstQualityMonitor.*`
- `Video/Gst/GstStatsCollector.hpp`

### 2.7 설정 및 테마

설정 화면에서 다음 값을 저장합니다.

- 카메라 IP / 포트
- Custom CCTV 사용 여부
- Custom CCTV ID / 비밀번호
- RTSPS 사용 여부
- ROS2 브리지 호스트
- 수동 제어 허용 여부
- 수동 선속도 / 각속도
- 자율주행 속도
- 다크 테마 여부

설정은 실행 파일 기준 `settings.ini`에 저장됩니다.

관련 코드:

- `Components/settingswidget.*`
- `Utils/configmanager.h`

---

## 3. 현재 코드 분석 요약

### 3.1 진입 흐름

- `main.cpp`
  - GStreamer / OpenCV runtime 경로 설정
  - Pretendard 폰트 로드
  - `env/server.crt`를 Qt SSL 기본 CA 설정에 등록
  - `LoginDialog` 실행
  - 로그인 성공 시 `MainWindow` 실행

### 3.2 MainWindow 분리 구조

현재 `MainWindow` 관련 코드는 3개 파일로 나뉘어 있습니다.

- `mainwindow.cpp`
  - 로봇 제어 입력 처리
  - Goal tracking
  - ROS 메시지 후처리
  - 공용 오버레이 상태 관리
- `mainwindow_ui.cpp`
  - 화면 생성
  - 시그널/슬롯 연결
  - 페이지 전환
  - 녹화/다운로드/재생 연결
- `mainwindow_session.cpp`
  - 로그아웃
  - 세션 재로그인
  - 테마 토글
  - FRUC 후처리

### 3.3 기능별 책임

- `Views`
  - 페이지 단위 컨테이너
- `Components`
  - 카드, 바, 다이얼로그, 세부 위젯
- `Video`
  - GStreamer/OpenCV 기반 재생 및 후처리
- `Network`
  - 로그인 API, 카메라 제어 WS, rosbridge WS, 스트림 URL 계산
- `Utils`
  - 설정, 채널 매핑, Goal Overlay, 공용 확인 다이얼로그

### 3.4 현재 코드에서 눈에 띄는 운영 포인트

- `settings.ini`는 애플리케이션 실행 파일 폴더에 생성됩니다.
- `UseRtsps=true`일 때 CCTV 포트는 설정 포트가 아니라 `8322`로 강제됩니다.
- `UseCustomCCTV=true`일 때 CCTV URL은 사용자 입력 ID/비밀번호를 포함한 `rtsp(s)://<id>:<password>@...` 형태로 생성됩니다.
- RC Car 스트림은 로봇 호스트에서 파생됩니다.
  - 기본 라이브: `rtsp://<robot-host>:9554/camera`
  - RC 전체화면: `rtsp://<robot-host>:9554/camera_isp`
- Channel 2의 calibration click은 현재 고정 IP `192.168.0.110:9000`으로 전송됩니다.
- `Components/signupdialog.*`는 저장소에 남아 있지만, 현재 메인 가입 흐름은 `LoginDialog` 내부 stacked page가 담당합니다.

---

## 4. 디렉터리 구조

```text
FE/
├─ main.cpp
├─ mainwindow.h
├─ mainwindow.cpp
├─ mainwindow_ui.cpp
├─ mainwindow_session.cpp
├─ CMakeLists.txt
├─ resources.qrc
├─ README.md
├─ assets/
│  └─ fonts/
├─ style/
│  ├─ theme_dark.qss
│  ├─ theme_light.qss
│  ├─ app_styles.qss
│  └─ icons/
├─ Components/
│  ├─ full_underbar.*
│  ├─ logindialog.*
│  ├─ osdwidget.*
│  ├─ settingswidget.*
│  ├─ sidebar.*
│  ├─ signupdialog.*
│  ├─ slammapwidget.*
│  ├─ topbar.*
│  └─ videocard.*
├─ Views/
│  ├─ fullscreenview.*
│  ├─ liveview.*
│  └─ playbackview.*
├─ Video/
│  ├─ frucvideoprocessor.*
│  ├─ livevideowidget.*
│  ├─ recordedvideowidget.*
│  ├─ rtsppinger.*
│  ├─ videowidget.*
│  └─ Gst/
│     ├─ GstQualityMonitor.*
│     └─ GstStatsCollector.hpp
├─ Network/
│  ├─ authmanager.*
│  ├─ cameracontrolclient.*
│  ├─ rosbridgeclient.*
│  └─ streammanager.*
└─ Utils/
   ├─ channelcatalog.*
   ├─ configmanager.h
   ├─ framelessconfirmdialog.*
   └─ goaloverlaycontroller.*
```

### 폴더별 한 줄 정리

| 폴더 | 역할 | 대표 파일 |
|---|---|---|
| 루트 | 앱 진입점과 메인 셸 | `main.cpp`, `mainwindow_*` |
| `Components/` | 재사용 UI 조각 | `videocard`, `topbar`, `sidebar` |
| `Views/` | 페이지 단위 화면 | `liveview`, `playbackview`, `fullscreenview` |
| `Video/` | 영상 파이프라인과 파일 후처리 | `videowidget`, `livevideowidget`, `frucvideoprocessor` |
| `Network/` | HTTP / WebSocket 통신 | `authmanager`, `rosbridgeclient`, `cameracontrolclient` |
| `Utils/` | 설정, 매핑, 공용 유틸 | `configmanager`, `channelcatalog`, `goaloverlaycontroller` |
| `style/` | QSS 테마 및 아이콘 | `theme_dark.qss`, `theme_light.qss` |
| `assets/` | 폰트 리소스 | Pretendard |

---

## 5. 화면별 역할

### 5.1 LoginDialog

- 운영자 로그인
- 회원가입 2단계 폼
- 비밀번호 재설정
- 서버 IP 입력
- Remember device
- 다크/라이트 테마 토글
- 프레임리스 드래그 이동

### 5.2 LiveView

- CCTV 4분할 그리드
- RC Car 카메라
- SLAM 맵
- 카드별 전체화면 / 녹화 / Goal 지정
- 채널 표시/숨김
- 지도 Goal 지정 및 순찰 포인트 추가

### 5.3 FullScreenView

- 라이브 / 녹화 재생 공용 전체화면 화면
- 줌 인/아웃, 패닝, 리셋
- Control Mode 토글
- 긴급 정지 버튼
- Goal 화살표 오버레이

### 5.4 PlaybackView

- 서버/로컬 녹화 목록 표시
- 채널 카테고리 필터
- 다운로드 진행률 표시
- 더블 클릭 재생 요청

### 5.5 SettingsWidget

- 카메라 섹션
  - Camera IP
  - RTSP Port
  - Use Secure RTSPS
  - Use Custom CCTV URL
  - Custom CCTV ID / Password
- 로봇 섹션
  - ROS2 Bridge Host
  - Enable Manual Control
  - Linear X / Angular Z
  - Auto Speed

---

## 6. 외부 통신 구조

### 6.1 로그인 서버

`AuthManager`가 담당합니다.

- 입력: 서버 IP 또는 URL
- 정규화 규칙:
  - 스킴이 없으면 `http://` 추가
  - 포트가 없으면 `8080` 사용
  - trailing slash 제거

현재 코드에서 사용하는 엔드포인트:

- `POST /login`
- `POST /users`
- `GET /users/{id}`
- `PUT /users/{id}`
- `POST /refresh`

주요 동작:

- 로그인 성공 시 `access_token`, `refresh_token`, 만료 시각을 저장
- 만료 임박 시 자동 refresh
- 사용자 이메일은 `GET /users/{id}`로 조회

### 6.2 카메라 제어 WebSocket

`CameraControlClient`가 담당합니다.

- 주소: `ws://<camera-ip>:9000`
- 전송 메시지:
  - `CALIBRATION_CLICK`
  - `RECORD_CONTROL`
  - `GET_RECORDINGS`
  - `DOWNLOAD_FILE`
- 수신 메시지:
  - `RECORD_FINISHED`
  - `RECORDING_LIST`
  - `SLAM_MAPPING_ERROR`
  - `FILE_TRANSFER_START`
  - `FILE_TRANSFER_COMPLETE`

다운로드 파일은 기본적으로 시스템 Downloads 폴더에 저장됩니다.

### 6.3 ROS2 rosbridge WebSocket

`RosBridgeClient`가 담당합니다.

- 주소: `ws://<robot-host>:9090`
- 전송 타입:
  - `cmd_vel`
  - `mode_control`
  - `nav_command`
- 수신 타입:
  - map
  - odom
  - path
  - nav_status
  - nav_feedback

### 6.4 CCTV 스트림 URL 규칙

`StreamManager`가 담당합니다.

기본 모드:

- Low: `rtsp://<ip>:<port>/ch1` ~ `ch4`
- High: `rtsp://<ip>:<port>/ch1_fhd` ~ `ch4_fhd`

RTSPS 모드:

- 스킴이 `rtsps://`로 바뀝니다.
- 포트는 설정값과 관계없이 `8322`를 사용합니다.

Custom CCTV 모드:

- 형식:
  - `rtsp://<id>:<password>@<ip>:<port>/<index>/H.264/media.smp`
  - 또는 RTSPS면 `rtsps://...`
- `<index>`는 0~3입니다.
- 사용자 입력 ID/비밀번호는 URL-safe percent encoding 후 삽입됩니다.

### 6.5 RC Car 스트림 URL

`ConfigManager`에서 계산합니다.

- 라이브: `rtsp://<robot-host>:9554/camera`
- ISP 전체화면: `rtsp://<robot-host>:9554/camera_isp`

### 6.6 인증된 RTSPS 처리

`LiveVideoWidget`는 RTSPS 스트림에서 인증 토큰을 적용합니다.

- 우선 `rtspsrc before-send` 훅으로 `Authorization` 헤더를 삽입합니다.
- 훅이 불가능한 경우 `?token=<access_token>` 쿼리 파라미터 fallback을 사용합니다.
- Unauthorized 계열 오류가 감지되면 토큰 refresh를 시도합니다.

---

## 7. 채널 / 카테고리 매핑

`ChannelCatalog` 기준 매핑은 다음과 같습니다.

### 7.1 라이브/전체화면 채널

| View Index | 화면 이름 | 라이브 녹화 채널 ID | 전체화면 녹화 채널 ID |
|---|---|---:|---:|
| 0 | Channel 01 - Camera | 1 | 5 |
| 1 | Channel 02 - Camera | 2 | 6 |
| 2 | Channel 03 - Camera | 3 | 7 |
| 3 | Channel 04 - Camera | 4 | 8 |
| 4 | RC Car - Front Cam | 9 | 9 |

### 7.2 Playback 카테고리

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

## 8. 녹화 / 재생 / FRUC 동작 흐름

### 8.1 녹화

1. Live 카드에서 Record 버튼 클릭
2. `CameraControlClient::sendRecordCommand()` 전송
3. 서버가 녹화를 종료하면 `RECORD_FINISHED` 전송
4. `MainWindow`가 녹화 목록 새로고침 요청

### 8.2 자동 다운로드

1. 녹화 종료 URL에서 파일명을 추출
2. 로컬 Downloads 폴더에 이미 있으면 그대로 사용
3. 없으면 `DOWNLOAD_FILE` 요청
4. 바이너리 청크를 받아 파일로 저장
5. 완료 후 Playback 목록에 반영

### 8.3 재생

1. Playback 항목 더블 클릭
2. 로컬 파일이 있으면 `RecordedVideoWidget`로 즉시 재생
3. 없으면 다운로드 요청
4. 다운로드 완료 후 목록에 추가

### 8.4 FRUC

이 기능은 OpenCV가 활성화된 빌드에서만 동작합니다.

원본 MP4 파일에 대해 자동으로 두 가지 파생 파일을 생성합니다.

- `*_FRUC_FAST.mp4`
- `*_FRUC_HQ.mp4`

동작 조건:

- 입력 파일이 MP4여야 함
- 이미 FRUC 파생 파일이 아니어야 함
- 동일 출력 파일이 이미 존재하면 재생성하지 않음

---

## 9. 설정 파일(`settings.ini`)

설정 파일 위치:

- `QCoreApplication::applicationDirPath()/settings.ini`

### 9.1 주요 키와 기본값

| 섹션/키 | 기본값 | 설명 |
|---|---|---|
| `Network/CameraIP` | `192.168.0.39` | CCTV 서버 IP |
| `Network/CameraPort` | `8554` | 기본 RTSP 포트 |
| `Network/UseCustomCCTV` | `false` | Custom CCTV URL 사용 여부 |
| `Network/CustomCCTVUsername` | `admin` | Custom CCTV ID |
| `Network/CustomCCTVPassword` | `5hanwha!` | Custom CCTV 비밀번호 |
| `Network/UseRtsps` | `false` | RTSPS 사용 여부 |
| `Network/RobotIP` | `192.168.0.237` | 로봇 호스트 또는 URL 입력값 |
| `Auth/LoginServerUrl` | `192.168.0.110` | 로그인 서버 주소 |
| `Auth/ActiveUserId` | 없음 | 현재 로그인 사용자 ID |
| `Auth/ActiveUserEmail` | 없음 | 현재 로그인 사용자 이메일 |
| `Auth/ActiveAuthMode` | 없음 | 현재 인증 모드 |
| `Auth/RememberUser` | `false` | Remember device 사용 여부 |
| `Auth/RememberedUserId` | 없음 | 저장된 사용자 ID |
| `UI/DarkTheme` | `true` | 다크 테마 여부 |
| `Control/ManualControl` | `false` | WASD 수동 제어 허용 여부 |
| `Control/LinearX` | `0.30` | 수동 선속도 |
| `Control/AngularZ` | `0.50` | 수동 각속도 |
| `Navigation/AutoSpeed` | `0.15` | 자율주행 속도 |

### 9.2 파생 규칙

- `RobotIP`는 호스트만 넣어도 됩니다.
- 내부에서 다음 주소가 자동 생성됩니다.
  - `ws://<robot-host>:9090`
  - `rtsp://<robot-host>:9554/camera`
  - `rtsp://<robot-host>:9554/camera_isp`

---

## 10. 빌드 및 실행

### 10.1 개발 환경 전제

현재 CMake 설정은 사실상 Windows 환경을 기준으로 작성되어 있습니다.

필수 요소:

- CMake 3.16+
- Qt Widgets / WebSockets / Network
- GStreamer 1.0
- OpenCV (선택, FRUC 기능용)
- MSVC 또는 MinGW

현재 `CMakeLists.txt` 기준 참고 경로:

- OpenCV는 자동 탐색하거나, 필요하면 `OPENCV_ROOT` 또는 `OpenCV_DIR`로 지정할 수 있습니다.
- GStreamer(MSVC) 후보:
  - `C:/Program Files/gstreamer/1.0/msvc_x86_64`
  - `C:/gstreamer/1.0/msvc_x86_64`
  - `D:/gstreamer/1.0/msvc_x86_64`
- GStreamer(MinGW):
  - `C:/Program Files/gstreamer/1.0/mingw_x86_64`

### 10.2 예시 빌드

Qt 경로는 로컬 설치 위치에 맞게 조정해야 합니다.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Debug
```

OpenCV까지 함께 쓰고 싶으면:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64" -DOPENCV_ROOT="C:/opencv/build"
cmake --build build --config Debug
```

다른 PC에서 OpenCV 없이 먼저 빌드만 하고 싶으면:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64" -DVEDA_ENABLE_FRUC=OFF
cmake --build build --config Debug
```

실행 파일 예시:

```text
build/Debug/VEDA_QT_1.exe
```

### 10.3 런타임 참고

`main.cpp`는 실행 시 다음을 처리합니다.

- OpenCV DLL 경로를 `PATH`에 추가
- GStreamer DLL / plugin 경로를 `PATH`, `GST_PLUGIN_PATH`, `GST_PLUGIN_SCANNER`에 반영
- 번들된 `gstreamer/` 폴더가 있으면 우선 사용
- OpenCV를 찾지 못한 빌드에서는 FRUC 기능만 비활성화되고 앱 자체는 계속 빌드됩니다.

---

## 11. 리소스와 스타일

### 11.1 리소스

`resources.qrc`에 포함된 항목:

- `env/server.crt`
- `style/icons/*.svg`
- `assets/fonts/Pretendard-*.otf`

### 11.2 스타일

- `style/theme_dark.qss`
- `style/theme_light.qss`
- 일부 위젯은 코드 내부 스타일도 함께 사용합니다.

---

## 12. 현재 구조에서 중요한 파일

처음 코드를 읽을 때는 아래 순서가 가장 빠릅니다.

1. `main.cpp`
2. `mainwindow.h`
3. `mainwindow_ui.cpp`
4. `mainwindow.cpp`
5. `mainwindow_session.cpp`
6. `Network/authmanager.*`
7. `Network/rosbridgeclient.*`
8. `Network/cameracontrolclient.*`
9. `Views/liveview.*`
10. `Views/fullscreenview.*`
11. `Views/playbackview.*`
12. `Components/settingswidget.*`

---

## 13. 알려진 구현 메모

- Calibration click 서버 IP는 현재 코드상 `192.168.0.110`으로 고정되어 있습니다.
- 회원가입 전용 `signupdialog.*`는 저장소에 남아 있지만, 실제 주 사용 흐름은 `LoginDialog` 내부 스택 페이지입니다.
- 일부 UI 스타일은 QSS와 코드 내 inline style이 혼합되어 있습니다.
- `CMakeLists.txt`는 모든 소스 파일을 한 번에 나열하는 구조라서, 추후 폴더 기반 모듈화 시 `target_sources` 또는 `add_subdirectory` 방식으로 정리할 여지가 있습니다.

---

## 14. 트러블슈팅

### 로그인 서버 연결 실패

확인할 점:

- 서버 IP가 맞는지
- `8080` 포트가 열려 있는지
- 로그인 서버가 JSON 응답을 반환하는지

### RTSPS가 재생되지 않음

확인할 점:

- `Use Secure RTSPS` 사용 시 실제 포트가 `8322`인지
- `env/server.crt` 또는 서버 인증서 정책이 맞는지
- 토큰이 필요한 경우 로그인 세션이 살아 있는지

### 커스텀 CCTV가 연결되지 않음

확인할 점:

- `Use Custom CCTV URL`이 켜져 있는지
- Custom CCTV ID / Password가 올바른지
- 장비가 `<index>/H.264/media.smp` 규칙을 쓰는지
- 특수문자가 포함된 계정 정보인지

### 다운로드한 파일이 재생되지 않음

확인할 점:

- Downloads 폴더에 실제 파일이 생성되었는지
- 파일 크기가 너무 작으면 앱이 손상 파일로 간주하고 재다운로드할 수 있는지
- 원본이 MP4인지

### OpenCV 또는 GStreamer를 찾지 못함

확인할 점:

- GStreamer 설치 경로가 현재 PC와 맞는지
- DLL 및 plugin 경로가 런타임에 올바르게 잡혔는지
- OpenCV가 없으면 FRUC만 꺼진 상태로 빌드되는지
- OpenCV까지 쓰려면 `-DOPENCV_ROOT=...` 또는 `OpenCV_DIR` 환경 변수를 지정했는지

---

## 15. 요약

이 프로젝트는 하나의 Qt 데스크톱 앱 안에서 다음 세 축을 통합합니다.

- 인증 및 세션 관리
- CCTV/RC 라이브 및 녹화 재생
- ROS 기반 로봇 제어

현재 코드는 이미 `MainWindow`, `Views`, `Network`, `Video`, `Utils`로 역할이 나뉘어 있어 읽기 시작하기 좋은 상태이며, 운영에 중요한 설정 값과 URL 규칙은 이 README의 내용을 기준으로 따라가면 됩니다.
