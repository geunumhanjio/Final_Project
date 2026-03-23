# VEDA_QT_1 FE

Qt 6/C++ 기반의 CCTV 통합 관제 및 로봇 제어 프런트엔드입니다.

이 애플리케이션은 다음 기능을 하나의 데스크톱 앱으로 제공합니다.

- CCTV 4채널 + 로봇 카메라 실시간 모니터링
- RTSP/RTSPS 영상 재생
- 녹화 시작/정지, 녹화 목록 조회, 다운로드, 로컬 재생
- ROS2 rosbridge 기반 원격 로봇 제어
- Goal Pose 지정 및 상태 추적
- 로그인/회원가입/세션 유지
- 스트림 품질 OSD 표시
- 테마 전환 및 실행 설정 저장

## 1. 프로젝트 개요

앱 실행 시 먼저 로그인 화면이 열리고, 인증이 완료되면 메인 관제 화면이 표시됩니다.

이후 사용자는 다음과 같은 흐름으로 작업합니다.

1. Live View에서 CCTV와 로봇 카메라를 모니터링합니다.
2. 필요한 채널을 전체화면으로 확대합니다.
3. 녹화, 다운로드, 재생을 수행합니다.
4. Control Mode에서 목표 지점 또는 수동 주행 명령을 보냅니다.
5. Settings에서 카메라/로봇/테마/제어 속도 설정을 수정합니다.

## 2. 핵심 기능

### 2.1 인증과 세션

- 로그인
- 회원가입
- 사용자 프로필 조회
- Access token / Refresh token 기반 세션 유지
- 세션 만료 시 재로그인 유도

관련 코드:

- `Components/logindialog.*`
- `Components/signupdialog.*`
- `Network/authmanager.*`
- `mainwindow_session.cpp`

### 2.2 실시간 관제

- CCTV 4채널과 로봇 카메라 1채널 표시
- 채널 카드별 녹화 버튼 제공
- 더블클릭으로 전체화면 전환
- 스트림 장애 시 재연결 시도

관련 코드:

- `Views/liveview.*`
- `Components/videocard.*`
- `Video/livevideowidget.*`
- `Network/streammanager.*`

### 2.3 전체화면 보기와 제어

- 선택 채널 전체화면 표시
- 휠 줌
- 드래그 팬
- 사각형 줌
- 라이브 채널에서 Control Mode 활성화
- 목표 지점/방향 오버레이 표시

관련 코드:

- `Views/fullscreenview.*`
- `Components/full_underbar.*`
- `Utils/goaloverlaycontroller.*`

### 2.4 로봇 제어

- WASD 기반 수동 주행
- `cmd_vel` 주기 전송
- Goal Pose 전송
- Manual / Auto / Control / Patrol 모드 전환
- 긴급 정지
- 목표 도착 상태 추적

관련 코드:

- `mainwindow.cpp`
- `Network/rosbridgeclient.*`
- `Components/slammapwidget.*`
- `Views/fullscreenview.*`

### 2.5 녹화 목록, 다운로드, 재생, FRUC

- 카메라 서버 녹화 목록 조회
- 다운로드 진행률 표시
- 다운로드 파일 로컬 재생
- 카테고리별 필터링
- 다운로드된 원본 파일에 대해 FRUC FAST / FRUC HQ 후처리 자동 수행
- 생성된 FRUC 파일을 재생 목록에 자동 추가

FRUC 결과 파일 접미사:

- `_FRUC_FAST`
- `_FRUC_HQ`

관련 코드:

- `Views/playbackview.*`
- `Network/cameracontrolclient.*`
- `Video/recordedvideowidget.*`
- `Video/frucvideoprocessor.*`

### 2.6 OSD와 스트림 품질 지표

표시 가능한 지표:

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
- `Network/cameracontrolclient.*`

### 2.7 설정과 테마

설정 화면에서 다음 항목을 관리할 수 있습니다.

- 카메라 IP / 포트
- Custom CCTV 사용 여부
- Custom CCTV 아이디
- Custom CCTV 비밀번호
- RTSPS 사용 여부
- 로봇 host
- 수동 제어 허용 여부
- 수동 선속도 / 각속도
- 자동 주행 속도
- 다크/라이트 테마

