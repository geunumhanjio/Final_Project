# 시스템 아키텍처

## 1. 시스템 블록 다이어그램

```
┌─────────────────────────────────────────────────────────┐
│                    ROS Navigation Stack                 │
│              /cmd_vel (Twist) → diff drive 변환         │
└────────────────────────┬────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                  Raspberry Pi 4                         │
│    ┌──────────────────────────────────────────┐         │
│    │  ROS 노드                                │         │
│    │  - /cmd_vel 구독 → UART 명령 패킷 전송   │         │
│    │  - UART 응답 수신 → /odom, /imu 발행     │         │
│    └──────────────────┬───────────────────────┘         │
│                       │ UART (115200 baud)               │
│              GPIO14(TX)↔PA10(RX)                        │
│              GPIO15(RX)↔PA9(TX)                         │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│               STM32 NUCLEO-F401RE (84 MHz)              │
│                                                         │
│  ┌─────────┐  ┌──────────┐  ┌────────┐  ┌───────────┐  │
│  │  UART   │  │  Motor   │  │Encoder │  │  MPU6050  │  │
│  │Protocol │→ │ Control  │  │ Driver │  │  Driver   │  │
│  │(USART1) │  │(TIM2 PWM)│  │(TIM3/4)│  │  (I2C1)   │  │
│  └─────────┘  └────┬─────┘  └───┬────┘  └─────┬─────┘  │
│                     │            │              │        │
└─────────────────────┼────────────┼──────────────┼────────┘
                      │            │              │
                      ▼            ▼              ▼
               ┌──────────┐ ┌──────────┐  ┌────────────┐
               │  L298N   │ │ 엔코더   │  │  GY-521    │
               │모터드라이버│ │ TT 모터  │  │ (MPU6050)  │
               └────┬─────┘ └──────────┘  └────────────┘
                    │
                    ▼
              ┌──────────┐
              │ DC 모터  │
              │ 좌 / 우  │
              └──────────┘
```

## 2. 소프트웨어 레이어 구조

```
┌──────────────────────────────────────────────────┐
│  Application Layer (Core/Src/main.c)             │
│  - 주변장치 초기화                                │
│  - 메인 루프: Protocol_Process + 센서 송신        │
├──────────────────────────────────────────────────┤
│  Module Layer (사용자 작성 드라이버)               │
│  ┌──────────────┬───────────┬──────────────────┐ │
│  │uart_protocol │motor_ctrl │ encoder │mpu6050 │ │
│  │  .c/.h       │  .c/.h    │  .c/.h  │ .c/.h  │ │
│  └──────────────┴───────────┴──────────────────┘ │
├──────────────────────────────────────────────────┤
│  HAL Layer (Drivers/STM32F4xx_HAL_Driver/)       │
│  - HAL_UART, HAL_TIM, HAL_I2C, HAL_GPIO         │
│  - CubeMX가 자동 생성 — 수정 금지                 │
├──────────────────────────────────────────────────┤
│  CMSIS (Drivers/CMSIS/)                          │
│  - Cortex-M4 코어 정의, 스타트업 코드, 벡터 테이블│
└──────────────────────────────────────────────────┘
```

### 모듈별 역할

| 모듈 | 파일 | 역할 |
|------|------|------|
| UART Protocol | `uart_protocol.c/.h` | RPi↔STM32 패킷 파싱/디스패치, 센서 데이터 송신 |
| Motor Control | `motor_control.c/.h` | L298N 제어 (방향 GPIO + TIM2 PWM), 가속도 제한, 타임아웃 |
| Encoder | `encoder.c/.h` | TIM3/TIM4 엔코더 모드 카운트 읽기 |
| MPU6050 | `mpu6050.c/.h` | I2C1 통신으로 6축 센서 데이터 읽기 |

## 3. 메인 루프 타이밍

```c
while (1) {
    Protocol_Process();      // 매 루프: 수신 패킷 확인 → 명령 디스패치
    Motor_CheckTimeout();    // 매 루프: 500ms 타임아웃 체크

    if (now - last_send >= 50ms) {   // 50ms 주기 (20Hz)
        Protocol_SendOdom();          // 엔코더 카운트 → RPi 전송
        Protocol_SendIMU();           // IMU 6축 데이터 → RPi 전송
    }

    if (now - last_print >= 100ms) { // 100ms 주기 (10Hz)
        printf(엔코더 + IMU 디버그);  // USART2 → PC 시리얼 모니터
    }
}
```

### 타이밍 요약

