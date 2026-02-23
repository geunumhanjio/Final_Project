# PID 속도 제어 구현 가이드

## 개요

ROS에서 목표 속도(-600 ~ +600 mm/s)를 수신하면, 엔코더로 실제 속도를 측정하고 PID 제어로 PWM을 조절해 정확한 속도를 구현한다.

---

## 현재 구조 vs 목표 구조

### 현재 (Open-loop)
```
ROS → CMD_VELOCITY(mm/s) → PWM 직접 변환 → 모터
```
명령한 속도와 실제 속도가 다를 수 있음 (부하, 배터리 전압, 마찰 등)

### 목표 (Closed-loop)
```
ROS → CMD_VELOCITY(mm/s) → PID → PWM → 모터
                                ↑
                         엔코더 실제속도
```

---

## 구현 과정

### Step 1: 속도 측정 (엔코더 → mm/s)

현재 `Encoder_GetCount()`는 누적 tick만 반환한다. **10ms마다 변화량**으로 실제 속도를 계산해야 한다.

```
delta_tick = 현재 tick - 이전 tick
speed (mm/s) = (delta_tick / PPR) × 바퀴둘레(mm) / 0.01s
```

| 파라미터 | 값 | 비고 |
|---|---|---|
| PPR | 1920 | TIM3/TIM4 Encoder Mode 4x |
| 바퀴 둘레 | π × 바퀴지름 | **실측 필요** |
| 제어 주기 | 10ms | `HAL_GetTick()` 기준 |

> 구현 위치: `encoder.c`에 `Encoder_GetSpeed()` 함수 추가

---

### Step 2: 바퀴 지름 실측 (캘리브레이션)

소프트웨어로 계산한 이동거리와 실제 이동거리를 비교해 바퀴 둘레를 보정한다.

1. 엔코더 카운트 초기화
2. 모터를 정확히 1회전(1920 tick) 구동
3. 실제 이동거리를 자로 측정
4. 측정값을 바퀴 둘레로 사용

```
바퀴 둘레(mm) = 실측 이동거리(mm)
바퀴 지름(mm) = 바퀴 둘레 / π
```

---

### Step 3: PID 제어기 구현

10ms 주기 루프에서 실행한다.

```
error      = target_speed - measured_speed
integral  += error × dt
derivative = (error - prev_error) / dt

output = Kp×error + Ki×integral + Kd×derivative
PWM    = clamp(output, 0, 999)
```

> 구현 위치: `motor_control.c` 또는 별도 `pid.c` / `pid.h`

---

### Step 4: PID 게인 튜닝

순서대로 진행하며 USART2 printf로 `target / measured / pwm` 값을 실시간 확인한다.

| 순서 | 방법 | 목적 |
|---|---|---|
| 1 | `Ki=0, Kd=0` 상태에서 Kp만 증가 | 기본 반응 확인 |
| 2 | 정상상태 오차가 남으면 Ki 추가 | 목표 속도 도달 |
| 3 | 진동/오버슈트 발생 시 Kd 추가 | 안정화 |

---

## 구현 위치 요약

| 작업 | 파일 |
|---|---|
| 속도 계산 | `Core/Src/encoder.c` — `Encoder_GetSpeed()` 추가 |
| PID 로직 | `Core/Src/motor_control.c` 또는 신규 `pid.c` |
| 10ms 주기 실행 | `Core/Src/main.c` while 루프 |
| 게인 상수 정의 | `Core/Inc/motor_control.h` — `#define Kp, Ki, Kd` |

---

## 관련 문서

- [Encoder Setup Guide](Encoder_Setup_Guide.md)
- [Motor Control Guide](Motor_Control_Complete_Guide.md)
