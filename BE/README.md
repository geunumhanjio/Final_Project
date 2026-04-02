# Backend (BE) - 서버 인프라

VEDA 재난 대응 시스템의 하이브리드 백엔드 아키텍처.  
**C++ 미디어 처리 엔진**과 **Go 인증 서비스** 두 개의 독립 서버로 구성된다.

---

## 아키텍처 개요

```
┌─────────────────────────────────────────────────────┐
│              Backend (BE) Server                    │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │         C++ Proxy Server (ProxyServer)        │   │
│  │                                              │   │
│  │  ┌───────────┐   ┌───────────┐   ┌────────┐ │   │
│  │  │ RtspProxy │   │ VmsServer │   │RosClient│ │   │
│  │  │ :8554     │   │  :9000   │   │ :9090  │ │   │
│  │  │ :8322(TLS)│   │(WebSocket)│   │(WS출력)│ │   │
│  │  └─────┬─────┘   └─────┬─────┘   └────┬───┘ │   │
│  │        │               │              │     │   │
│  │        └───────────────┴──────────────┘     │   │
│  │              CalibrationManager (OpenCV)      │   │
│  └──────────────────────────────────────────────┘   │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │         Go Auth Server (login)               │   │
│  │              :8080 (REST API)                │   │
│  │   JWT Auth ─── Gin Router ─── MySQL          │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

---

## 서버 - 클라이언트 스트리밍 플로우

<img width="1477" height="807" alt="Image" src="https://github.com/user-attachments/assets/c28cf30b-0389-4348-9221-8de0128698a1" />

## 클라 - 서버 - ROS2 플로우

<img width="1580" height="807" alt="Image" src="https://github.com/user-attachments/assets/f5a43092-796c-479a-a1cf-dad2ceaf299e" />

## Media Server 내부 구조 (파이프라인)

<img width="1280" height="1572" alt="Image" src="https://github.com/user-attachments/assets/cf5f3edd-2ae7-4140-8688-a24822c1e5d5" />

---

## 디렉토리 구조

```
BE/
├── CMakeLists.txt              # C++ 빌드 설정 (CMake 3.10+)
├── src/
│   ├── main.cpp                # 진입점 - 컴포넌트 초기화 및 바인딩
│   ├── media/
│   │   ├── RtspProxy.cpp       # GStreamer RTSP 프록시 서버
│   │   ├── CctvScanner.cpp     # CCTV 채널 스캔
│   │   └── GstQualityMonitor.cpp # GStreamer 품질 모니터링 플러그인
│   ├── calibration/
│   │   └── CalibrationManager.cpp # 좌표계 변환 (호모그래피)
│   └── network/
│       ├── VmsServer.cpp       # WebSocket 서버 (FE ↔ BE)
│       └── RosClient.cpp       # WebSocket 클라이언트 (BE → ROS2)
├── include/                    # 헤더 파일 (src/ 미러링)
└── login/                      # Go 인증 서버
    ├── cmd/server/main.go      # 진입점 - 서버 부트스트랩
    ├── go.mod
    └── internal/
        ├── api/handler.go      # HTTP 핸들러 (REST 엔드포인트)
        ├── auth/service.go     # JWT 발급/검증 로직
        ├── config/config.go    # 환경 변수 기반 설정
        ├── db/mysql.go         # DB 연결 및 마이그레이션
        ├── store/              # MySQL 데이터 접근 계층
        └── user/service.go     # 사용자 비즈니스 로직
