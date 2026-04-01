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
  0xFF     0xFE    1B   1B   0~12B      1B
```

| 필드 | 크기 | 설명 |
|------|------|------|
| HEADER | 2 byte | `0xFF 0xFE` (패킷 시작 신호) |
| CMD | 1 byte | 명령 종류 |
| LEN | 1 byte | DATA 필드 길이 (0~12) |
| DATA | 0~12 byte | 명령에 따른 데이터 |
| CHECKSUM | 1 byte | `CMD ^ LEN ^ DATA[0] ^ DATA[1] ^ ...` (XOR) |

- 최소 패킷 크기: **5 byte** (데이터 없는 경우)
- 최대 패킷 크기: **17 byte** (데이터 12바이트)

> 관련 상수: `PROTO_MAX_DATA_LEN=12`, `PROTO_MIN_PACKET_SIZE=5`, `PROTO_MAX_PACKET_SIZE=17` (`uart_protocol.h`)

---

## 명령 정의: CMD 필드

### RPi → STM32 (명령) : `0x01` ~ `0x7F`

| CMD | 이름 | LEN | DATA | 설명 |
|-----|------|-----|------|------|
| `0x01` | CMD_VELOCITY | 4 | `[vL_low, vL_high, vR_low, vR_high]` | 좌/우 바퀴 속도 (mm/s) |
| `0x02` | CMD_STOP | 0 | 없음 | 부드러운 정지 (Coast) |
| `0x03` | CMD_ESTOP | 0 | 없음 | 비상 정지 (Brake) |
| `0x04` | CMD_RELEASE | 0 | 없음 | 비상 정지 해제 |

**CMD_VELOCITY (0x01) 상세:**
- 속도 범위: **-600 ~ +600** (int16_t, mm/s 단위)
- 양수: 전진 / 음수: 후진
- LEN이 4가 아닌 패킷은 `rx_invalid_len` 카운트 후 무시됨
- 수신 시 `Motor_SetVelocity(v_left, v_right)` 호출 → 가속도 제한 적용
- 500ms 이상 명령이 없으면 자동 정지 (watchdog)

### STM32 → RPi (응답/센서) : `0x80` ~ `0xFF`

50ms 주기(20Hz)로 메인 루프에서 자동 송신됩니다.

| CMD | 이름 | LEN | DATA | 설명 |
|-----|------|-----|------|------|
| `0x81` | RSP_ODOM | 4 | `[encL_low, encL_high, encR_low, encR_high]` | 엔코더 카운트 (int16_t × 2) |
| `0x82` | RSP_IMU | 12 | `[ax(2B), ay(2B), az(2B), gx(2B), gy(2B), gz(2B)]` | IMU 6축 raw 데이터 (int16_t × 6) |

**RSP_ODOM (0x81) 상세:**
- `left_ticks`, `right_ticks`: 각각 int16_t (2byte), Little-Endian
- `Encoder_GetCount(ENCODER_LEFT/RIGHT)` 값을 그대로 전송
- 현재 raw 카운트만 전송 (바퀴 지름과 엔코더 분해능을 고려해 거리 변환은 RPi 측에서 수행)

**RSP_IMU (0x82) 상세:**
- `MPU6050_Data_t` 구조체의 6개 int16_t 필드를 순서대로 전송
- 데이터 순서: `accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z`
- 모두 raw 값 (물리량 변환은 RPi 측에서 수행)
- `MPU6050_ReadAll()` 실패 시 해당 주기의 IMU 패킷은 송신하지 않음

---

## 수신 흐름 (RPi → STM32)

```
USART1_IRQHandler()                          ← 하드웨어 인터럽트
  └→ HAL_UART_IRQHandler()                   ← HAL 처리
      └→ HAL_UART_RxCpltCallback()           ← 콜백
          ├→ Protocol_FeedByte(rx_byte)      ← 상태머신에 바이트 투입
          └→ HAL_UART_Receive_IT() 재등록     ← 다음 바이트 수신 준비
```

### 상태머신 (Protocol_FeedByte)

```
WAIT_HEADER1 ──(0xFF)──→ WAIT_HEADER2 ──(0xFE)──→ WAIT_CMD
     ↑                        │                        │
     └──(other)───────────────┘                     (byte)
                                                       │
                                                       ▼
                                                   WAIT_LEN
                                                       │
                                          ┌──(len=0)───┤──(len>0)──┐
                                          ▼            │           ▼
                                    WAIT_CHECKSUM      │      WAIT_DATA
                                          ↑      (len>12→err)     │
                                          └──(data_index>=len)────┘
                                          │
                                  checksum 비교
                                   ├─ 일치: rx_packet → ready_packet 복사
                                   │        packet_ready = true
                                   └─ 불일치: rx_checksum_errors++
                                          │
                                          ▼
                                    WAIT_HEADER1 (리셋)
