# STM32 - Raspberry Pi UART 통신 프로토콜

## 통신 설정

| 항목 | 값 |
|------|-----|
| UART | USART1 |
| Baud Rate | 115200 bps |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| 바이트 순서 | Little-Endian |

### 핀 연결

| Raspberry Pi | STM32 (NUCLEO-F401RE) |
|---|---|
| GPIO 14 (TX, Phy 8) | PA10 (USART1_RX) |
| GPIO 15 (RX, Phy 10) | PA9 (USART1_TX) |
| GND | GND |

> TX↔RX 교차 연결. GND는 반드시 공유해야 합니다.

---

## 패킷 구조

```
[HEADER1][HEADER2][CMD][LEN][DATA...][CHECKSUM]
  0xFF     0xFE    1B   1B   0~8B      1B
```

| 필드 | 크기 | 설명 |
|------|------|------|
| HEADER | 2 byte | `0xFF 0xFE` (패킷 시작 신호) |
| CMD | 1 byte | 명령 종류 |
| LEN | 1 byte | DATA 필드 길이 (0~8) |
| DATA | 0~8 byte | 명령에 따른 데이터 |
| CHECKSUM | 1 byte | `CMD ^ LEN ^ DATA[0] ^ DATA[1] ^ ...` (XOR) |

- 최소 패킷 크기: 5 byte (데이터 없는 경우)
- 최대 패킷 크기: 13 byte

---

## 명령 정의: CMD 필드

### RPi → STM32 (명령) : `0x01` ~ `0x7F`

| CMD | 이름 | LEN | DATA | 설명 |
|-----|------|-----|------|------|
| `0x01` | CMD_VELOCITY | 4 | `[vL_low, vL_high, vR_low, vR_high]` | 좌/우 바퀴 속도 (mm/s) |
| `0x02` | CMD_STOP | 0 | 없음 | 부드러운 정지 (Coast) |
| `0x03` | CMD_ESTOP | 0 | 없음 | 비상 정지 (Brake) |
| `0x04` | CMD_RELEASE | 0 | 없음 | 비상 정지 해제 |

**속도 범위**: -600 ~ +600 (int16_t, mm/s 단위)
- 양수: 전진
- 음수: 후진
- 500ms 이상 명령이 없으면 자동 정지 (watchdog)

> **⚠️ [미확정] 최대 속도와 가속도 제한, 전송 주기는 실험을 통한 실측 후 변경될 가능성이 있습니다.**

### STM32 → RPi (응답/센서) : `0x80` ~ `0xFF`

| CMD | 이름 | LEN | DATA | 설명 |
|-----|------|-----|------|------|
| `0x81` | RSP_ODOM | 8 | `[encL(4byte), encR(4byte)]` | 엔코더 카운트 (int32_t) |
| `0x82` | RSP_IMU | 6 | `[gx(2byte), gy(2byte), gz(2byte)]` | 자이로 각속도 (int16_t) |

> 센서 응답은 추후 구현 예정

---

## 통신 예시

### 예시 1: 속도 명령 (왼쪽 300 mm/s, 오른쪽 -200 mm/s)

```
300  = 0x012C → Little-Endian: [0x2C, 0x01]
-200 = 0xFF38 → Little-Endian: [0x38, 0xFF]

패킷: FF FE 01 04 2C 01 38 FF EB
                                └─ checksum: 01^04^2C^01^38^FF = EB
```

### 예시 2: 부드러운 정지

```
패킷: FF FE 02 00 02
                   └─ checksum: 02^00 = 02
```

### 예시 3: 비상 정지

```
패킷: FF FE 03 00 03
                   └─ checksum: 03^00 = 03
```

---

## ROS 연동

### 데이터 흐름

```
ROS Navigation Stack
    │  /cmd_vel (geometry_msgs/Twist)
    │  linear.x (m/s), angular.z (rad/s)
    ▼
Raspberry Pi (ROS 노드)
    │  Differential Drive 변환:
    │    v_left  = linear.x - angular.z × (wheel_base / 2)
    │    v_right = linear.x + angular.z × (wheel_base / 2)
    │  단위 변환: m/s → mm/s, int16_t로 패킹
    │
    │  UART 전송 (CMD_VELOCITY, 10ms 주기)
    ▼
STM32
    │  수신 → 가속도 제한 적용 → PWM 출력
    ▼
모터 구동
```

### ROS 파라미터와의 일치 요구사항

ROS Navigation Stack에도 로봇의 물리적 한계를 설정하는 파라미터가 있습니다. STM32 펌웨어의 값과 **반드시 일치**시켜야 경로 추적이 정확합니다.

| ROS 파라미터 | 대응하는 STM32 값 | 불일치 시 문제 |
|---|---|---|
| `max_vel_x` | `MOTOR_MAX_SPEED_MMPS` (600 mm/s = 0.6 m/s) | ROS가 로봇이 못 내는 속도를 명령 |
| `max_acceleration` | `MAX_ACCEL_MMPS_PER_10MS` (현재 1.0 m/s²) | ROS 예상보다 느리게 가속 → 경로 이탈 |
| `cmd_vel` 발행 주기 | 전송 주기 (10ms) | RPi가 더 느리게 보내면 STM32 watchdog 작동 가능 |

---

## 주의사항

1. **Watchdog**: STM32는 500ms 이상 속도 명령(`0x01`)이 없으면 자동 정지합니다.
   → RPi 측에서 10ms 주기로 속도 명령을 반복 전송해 주세요.
2. **비상 정지 상태**: `CMD_ESTOP(0x03)` 후에는 `CMD_RELEASE(0x04)`를 보내기 전까지 모든 속도 명령이 무시됩니다.
3. **Checksum 불일치**: STM32는 checksum이 맞지 않는 패킷을 무시합니다.