```

---

## 컴포넌트 상세

### 1. RtspProxy (C++)

Hanwha Vision CCTV의 RTSP 스트림을 수신하고, Frontend 클라이언트에 재배포하는 GStreamer 기반 프록시 서버.

| 포트 | 프로토콜 | 역할 |
|------|----------|------|
| `8554` | RTSP | FE 클라이언트에 스트림 제공 |
| `8322` | RTSPS (TLS) | JWT 인증 + TLS 암호화 스트리밍 |

**주요 기능**
- 다채널 RTSP 수신 및 재배포 (GStreamer 파이프라인)
- 채널별 녹화 시작/중지 및 파일 저장
- 실시간 스트림 품질 통계 수집 (FPS, 비트레이트, 프록시 지연)
- RTSPS 접속 시 JWT 토큰 검증

---

### 2. VmsServer (C++)

Frontend 클라이언트와 통신하는 WebSocket 서버 (Boost.Beast 기반).

| 포트 | 프로토콜 | 역할 |
|------|----------|------|
| `9000` | WebSocket | FE ↔ BE 양방향 통신 |

**수신 메시지 타입**

| 타입 | 설명 |
|------|------|
| `COORDINATE_REPORT` | CCTV에서 감지된 위험 좌표 수신 |
| `ROBOT_CONTROL` | 수동 로봇 제어 명령 |
| `start` / `stop` | 채널 녹화 제어 |

**송신 메시지 타입**

| 타입 | 설명 |
|------|------|
| 채널 통계 | FPS, 비트레이트, 지연시간 브로드캐스트 |
| 녹화 파일 | 녹화 완료 후 요청한 세션에 파일 전송 |

---

### 3. RosClient (C++)

BE에서 ROS2 시스템으로 목표 좌표를 전달하는 WebSocket 클라이언트 (Boost.Beast 기반).

- 연결 대상: `192.168.0.237:9090` (rosbridge_server)
- 캘리브레이션 완료된 좌표를 ROS2 토픽으로 발행

**데이터 흐름**

```
VmsServer (수신) → CalibrationManager (좌표 변환) → RosClient (ROS2 전송)
```

---

### 4. CalibrationManager (C++)

CCTV 픽셀 좌표를 로봇 맵 좌표로 변환하는 캘리브레이션 모듈 (OpenCV 호모그래피).

```cpp
cv::Point2f apply(const cv::Point2f& inputPoint);
// CCTV 이미지 좌표(px) → 로봇 맵 좌표(m)
```

---

### 5. Go Auth Server (login)

JWT 기반 인증 및 사용자 관리를 담당하는 독립 REST API 서버 (Gin + MySQL).

| 포트 | 프로토콜 | 역할 |
|------|----------|------|
| `8080` | HTTP/REST | 인증 및 사용자 CRUD API |

**토큰 정책**

| 토큰 종류 | 기본 TTL | 환경변수 |
|-----------|----------|---------|
| Access Token | 1시간 | `ACCESS_TOKEN_TTL` |
| Refresh Token | 14일 (336h) | `REFRESH_TOKEN_TTL` |

---

## API 레퍼런스

### Go Auth Server (`:8080`)

#### 헬스 체크
```
GET /healthz
→ 200 OK  { "status": "ok" }
```

#### 로그인
```
POST /login
Content-Type: application/json

{ "id": "admin", "password": "admin123" }

→ 200 OK
{
  "access_token":       "eyJ...",
  "refresh_token":      "eyJ...",
  "token_type":         "Bearer",
  "access_expires_at":  "2026-04-02T11:00:00Z",
  "refresh_expires_at": "2026-04-16T10:00:00Z"
}
```

#### 토큰 갱신
```
POST /refresh
{ "refresh_token": "eyJ..." }