설정은 `settings.ini`에 저장되며 앱 재실행 후에도 유지됩니다.

관련 코드:

- `Components/settingswidget.*`
- `Utils/configmanager.h`

## 3. 현재 아키텍처

### 3.1 MainWindow 분리 구조

현재 `MainWindow` 관련 코드는 다음처럼 나뉘어 있습니다.

- `mainwindow.cpp`
  - 로봇 제어 입력 처리
  - Goal tracking
  - 공유 비디오 오버레이 상태 관리
  - 제어 세션 상태 전환
- `mainwindow_ui.cpp`
  - 위젯 생성
  - 페이지 전환
  - 시그널/슬롯 연결
  - 녹화/다운로드/재생 연결
- `mainwindow_session.cpp`
  - 테마 전환
  - 로그아웃
  - 세션 재진입
  - FRUC 시작

### 3.2 공용 유틸 모듈

- `Utils/channelcatalog.*`
  - 채널명, 녹화 카테고리, 사이드바 제목 매핑
- `Utils/goaloverlaycontroller.*`
  - 목표 오버레이 위젯
  - 좌표 변환
  - 드래그 커밋 계산
  - 오버레이 재투영
- `Utils/framelessconfirmdialog.*`
  - 공통 확인 다이얼로그

### 3.3 계층별 역할

- `Views`
  - 페이지 단위 화면
- `Components`
  - 재사용 UI 위젯
- `Video`
  - GStreamer / OpenCV 기반 영상 처리
- `Network`
  - 인증, 카메라 제어, rosbridge, 스트림 URL 생성
- `Utils`
  - 설정, 매핑, 공용 UI/오버레이 로직

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
├─ style/
│  ├─ theme_dark.qss
│  └─ theme_light.qss
├─ Views/
│  ├─ liveview.*
│  ├─ fullscreenview.*
│  └─ playbackview.*
├─ Components/
│  ├─ topbar.*
│  ├─ sidebar.*
│  ├─ videocard.*
│  ├─ full_underbar.*
│  ├─ slammapwidget.*
│  ├─ osdwidget.*
│  ├─ settingswidget.*
│  ├─ logindialog.*
│  └─ signupdialog.*
├─ Video/
│  ├─ videowidget.*
│  ├─ livevideowidget.*
│  ├─ recordedvideowidget.*
│  ├─ frucvideoprocessor.*
│  ├─ rtsppinger.*
│  └─ Gst/
│     ├─ GstQualityMonitor.*
│     └─ GstStatsCollector.hpp
├─ Network/
│  ├─ authmanager.*
│  ├─ cameracontrolclient.*
│  ├─ rosbridgeclient.*
│  └─ streammanager.*
└─ Utils/
   ├─ configmanager.h
   ├─ channelcatalog.*
   ├─ goaloverlaycontroller.*
   └─ framelessconfirmdialog.*
```

## 5. 런타임 통신 구조

### 5.1 인증 서버

- 담당: `AuthManager`
- 역할:
  - 로그인
  - 회원가입
  - 토큰 갱신
  - 사용자 프로필 조회

### 5.2 카메라 제어 서버

- 담당: `CameraControlClient`
- 기본 주소: `ws://<camera-ip>:9000`
- 역할:
  - 녹화 시작/정지
  - 녹화 목록 요청
  - 파일 다운로드
  - 스트림 통계 수신

### 5.3 ROS2 rosbridge

- 담당: `RosBridgeClient`
- 기본 주소: `ws://<robot-host>:9090`
- 역할:
  - `cmd_vel`
  - Goal Pose
  - 모드 제어
  - 내비게이션 취소
  - odom / path / nav status / nav feedback 수신

### 5.4 RTSP/RTSPS 스트림

- 담당: `StreamManager`, `VideoWidget`, `LiveVideoWidget`
- 역할:
  - 설정 기반 스트림 URL 생성
  - 저화질/고화질 URL 분리
  - GStreamer 파이프라인 재생

### 5.5 앱 시작 시 초기화

`main.cpp`에서 다음 작업을 수행합니다.