| 동작 | 주기 | 실행 위치 |
|------|------|-----------|
| `Protocol_Process()` | 매 루프 | 메인 루프 |
| `Motor_CheckTimeout()` | 매 루프 | 메인 루프 |
| 센서 데이터 RPi 전송 | 50ms (20Hz) | 메인 루프 |
| 디버그 출력 (USART2) | 100ms (10Hz) | 메인 루프 |
| UART 바이트 수신 | 인터럽트 | ISR (USART1) |

## 4. ISR vs 메인 루프 컨텍스트

```
         ISR 컨텍스트                         메인 루프 컨텍스트
    ┌───────────────────┐              ┌──────────────────────────┐
    │ USART1_IRQHandler │              │ while(1) {               │
    │   │                │              │   if (packet_ready) {    │
    │   ▼                │              │     packet_ready = false │
    │ HAL_UART_IRQHandler│              │     Protocol_Dispatch()  │
    │   │                │              │     → Motor_SetVelocity  │
    │   ▼                │              │     → Motor_EmergencyStop│
    │ HAL_UART_RxCplt   │  packet_ready │     → Motor_SoftStop     │
    │ Callback()        │──(volatile)──→│   }                      │
    │   │                │              │   Motor_CheckTimeout()   │
    │   ▼                │              │   센서 데이터 송신        │
    │ Protocol_FeedByte()│              │ }                        │
    │   상태머신 파싱     │              │                          │
    │   → ready_packet   │              │                          │
    │   → packet_ready=1 │              │                          │
    │   HAL_UART_Receive │              │                          │
    │   _IT (다음 바이트) │              │                          │
    └───────────────────┘              └──────────────────────────┘
```

**핵심 포인트:**
- `Protocol_FeedByte()`는 **ISR에서 실행**되므로 빠르게 완료되어야 함
- `packet_ready`는 `volatile bool`로 선언하여 ISR↔메인 루프 동기화
- 패킷 파싱은 ISR에서, 명령 실행(모터 제어)은 메인 루프에서 수행
- 더블 버퍼링: `rx_packet`(ISR 전용) → `ready_packet`(메인 루프 전용)

## 5. 타이머 할당표

| 타이머 | 기능 | 핀 | 설정 |
|--------|------|-----|------|
| TIM2 | PWM (모터 속도) | PA0 (CH1, 왼쪽), PA1 (CH2, 오른쪽) | PSC=83, ARR=999 → 1kHz |
| TIM3 | 엔코더 (왼쪽) | PA6 (CH1), PA7 (CH2) | Encoder Mode TI12, Filter=10, Period=65535 |
| TIM4 | 엔코더 (오른쪽) | PB6 (CH1), PB7 (CH2) | Encoder Mode TI12, Filter=10, Period=65535 |

## 6. 통신 인터페이스

| 인터페이스 | 용도 | 핀 | 설정 |
|------------|------|-----|------|
| USART1 | RPi↔STM32 데이터 통신 | PA9 (TX), PA10 (RX) | 115200 baud, 인터럽트 수신 |
| USART2 | PC 디버그 (VCP) | PA2 (TX), PA3 (RX) | 115200 baud, printf 출력 |
| I2C1 | MPU6050 센서 통신 | PB8 (SCL), PB9 (SDA) | Fast Mode 400kHz |

## 7. 구현 현황

| 기능 | 상태 | 비고 |
|------|------|------|
| 모터 PWM 제어 | 완료 | L298N, TIM2 CH1/CH2 |
| 가속도 제한 | 완료 | 10ms당 200mm/s |
| 명령 타임아웃 | 완료 | 500ms watchdog |
| 비상 정지/해제 | 완료 | 브레이크 모드 |
| UART 패킷 수신 (파싱) | 완료 | ISR 상태머신 |
| UART 패킷 송신 (Odom) | 완료 | 엔코더 카운트 int16 x2 |
| UART 패킷 송신 (IMU) | 완료 | 6축 raw int16 x6 |
| 엔코더 카운트 읽기 | 기본 구현 | 오버플로우 미처리, 거리 변환 미구현 |
| MPU6050 데이터 읽기 | 기본 구현 | raw 데이터만 (물리량 변환 미구현) |
| PID 속도 제어 | 미구현 | 엔코더 피드백 기반 제어 |
| 오도메트리 계산 | 미구현 | 엔코더 → 이동 거리/각도 변환 |
| IMU 물리량 변환 | 미구현 | raw → deg/s, m/s² 변환 |