→ 200 OK  { "access_token": "eyJ...", ... }
```

#### 사용자 관리
```
GET    /users           → 사용자 목록 조회
GET    /users/:id       → 단일 사용자 조회
POST   /users           → 사용자 생성  (id, email, password 필수)
PUT    /users/:id       → 사용자 정보 수정
DELETE /users/:id       → 사용자 삭제
```

**에러 응답 형식**
```json
{ "error": "error_code", "message": "상세 설명" }
```

| HTTP 코드 | 에러 코드 | 설명 |
|-----------|-----------|------|
| 400 | `invalid_request` | 요청 바디 파싱 실패 |
| 401 | `invalid_credentials` | ID 또는 비밀번호 불일치 |
| 404 | `user_not_found` | 존재하지 않는 사용자 |
| 409 | `user_already_exists` | ID 또는 이메일 중복 |
| 500 | `internal_error` | 서버 내부 오류 |

---

### VmsServer WebSocket (`:9000`)

#### 좌표 보고 (FE → BE)
```json
{
  "type": "COORDINATE_REPORT",
  "payload": {
    "x": 150.5,
    "y": 200.0,
    "confidence": 0.95,
    "camera_id": 1
  }
}
```

#### 녹화 제어 (FE → BE)
```json
{
  "type": "COMMAND",
  "session_id": 1,
  "cmd": "start",
  "channel": 2
}
```

#### 채널 통계 (BE → FE, 브로드캐스트)
```json
{
  "channel_id": 1,
  "fps": 29.97,
  "bitrate_kbps": 4096.0,
  "proxy_latency_ms": 12.5
}
```

---

## 환경 변수

### Go Auth Server

| 변수 | 기본값 | 설명 |
|------|--------|------|
| `JWT_SECRET` | (필수) | JWT 서명 비밀키 |
| `JWT_SECRET_FILE` | - | 비밀키 파일 경로 (우선 적용) |
| `LOGIN_SERVER_ADDR` | `:8080` | 서버 바인딩 주소 |
| `ACCESS_TOKEN_TTL` | `1h` | Access 토큰 유효 기간 |
| `REFRESH_TOKEN_TTL` | `336h` | Refresh 토큰 유효 기간 |
| `MYSQL_HOST` | `127.0.0.1` | MySQL 호스트 |
| `MYSQL_PORT` | `3306` | MySQL 포트 |
| `MYSQL_USER` | `rokgeun` | MySQL 사용자 |
| `MYSQL_PASSWORD` | - | MySQL 비밀번호 |
| `MYSQL_DATABASE` | `login_server` | 데이터베이스명 |

### C++ Proxy Server

ROS Bridge 연결 주소는 `src/main.cpp`에서 직접 설정:
```cpp
rosClient->connect("192.168.0.237", "9090");  // ROS2 브리지 IP
```

---

## 빌드

### 사전 요구사항

```bash
# Ubuntu 22.04 / 20.04
sudo apt install \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-rtsp \
  libgstrtspserver-1.0-dev \
  libopencv-dev \
  libboost-all-dev \
  libssl-dev \
  cmake build-essential

# jwt-cpp (vcpkg 또는 수동 설치)
# /usr/local/include/jwt-cpp/jwt.h 경로 확인
```

Go 1.22 이상 필요.

### C++ Proxy Server 빌드

```bash
cd BE/
mkdir build && cd build
cmake ..
make -j$(nproc)

# 실행 파일: build/ProxyServer
```

### Go Auth Server 빌드

```bash
cd BE/login/
go mod download
go build -o login-server ./cmd/server

# 실행 파일: login/login-server
```

---

## 실행

### 1. MySQL 준비

```bash
sudo mysql -u root -p <<EOF
CREATE DATABASE login_server CHARACTER SET utf8mb4;
CREATE USER 'rokgeun'@'localhost' IDENTIFIED BY 'your_password';
GRANT ALL PRIVILEGES ON login_server.* TO 'rokgeun'@'localhost';
FLUSH PRIVILEGES;
EOF
```

### 2. Go Auth Server 실행

```bash
export JWT_SECRET="your-secret-key"
export MYSQL_HOST="localhost"
export MYSQL_PASSWORD="your_password"

cd BE/login/
./login-server
# → Listening on :8080
```

### 3. C++ Proxy Server 실행

```bash
cd BE/build/
./ProxyServer
# → RTSP  :8554
# → RTSPS :8322
# → VMS   :9000
# → ROS Bridge 연결 시도 (192.168.0.237:9090)
```

### 포트 확인

```bash
netstat -tlnp | grep -E "(8080|8322|8554|9000)"
```

---

## 기술 스택

| 서버 | 언어 | 주요 라이브러리 |
|------|------|----------------|
| Proxy Server | C++17 | GStreamer, Boost.Beast, OpenCV, OpenSSL, jwt-cpp |
| Auth Server | Go 1.22+ | Gin, go-jwt, go-sql-driver/mysql |

---

## 문제 해결

#### RTSP 스트림 연결 실패
```bash
# GStreamer 플러그인 확인
gst-inspect-1.0 rtspsrc
gst-inspect-1.0 rtsph264pay

# 파이프라인 직접 테스트
GST_DEBUG=3 ./ProxyServer
```

#### ROS Bridge 연결 실패
```bash
# ROS2 노드에서 rosbridge 실행 확인
ros2 run rosbridge_server rosbridge_websocket
# 기본 포트: 9090
```

#### Go 서버 DB 연결 실패
```bash
# MySQL 서비스 상태 확인
sudo systemctl status mysql

# 환경 변수 확인
echo $JWT_SECRET $MYSQL_HOST $MYSQL_PASSWORD
```