- OpenCV DLL 경로 PATH 반영
- GStreamer DLL 및 plugin 경로 설정
- 내장 SSL 인증서 등록
- 내장 폰트 로드
- GStreamer 초기화
- 기본 설정 로드
- 로그인 다이얼로그 표시

## 6. 스트림 URL 규칙

`StreamManager`는 설정값에 따라 CCTV URL을 생성합니다.

### 6.1 기본 모드

`UseCustomCCTV = false`일 때:

- 저화질: `rtsp://<ip>:<port>/ch1`
- 고화질: `rtsp://<ip>:<port>/ch1_fhd`

### 6.2 Custom CCTV 모드

`UseCustomCCTV = true`일 때:

- `rtsp://<id>:<password>@<ip>:<port>/<index>/H.264/media.smp`

설정 화면에서 `Use Custom CCTV URL`을 켜면 다음 입력칸이 표시됩니다.

- `Custom CCTV ID`
- `Custom CCTV Password`

기본값:

- ID: `admin`
- Password: `5hanwha!`

비밀번호나 아이디에 특수문자가 포함되더라도 내부적으로 percent-encoding 처리한 뒤 URL을 생성합니다.

### 6.3 RTSPS 모드

`UseRtsps = true`일 때:

- 스킴이 `rtsps://`로 변경됩니다.
- 포트가 `8322`로 강제됩니다.
- 앱 시작 시 리소스에 포함된 인증서를 기본 SSL 신뢰 구성에 등록합니다.

## 7. 환경 요구사항

권장 개발 환경:

- OS: Windows 10 / 11 64-bit
- Compiler: MSVC 64-bit 또는 MinGW 64-bit
- Qt: Qt 6.x
- Qt 모듈: `Widgets`, `WebSockets`, `Network`
- CMake: 3.16 이상
- GStreamer: Runtime + Development 패키지
- OpenCV: `opencv_world` 포함 빌드

현재 `CMakeLists.txt`는 Windows 절대경로 기준으로 GStreamer와 OpenCV를 찾습니다.
환경에 따라 `GST_ROOT`, `OPENCV_ROOT`를 직접 수정해야 할 수 있습니다.

기본 경로 예시:

- GStreamer MSVC
  - `C:/Program Files/gstreamer/1.0/msvc_x86_64`
  - `C:/gstreamer/1.0/msvc_x86_64`
  - `D:/gstreamer/1.0/msvc_x86_64`
- GStreamer MinGW
  - `C:/Program Files/gstreamer/1.0/mingw_x86_64`
- OpenCV
  - `C:/opencv/build`

## 8. 빌드 방법

### 8.1 Qt Creator

1. `FE/CMakeLists.txt`를 Qt Creator로 엽니다.
2. 사용할 Kit을 선택합니다.
3. GStreamer / OpenCV 경로를 확인합니다.
4. Configure 후 Build를 실행합니다.

### 8.2 CMake CLI 예시

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

빌드 결과:

```text
build/Debug/VEDA_QT_1.exe
```

## 9. 실행 및 첫 설정

1. 앱을 실행합니다.
2. 로그인 화면에서 서버 주소와 계정을 입력합니다.
3. 로그인 후 메인 화면이 열립니다.
4. 필요하면 Settings에서 다음 값을 수정합니다.
   - 카메라 IP / 포트
   - Custom CCTV 사용 여부
   - Custom CCTV ID / Password
   - RTSPS 사용 여부
   - 로봇 host
   - 제어 속도
5. Live View 또는 Playback 화면으로 이동합니다.

앱 최초 실행 시 실행 파일 폴더에 `settings.ini`가 생성됩니다.

## 10. settings.ini 주요 항목

