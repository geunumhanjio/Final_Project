# navigation_manager

자율 주행 미션 컨트롤러. Nav2 `NavigateToPose` 액션을 wrapping하여
WebSocket bridge 또는 터미널에서 전달되는 명령을 처리한다.

---

## 패키지 구조

```
navigation_manager/
├── navigation_manager/
│   └── navigation_manager_node.py   ← 메인 노드
├── config/
│   └── waypoints.yaml               ← 웨이포인트 및 순찰 경로 정의
└── launch/
    └── navigation_manager.launch.py
```

---

## 설치

```bash
cd ~/ros2_ws
colcon build --packages-select navigation_manager
source install/setup.bash
```

---

## 사용법

### wsl_bringup을 통한 실행 (권장)

`nav_mode:=localization` 시 자동으로 함께 실행된다.

```bash
ros2 launch wsl_bringup wsl_bringup.launch.py nav_mode:=localization use_rviz:=true
```

### 단독 실행

```bash
ros2 launch navigation_manager navigation_manager.launch.py
```

---

## 명령 토픽 (`/nav/command`)

`std_msgs/String` 타입, JSON 형식으로 발행한다.

### 특정 좌표로 이동
```bash
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"goto\", \"x\": 1.0, \"y\": 0.5, \"yaw\": 0.0}"}'
```

### 웨이포인트로 이동
```bash
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"goto_wp\", \"name\": \"point_a\"}"}'
```

### 순찰 시작
```bash
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"patrol\", \"route\": \"default\"}"}'
```

### 취소
```bash
ros2 topic pub --once /nav/command std_msgs/String \
  '{"data": "{\"cmd\": \"cancel\"}"}'
```

---

## 토픽

### 퍼블리시

| 토픽 | 타입 | 주기 | 설명 |
|------|------|------|------|
| `/nav/status` | `std_msgs/String` (JSON) | 1 Hz | 현재 상태 (IDLE / NAVIGATING / RECOVERING / FAILED) |
| `/nav/feedback` | `std_msgs/String` (JSON) | Nav2 feedback 주기 | 남은 거리, ETA, 순찰 진행도 |

### 서브스크라이브

| 토픽 | 타입 | 설명 |
|------|------|------|
| `/nav/command` | `std_msgs/String` (JSON) | 네비게이션 명령 |

### `/nav/status` 예시
```json
{
  "state": "NAVIGATING",
  "target": "point_a",
  "dist_remaining": 1.23,
  "patrol_active": true,
  "patrol_index": 0,
  "patrol_total": 4
}
```

### `/nav/feedback` 예시
```json
{
  "dist_remaining": 1.23,
  "eta_sec": 8.1,
  "current_wp": 1,
  "total_wp": 4
}
```

---

## 웨이포인트 설정 (`config/waypoints.yaml`)

```yaml
waypoints:
  home:
    x: 0.0
    y: 0.0
    yaw: 0.0
  point_a:
    x: 1.0
    y: 0.0
    yaw: 0.0

patrol_routes:
  default:
    - point_a
    - point_b
    - home
```

좌표는 SLAM으로 생성한 맵 기준 `map` 프레임 좌표다.
RViz에서 원하는 위치를 확인한 뒤 입력한다.
변경 후 `colcon build` 없이 노드 재시작만 하면 반영된다.

---

## 파라미터

| 파라미터 | 기본값 | 설명 |
|----------|--------|------|
| `waypoints_file` | `config/waypoints.yaml` | 웨이포인트 파일 경로 |
| `max_retries` | `2` | 목표 실패 시 최대 재시도 횟수 |
| `retry_delay_sec` | `2.0` | 재시도 전 대기 시간 (초) |
| `linear_speed` | `0.12` | ETA 추정용 평균 선속도 (m/s) |

---

## 상태 머신

```
IDLE → (명령 수신) → NAVIGATING → (도착) → IDLE
                               → (실패) → RECOVERING → (재시도) → NAVIGATING
                                                      → (횟수 초과) → FAILED
NAVIGATING → (cancel 명령) → IDLE
```
