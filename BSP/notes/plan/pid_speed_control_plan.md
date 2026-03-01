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

---

## 심화 구현 (기본 구현 완료 후)

기본 PID가 동작한 뒤 아래 순서대로 개선한다.

---

### 심화 Step 1: 제어 루프를 타이머 ISR로 이동

현재 계획의 `main.c` while 루프 + `HAL_GetTick()` 방식은 루프 내 다른 작업(UART 처리 등)에 따라 실행 주기가 흔들린다.
TIM5를 10ms 주기로 설정하고 인터럽트 콜백 안에서 제어 루프를 실행하면 타이밍이 보장된다.

```c
// CubeMX: TIM5 → Period = 839999 (84MHz / 84 prescaler → 1kHz, period 10 = 10ms)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) {
        float speed_L = Encoder_GetSpeed(LEFT);
        float speed_R = Encoder_GetSpeed(RIGHT);
        Motor_SetPWM(LEFT,  PID_Update(&pid_L, speed_L));
        Motor_SetPWM(RIGHT, PID_Update(&pid_R, speed_R));
    }
}
```

> **왜 하는가:** 메인 루프는 실행 시간이 가변적이라 제어 주기가 불균일해지고 PID 계산이 틀려진다.

---

### 심화 Step 2: 적분 와인드업(Integral Windup) 방지

로봇이 장애물에 막히거나 모터가 정지된 상태에서 오차가 계속 쌓이면 적분항이 한계 없이 커진다.
장애물 해제 후 PWM이 폭주하는 원인이 된다. 적분항에 상한/하한을 강제로 적용한다.

```c
pid->integral += error * dt;
if      (pid->integral >  INTEGRAL_LIMIT) pid->integral =  INTEGRAL_LIMIT;
else if (pid->integral < -INTEGRAL_LIMIT) pid->integral = -INTEGRAL_LIMIT;
```

`INTEGRAL_LIMIT`는 최대 PWM(999)의 30~50% 수준에서 시작해 튜닝한다.

> **왜 하는가:** 와인드업 방지 없이는 로봇이 막혔다 풀릴 때 모터가 갑자기 최대 출력으로 튀는 위험이 있다.

---

### 심화 Step 3: 피드포워드(Feed-Forward) 추가

PID만 사용하면 목표 속도에 도달하기까지 오차가 쌓이는 시간이 필요하다.
목표 속도에서 예상 PWM을 직접 계산해 더해주면, PID는 나머지 오차만 교정하면 되므로 응답이 빠르고 오버슈트가 줄어든다.

```c
// FF_GAIN: (최대 PWM) / (최대 속도) = 999 / 600 ≈ 1.665 에서 시작
float ff     = target_speed * FF_GAIN;
float output = ff + PID_Update(&pid, measured_speed);
output = clamp(output, 0, 999);
```

튜닝 방법: Ki=0인 상태에서 FF_GAIN만 조절해 오버슈트 없이 목표 속도에 근접하게 만든 뒤, 남은 정상 상태 오차를 Ki로 제거한다.

> **왜 하는가:** 피드포워드는 모터의 물리적 특성을 사전 보상하므로, PID 게인을 낮게 유지하면서도 빠른 응답을 얻을 수 있다.

---

### 심화 Step 4: 미분항 개선

#### 4-1. 오차가 아닌 측정값을 미분 (Derivative on Measurement)

목표 속도가 스텝 변화할 때 `(error - prev_error) / dt`는 미분항이 순간적으로 폭발한다("미분 킥").
측정값을 미분하면 setpoint 변화에 반응하지 않고 실제 속도 변화만 반영한다.

```c
// 기존 (미분 킥 발생)
derivative = (error - prev_error) / dt;

// 개선 (미분 킥 없음)
derivative = -(measured - prev_measured) / dt;
prev_measured = measured;
```

#### 4-2. 미분항 저역통과 필터 (Derivative Low-Pass Filter)

엔코더 tick 차이를 미분하면 1~2 tick 오차가 큰 노이즈가 된다. 1차 필터로 고주파 성분을 제거한다.

```c
// alpha: 0.7~0.9 범위에서 튜닝 (높을수록 필터 강함 = 반응 느림)
#define DERIV_ALPHA 0.8f
float raw_d      = -(measured - prev_measured) / dt;
filtered_d       = DERIV_ALPHA * prev_filtered_d + (1.0f - DERIV_ALPHA) * raw_d;
prev_filtered_d  = filtered_d;
```

> **왜 하는가:** 노이즈가 많은 미분항은 Kd를 크게 하지 못하게 막는다. 필터 적용 후 Kd 효과를 제대로 볼 수 있다.

---

### 심화 구현 우선순위 요약

| 순서 | 항목 | 난이도 | 효과 |
|---|---|---|---|
| 1 | 타이머 ISR로 이동 | 낮음 | 제어 주기 안정화 |
| 2 | 적분 와인드업 방지 | 낮음 | 안전성 향상 |
| 3 | 피드포워드 추가 | 중간 | 응답 속도/안정성 크게 향상 |
| 4 | 미분 on Measurement + LPF | 중간 | 미분 킥 제거, 노이즈 감소 |

---

## 관련 문서

- [Encoder Setup Guide](Encoder_Setup_Guide.md)
- [Motor Control Guide](Motor_Control_Complete_Guide.md)
