# 발표 자료 구성 제안

---

## 1. 구현 목표 (What)

프로젝트 전체 목표 중 BSP(Board Support Package) 드라이버 개발자 역할:

- 한화비전 AI CCTV → ROS 자율주행 로봇 → 현장 개입 시스템에서, 로봇의 최하위 하드웨어 제어 계층 담당
- STM32 NUCLEO-F401RE가 Raspberry Pi 4(ROS)와 UART 통신하며 모터 구동, 센서 데이터 수집을 수행

구현 범위:

| 항목      | 내용         | STM보드 제어 방식 | 
| :-------- | :----------------------------------------- | :------ | 
| 모터 제어 | L298N 모터드라이버를 통해 로봇 바퀴의 DC모터 제어  | PWM과 GPIO 신호로 회전 속력과 방향 제어 |
| RPi↔STM32 통신 | STM32에서 RPi로 센서 데이터를 송신하고 모터 제어 명령을 수신 명령 | UART 시리얼 통신 | 
| 휠 엔코더 | 좌/우 바퀴 회전 속력과 방향 측정  | 타이머 Encoder 모드 | 
| IMU 센서 | MPU6050 6축 센서의 가속도계와 자이로스코프로 로봇 몸체의 기울기와 회전을 측정하여 바닥면에 따라 오차가 발생할 수 있는 엔코더를 보조  | I2C 시리얼 통신 | 

---

## 2. 구현 결과 (How — 모듈별 설명)

### (1) UART Protocol (uart_protocol.c — 245줄)

- 인터럽트 기반 1바이트 수신 → 6단계 상태머신(State Machine)으로 패킷 파싱
- 더블 버퍼링: ISR이 `rx_packet`에 쓰고, 메인 루프가 `ready_packet`에서 읽음 → 데이터 경합 방지
- 4가지 명령 처리: `CMD_VELOCITY`(속도 설정), `CMD_STOP`(정지), `CMD_ESTOP`(비상정지), `CMD_RELEASE`(해제)
- 50ms 주기로 `RSP_ODOM`(엔코더), `RSP_IMU`(IMU) 응답 패킷 송신
- 에러 복구: UART 에러(ORE/NE/FE) 발생 시 자동 재시작 → 통신 끊김 방지

### (2) Motor Control (motor_control.c — 314줄)

- L298N 진리표 기반 방향 제어 (GPIO) + TIM2 PWM 속도 제어 (1kHz, 1000단계)
- 안전 기능 3가지:
  - 가속도 제한: 10ms당 최대 200mm/s 변화 → 급가속 방지
  - 명령 타임아웃: 500ms간 새 명령 없으면 자동 SoftStop (통신 두절 대비)
  - 비상 정지: 즉시 브레이크 모드(`IN1=IN2=HIGH`), 해제 전까지 모든 명령 무시

### (3) Encoder (encoder.c — 26줄)

- TIM3(왼쪽)/TIM4(오른쪽)를 하드웨어 Encoder Mode로 설정
- A상/B상 신호를 타이머가 자동 카운팅 → CPU 부하 없이 회전량 측정
- 1920 PPR (Pulses Per Revolution) 해상도

### (4) MPU6050 (mpu6050.c — 73줄)

- I2C1 (400kHz Fast Mode)로 MPU6050과 통신
- `WHO_AM_I` 레지스터로 연결 검증 후 초기화
- 14바이트 burst read로 가속도 3축 + 자이로 3축 한 번에 읽기

### 메인 루프 타이밍

- 매 루프 → `Protocol_Process()` + `Motor_CheckTimeout()`
- 50ms 주기 → RPi에 엔코더/IMU 데이터 송신 (20Hz)
- 500ms 주기 → USART2로 디버그 로그 출력 (PC 시리얼 모니터)

---

## 3. 소프트웨어 아키텍처 (발표 시 다이어그램 활용)

`architecture.md`에 있는 블록 다이어그램과 레이어 구조를 슬라이드에 넣으면 효과적입니다:

```
ROS Navigation Stack → RPi 4 (ROS Node) → UART → STM32
                                                  ├─ Motor Control (TIM2 PWM → L298N → DC Motor)
                                                  ├─ Encoder (TIM3/TIM4 → 바퀴 회전 카운트)
                                                  └─ MPU6050 (I2C → 6축 센서)
```

ISR vs 메인 루프 분리 구조도 강조 포인트:
- ISR: 빠른 바이트 수신 + 파싱만 담당
- 메인 루프: 명령 실행(모터 제어) + 센서 송신

---

## 4. 추가 구현 계획 (Next Steps)

| 우선순위 | 기능              | 설명                                                              |
| :------- | :---------------- | :---------------------------------------------------------------- |
| 1        | 오도메트리 계산   | 엔코더 tick → 이동 거리(mm)/각도(rad) 변환. 16비트 오버플로우 처리 포함 |
| 2        | IMU 물리량 변환   | raw 값 → deg/s, m/s² 변환 (sensitivity: 131 LSB/(deg/s), 16384 LSB/g) |
| 3        | PID 속도 제어     | 엔코더 피드백으로 실제 속도 측정 → 목표 속도와 비교 → PWM 보정        |
| 4        | ROS 연동 완성     | RPi 측 ROS 노드에서 /odom, /imu 토픽 발행 (RPi 팀과 협업)             |

---

## 발표 포인트 요약

1.  4개 모듈(UART Protocol, Motor Control, Encoder, MPU6050)을 직접 설계/구현
2.  안전 중심 설계: 가속도 제한, 명령 타임아웃, 비상 정지 — 산업 안전 로봇에 필수
3.  ISR-메인루프 분리: 실시간 통신 안정성 확보 (더블 버퍼링, volatile 동기화)
4.  다음 단계: 센서 데이터 물리량 변환 → PID 제어 → ROS 완전 연동