| 키 | 설명 | 기본값 |
|---|---|---|
| `Network/CameraIP` | CCTV 서버 IP | `192.168.0.39` |
| `Network/CameraPort` | CCTV 서버 RTSP 포트 | `8554` |
| `Network/CustomCCTVUsername` | Custom CCTV 계정 ID | `admin` |
| `Network/CustomCCTVPassword` | Custom CCTV 계정 비밀번호 | `5hanwha!` |
| `Network/UseCustomCCTV` | Custom CCTV URL 규칙 사용 | `false` |
| `Network/UseRtsps` | RTSPS 사용 | `false` |
| `Network/RobotIP` | 로봇 host 입력값 | `192.168.0.237` |
| `Auth/LoginServerUrl` | 로그인 서버 주소 | `192.168.0.110` |
| `Auth/RememberUser` | 사용자 기억하기 | `false` |
| `Control/ManualControl` | 수동 제어 허용 | `false` |
| `Control/LinearX` | 수동 선속도 | `0.30` |
| `Control/AngularZ` | 수동 각속도 | `0.50` |
| `Navigation/AutoSpeed` | 자동 주행 속도 | `0.15` |
| `UI/DarkTheme` | 다크 테마 사용 | `true` |

참고:

- `Network/RobotIP`는 내부적으로 host를 추출해 `ws://<host>:9090`와 `rtsp://<host>:9554/...` 조합에 사용됩니다.
- 설정 변경 후 일부 연결은 즉시 재연결됩니다.

## 11. 사용자 흐름 요약

### 11.1 Live View

- CCTV 카드 확인
- 더블클릭으로 전체화면 이동
- 채널별 녹화 버튼 사용
- OSD 지표 확인

### 11.2 Full Screen

- 확대된 영상 확인
- 휠 줌 / 드래그 팬 / 사각형 줌
- 라이브 채널에서 Control Mode 진입
- 드래그로 목표 지점과 방향 지정

### 11.3 Robot Control

- `W`, `A`, `S`, `D`로 수동 이동
- Sidebar 또는 Quick Panel에서 모드 전환
- Emergency Stop으로 이동 취소
- 목표 지점 도착 추적

### 11.4 Playback

- 녹화 목록 새로고침
- 카테고리 필터링
- 더블클릭 재생
- 다운로드 진행률 확인
- FRUC 결과 자동 등록

## 12. 개발 시 참고 포인트

### 12.1 목표 오버레이

비디오 목표 오버레이는 `Utils/goaloverlaycontroller.*`로 공통화되어 있습니다.

현재 사용 위치:

- `Components/videocard.cpp`
- `Views/fullscreenview.cpp`

### 12.2 채널 매핑

채널명, 카테고리, 녹화 채널 ID 매핑은 `Utils/channelcatalog.*`에 모여 있습니다.

### 12.3 설정 반영

대부분의 설정은 `ConfigManager`를 통해 저장되고 `configChanged()` 시그널로 전파됩니다.

## 13. 트러블슈팅

### 13.1 GStreamer를 찾지 못하는 경우

- `CMakeLists.txt`의 `GST_ROOT`를 현재 환경에 맞게 수정합니다.
- Runtime뿐 아니라 Development 패키지도 설치되어 있어야 합니다.

### 13.2 OpenCV를 찾지 못하는 경우

- `OPENCV_ROOT` 기본값은 `C:/opencv/build`입니다.
- 설치 위치가 다르면 `CMakeLists.txt`를 수정해야 합니다.

### 13.3 영상이 보이지 않는 경우

- `settings.ini`의 카메라 IP/포트를 확인합니다.
- `UseCustomCCTV`, `UseRtsps` 조합이 장비와 맞는지 확인합니다.
- Custom CCTV 모드라면 ID/비밀번호가 맞는지 확인합니다.

### 13.4 로봇 제어가 동작하지 않는 경우

- 로봇 host 설정이 올바른지 확인합니다.
- rosbridge가 `9090` 포트에서 열려 있는지 확인합니다.
- Control Mode는 라이브 CCTV 채널에서만 활성화됩니다.

### 13.5 로그인 후 세션이 자주 끊기는 경우

- 인증 서버의 토큰 만료 정책을 확인합니다.
- `AuthManager`의 refresh 흐름과 서버 응답 포맷이 일치하는지 확인합니다.

## 14. 주의사항

- 현재 프로젝트는 Windows 중심으로 작성되어 있습니다.
- 외부 서비스 프로토콜은 프로젝트 전용 백엔드 규약을 전제로 합니다.
- 운영 환경에 따라 URL 규칙, 계정, 포트는 분리 설정이 필요할 수 있습니다.