```

**헤더 처리 특이사항:** `WAIT_HEADER2` 상태에서 `0xFF`를 수신하면 상태를 유지합니다 (연속된 `0xFF` 후 `0xFE`가 올 수 있으므로).

### 더블 버퍼링

ISR과 메인 루프 간 안전한 데이터 전달을 위해 두 개의 패킷 버퍼를 사용합니다:

| 버퍼 | 접근 주체 | 용도 |
|------|-----------|------|
| `rx_packet` | ISR 전용 (쓰기) | 상태머신이 파싱 중인 패킷 |
| `ready_packet` | ISR→메인 루프 (읽기) | 파싱 완료된 패킷 |

- `packet_ready` 플래그(`volatile bool`)로 동기화
- ISR에서 `packet_ready==true`이면 (메인 루프가 아직 안 읽음) 새 패킷을 **버림** → 유실 가능하지만 안전성 확보
- 메인 루프의 `Protocol_Process()`가 `ready_packet`을 로컬 변수로 복사 후 `packet_ready=false` 설정

### 명령 디스패치 (Protocol_Dispatch)

`Protocol_Process()` → `Protocol_Dispatch()` 에서 CMD별 분기:

| CMD | 호출 함수 |
|-----|-----------|
| `0x01` CMD_VELOCITY | `Protocol_HandleVelocity()` → `Motor_SetVelocity(vL, vR)` |
| `0x02` CMD_STOP | `Motor_SoftStop()` |
| `0x03` CMD_ESTOP | `Motor_EmergencyStop()` |
| `0x04` CMD_RELEASE | `Motor_ReleaseEmergency()` |
| 기타 | `rx_unknown_cmd++` (무시) |

---

## 송신 흐름 (STM32 → RPi)

```
main.c while(1) 루프
  └→ 50ms 주기 체크 (HAL_GetTick)
      ├→ Protocol_SendOdom(left_ticks, right_ticks)
      │    └→ Protocol_SendPacket(0x81, data, 4)
      │         └→ HAL_UART_Transmit() [블로킹, 10ms 타임아웃]
      └→ MPU6050_ReadAll(&imu)
           ├─ HAL_OK → Protocol_SendIMU(&imu)
           │             └→ Protocol_SendPacket(0x82, data, 12)
           └─ 실패 → 해당 주기 IMU 전송 스킵
```

**Protocol_SendPacket 구현:**
1. 버퍼에 `[0xFF, 0xFE, CMD, LEN, DATA..., CHECKSUM]` 조립
2. `HAL_UART_Transmit()`으로 블로킹 전송 (타임아웃 10ms)
3. 체크섬 계산: `CMD ^ LEN ^ DATA[0] ^ ... ^ DATA[n-1]`

---

## UART 에러 복구

UART 통신 중 발생할 수 있는 하드웨어 에러를 자동으로 복구합니다.

```c
HAL_UART_ErrorCallback()
  ├→ uart_error_count++
  ├→ __HAL_UART_CLEAR_OREFLAG()   // Overrun Error 클리어
  ├→ __HAL_UART_CLEAR_NEFLAG()    // Noise Error 클리어
  ├→ __HAL_UART_CLEAR_FEFLAG()    // Framing Error 클리어
  └→ HAL_UART_Receive_IT() 재등록  // 인터럽트 체인 복구
```

에러 플래그를 클리어하지 않으면 UART가 멈추므로, 에러 발생 시 즉시 클리어 후 수신을 재개합니다.

---

## 통신 통계 (디버깅)

`ProtoStats_t` 구조체로 통신 상태를 추적합니다:

| 필드 | 설명 |
|------|------|
| `rx_packets` | 정상 수신된 패킷 수 |
| `rx_checksum_errors` | 체크섬 불일치 횟수 |
| `rx_invalid_len` | LEN 필드 범위 초과 (>12) 또는 CMD_VELOCITY의 LEN≠4 |
| `rx_unknown_cmd` | 정의되지 않은 CMD 수신 횟수 |

별도로 `uart_error_count`가 UART 하드웨어 에러(ORE/NE/FE) 발생 횟수를 추적합니다.

500ms 주기로 USART2(PC)를 통해 진단 로그가 출력됩니다:
```
[DIAG] rx_pkt:123 err:0 chk_err:0 unk:0  enc L:456 R:789
```

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

### 예시 4: RSP_ODOM 응답 (L=100, R=-50)

```
100 = 0x0064 → Little-Endian: [0x64, 0x00]
-50 = 0xFFCE → Little-Endian: [0xCE, 0xFF]

패킷: FF FE 81 04 64 00 CE FF 18
                                └─ checksum: 81^04^64^00^CE^FF = 18
```

### 예시 5: RSP_IMU 응답

```
accel_x=100, accel_y=-200, accel_z=16384, gyro_x=0, gyro_y=0, gyro_z=50

패킷: FF FE 82 0C [64 00] [38 FF] [00 40] [00 00] [00 00] [32 00] [CHKSUM]
                    ax_L/H  ay_L/H  az_L/H  gx_L/H  gy_L/H  gz_L/H
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
    │
    │  50ms 주기로 센서 데이터 응답
    │  RSP_ODOM (엔코더) + RSP_IMU (IMU)
    ▼
Raspberry Pi (ROS 노드)
    │  수신 → 물리량 변환 → ROS 토픽 발행
    │  RSP_ODOM → /odom (nav_msgs/Odometry)
    │  RSP_IMU  → /imu  (sensor_msgs/Imu)
    ▼
ROS Navigation Stack (위치 추정, 경로 계획)
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
4. **패킷 유실**: ISR에서 이전 패킷을 메인 루프가 아직 처리하지 않았으면 새 패킷은 버려집니다. 빠른 속도로 연속 전송 시 유의하세요.
5. **송신 블로킹**: `Protocol_SendPacket()`은 `HAL_UART_Transmit()` 블로킹 호출 (10ms 타임아웃)입니다. 50ms 주기 내에 Odom+IMU 2개 패킷을 전송하므로 타이밍에 여유가 있습니다.

---

## 소스 파일 참조

| 파일 | 줄 수 | 역할 |
|------|-------|------|
| `Core/Inc/uart_protocol.h` | 105줄 | 상수, 타입, 함수 선언 |
| `Core/Src/uart_protocol.c` | 245줄 | 프로토콜 구현 전체 |
| `Core/Src/main.c` (130~159) | — | 메인 루프에서 송수신 호출 |
