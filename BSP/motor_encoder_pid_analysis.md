# 모터 · 엔코더 · PID 코드 분석 가이드라인

> **대상 파일**
> - `Components/Motor/drv_encoder.c` / `.h`
> - `Components/Motor/drv_motor.c` / `.h`
> - `Components/Motor/algo_pid.c` / `.h`
> - `Core/Src/main.c` (메인 루프 — 호출 구조)
>
> **분석 목적** 
> 인간의 코드 이해 없이 AI만을 활용해 하드웨어 제어 코드를 개선하는 실험을 진행한다. 이를 통해 앱 레이어가 아닌 로우 레벨(Low-level) 임베디드 환경에서 AI가 단독으로 펌웨어를 작성할 수 있는지, 그리고 실제 하드웨어 제어 시 실수를 유발하는지 확인하는 것이 핵심 목적이다.
---

## 1. 전체 데이터 흐름 요약

```
[엔코더 하드웨어]
  TIM3/TIM4 CNT 레지스터 자동 증감 (HW)
        │
        ▼
Encoder_GetSpeed()          ← 10ms마다 main.c에서 호출
  └─ delta = current - prev  (int16_t 뺄셈 → 오버플로우 자동 처리)
  └─ mm/s = delta / PPR * 둘레 / dt
        │
        ▼
Motor_PID_Update(spd_L, spd_R)   ← 10ms마다 main.c에서 호출
  └─ PID_Update() × 2 (좌·우)
  └─ output = Kp*error + Ki*integral + Kd*derivative  [mm/s]
        │
        ▼
Motor_SetRaw(LEFT/RIGHT, output_mmps)
  └─ SpeedMMPS_To_PWM(): mm/s → PWM pulse (0~999)
  └─ 방향 GPIO (PC0/PC1, PC2/PC3)
  └─ __HAL_TIM_SET_COMPARE(&htim2, CH1/CH2, pwm)
        │
        ▼
[L298N 모터 드라이버] → [DC 모터]
```

---

## 2. `drv_encoder.c` 분석

### 2-1. 하드웨어 설정 확인

| 항목 | 값 | 근거 |
|------|-----|------|
| 왼쪽 타이머 | TIM3 (PA6 CH1, PA7 CH2) | `tim.c` MX_TIM3_Init |
| 오른쪽 타이머 | TIM4 (PB6 CH1, PB7 CH2) | `tim.c` MX_TIM4_Init |
| 인코더 모드 | `TIM_ENCODERMODE_TI12` | A상·B상 모두 카운트 → **4체배** |
| 타이머 최대값 | 65535 (16비트) | `Init.Period = 65535` |
| 입력 필터 | `IC1Filter = IC2Filter = 10` | 채터링 방지 (10 클록 샘플링) |

**PPR 계산:**
```
물리 CPR(엔코더 한 바퀴): 480 펄스
4체배(TI12 모드):          × 4
ENCODER_PPR:              = 1920 tick/rev
```

**바퀴 둘레 계산:**
```
직경 65mm → 둘레 = π × 65 = 204.2mm  ✓
```

### 2-2. 오른쪽 엔코더 부호 반전 (`-` 부호)

```c
// drv_encoder.c:21
return -(int16_t)__HAL_TIM_GET_COUNTER(&htim4);
```

왼쪽·오른쪽 모터는 로봇에 **서로 마주보게** 장착되어 있어, 로봇이 직진할 때 두 모터의 물리적 회전 방향이 반대다. 이 `-1` 부호가 두 바퀴의 "앞으로" 방향을 소프트웨어에서 통일시킨다.

> **확인 포인트:** 실제 로봇을 앞으로 밀었을 때 `Encoder_GetCount(LEFT)`와 `Encoder_GetCount(RIGHT)` 부호가 **둘 다 양수**인지 검증하라.

### 2-3. 16비트 오버플로우 자동 처리 원리

타이머 CNT는 0 → 65535 → 0 으로 롤오버(Overflow), 또는 65535 → 0 → 65535 로 언더플로우한다.

```c
int16_t delta = current - prev_left;  // int16_t 뺄셈
```

`int16_t`의 범위는 -32768 ~ +32767이다. 10ms 동안 엔코더 변화량이 ±32767 tick을 넘지 않는다면, **2의 보수 산술 덕분에 오버플로우가 자동으로 상쇄된다.**

예시 (언더플로우):
```
current  =     5  (0x0005)
prev     = 65530  (0xFFFA)
delta    = 5 - 65530 = -65525  (uint16 기준)
int16_t 캐스팅 후: -65525 + 65536 = +11  ✓ 실제 이동량과 일치
```

> **한계:** 10ms 안에 32767 tick(≈ 3,400 mm/s) 이상 변하면 계산이 틀린다. 현재 최대 속도(600 mm/s)에서는 안전하다.

### ⚠️ 2-4. 알려진 버그: `Encoder_ResetCount()` 후 Speed Spike

```c
void Encoder_ResetCount(Encoder_Select_t encoder)
{
    __HAL_TIM_SET_COUNTER(&htim3, 0);  // 타이머 CNT만 0으로 초기화
    // ← prev_left static 변수는 초기화되지 않음!
}
```

`Encoder_GetSpeed()` 내부의 `prev_left` / `prev_right`는 `static` 변수라서 `Encoder_ResetCount()`를 호출해도 초기화되지 않는다.

**버그 재현 시나리오:**
```
prev_left  = 1000  (리셋 직전의 누적 카운트)
타이머 리셋 후 current = 0
delta = 0 - 1000 = -1000 tick

속도 계산: -1000 / 1920 * 204.2 / 0.01 = -10,635 mm/s (!)
```

한 프레임 동안 17배 이상의 속도 스파이크가 PID에 입력되어 모터가 순간적으로 오작동한다.

---

## 3. `algo_pid.c` 분석

### 3-1. 이산 PID 공식 확인

```c
float error      = target - measured;
pid->integral   += error * pid->dt;          // 직사각형 적분법 (Rectangular rule)
float derivative = (error - pid->prev_error) / pid->dt;  // 후방 차분 미분

float output = Kp * error + Ki * integral + Kd * derivative;
```

| 항 | 역할 | 현재 게인 |
|----|------|----------|
| P항 `Kp * error` | 현재 오차 비례 보정 | **1.0** |
| I항 `Ki * integral` | 누적 오차 보정 (정상 상태 오차 제거) | **0.0** (미사용) |
| D항 `Kd * derivative` | 오차 변화율 억제 (진동 감쇠) | **0.0** (미사용) |

출력 클램프: `-600 ~ +600 mm/s` (`MOTOR_MAX_SPEED_MMPS`)

### 3-2. 현재 Kp=1.0만 사용 시 동작 특성

```
target = 300 mm/s,  measured = 0 mm/s
→ error = 300,  output = 300 → PWM 약 50%
→ 모터 가속 → measured = 280 mm/s
→ error = 20,  output = 20 → PWM 약 3.3%
→ 마찰력 > 모터 힘 → 감속 → error 다시 증가 → 반복
```

**결론:** Kp=1.0만으로는 정상 상태에서 모터가 목표 속도를 유지하지 못하고 목표 주변에서 **헌팅(hunting, 진동)** 한다. Ki를 추가하여 적분기가 누적 오차를 보상해야 지속적인 속도 유지가 가능하다.

### ⚠️ 3-3. 알려진 문제: Integral Windup (적분 누적) 방어 없음

```c
pid->integral += error * pid->dt;  // 상한 없이 무한 누적 가능
```

**문제 시나리오:**
- 로봇 바퀴가 벽에 막혀 측정 속도 = 0인데 목표 속도 = 300 mm/s
- `error = 300`이 계속 누적 → `integral`이 수천 단위로 폭증
- 벽에서 벗어나는 순간 `Ki * integral`이 폭발적 출력을 만들어 로봇이 순간 급출발

**대응 방법 (구현 과제):**
```c
// integral 클램프 예시 (적분항이 최대 출력의 50%를 넘지 않도록)
float windup_limit = (float)MOTOR_MAX_SPEED_MMPS / (pid->Ki > 0 ? pid->Ki : 1.0f);
if      (pid->integral >  windup_limit) pid->integral =  windup_limit;
else if (pid->integral < -windup_limit) pid->integral = -windup_limit;
```

### 3-4. dt 동기화 구조 확인

```c
// drv_motor.h
#define PID_DT  0.01f   // 10ms

// main.c (PID 루프)
if (now - last_pid >= 10) {   // 10ms 경과 시 실행
    Motor_PID_Update(spd_L, spd_R);
}
```

`PID_DT`(0.01s)와 메인 루프 호출 주기(10ms)가 일치한다. ✓

> **주의:** 메인 루프에서 다른 작업(UART 송수신 등)이 길어지면 실제 호출 간격이 10ms를 초과할 수 있다. 정밀한 제어가 필요하다면 `HAL_GetTick()`으로 실제 `dt`를 계산하는 방식으로 개선할 수 있다.

---

## 4. `drv_motor.c` 분석

### 4-1. PWM 해상도 및 주파수 계산

```
시스템 클록: 84 MHz
TIM2 Prescaler: 83  → TIM2 클록 = 84 MHz / (83+1) = 1 MHz
TIM2 Period(ARR): 999 → PWM 주파수 = 1 MHz / (999+1) = 1 kHz
PWM 해상도: 1000 단계 (0~999)
```

**속도 → PWM 변환 공식:**
```c
pwm = (speed_mmps * 999) / 600
```
예: 300 mm/s → 300 * 999 / 600 = 499.5 → 499 (정수 내림)

### 4-2. L298N 진리표 vs 코드 일치 확인

| 방향 | IN1 | IN2 | 코드 |
|------|-----|-----|------|
| 정방향 | HIGH | LOW | `forward=1` → SET, RESET ✓ |
| 역방향 | LOW | HIGH | `forward=0` → RESET, SET ✓ |
| 브레이크 | HIGH | HIGH | `EmergencyStop()` ✓ |
| 코스트 | LOW | LOW | `ReleaseEmergency()` 후 코스트 ✓ |

### 4-3. 상태 머신 구조

```
[초기화] Motor_Init()
    │  PWM 타이머 시작, GPIO 초기화, PID 인스턴스 생성
    ▼
[대기] last_cmd_time 갱신
    │
    ├─ Motor_SetVelocity() 수신 → target_mmps 업데이트 → 타임아웃 리셋
    ├─ Motor_CheckTimeout() 500ms 경과 → Motor_SoftStop() (target=0)
    ├─ Motor_EmergencyStop() → emergency_stop_active=1 → 즉시 브레이크
    └─ Motor_ReleaseEmergency() → emergency_stop_active=0 → 코스트 모드

[PID 루프 — 10ms]
    Motor_PID_Update(spd_L, spd_R)
      └─ emergency_stop_active=1이면 즉시 리턴 (PID 무력화)
      └─ PID_Update() → Motor_SetRaw()
```

### ⚠️ 4-4. 알려진 문제: 데드밴드(Deadband) 미처리

```c
static uint16_t SpeedMMPS_To_PWM(uint16_t speed_mmps)
{
    return (uint16_t)((speed_mmps * MOTOR_PWM_PERIOD) / MOTOR_MAX_SPEED_MMPS);
}
// 예: 10 mm/s → pwm = 10*999/600 = 16 (약 1.6% 듀티)
```

1~2% 듀티의 PWM은 DC 모터의 정지 마찰력(static friction)을 이기지 못한다. 모터 코일에 전류가 흘러 열만 발생하고 회전하지 않는다.

**대응 방법 (구현 과제):**
```c
// 최소 기동 PWM 이하이면 0으로 처리 (예: 8% = 약 80/999)
#define MOTOR_DEADBAND_PWM 80
if (pwm_value > 0 && pwm_value < MOTOR_DEADBAND_PWM) {
    pwm_value = 0;  // 또는 MOTOR_DEADBAND_PWM으로 올림 (기동 보조)
}
```

### ⚠️ 4-5. 디버그 printf 성능 문제

```c
// drv_motor.c:110 — Motor_SetRaw()에 포함
printf("[Real Motor PWM Value and Speed] %s: pwm = %d ...\r\n", ...);
```

`Motor_SetRaw()`는 `Motor_PID_Update()` 안에서 **10ms마다 2회** 호출된다. USART2 (115200 baud)로 긴 문자열을 출력하면 **수 ms의 블로킹**이 발생하여 PID 루프 타이밍을 어긋나게 한다.

> **권장:** 튜닝이 완료되면 이 `printf`를 제거하거나, 500ms 주기 진단 출력(`[DIAG]` 블록)으로 통합한다.

---

## 5. 메인 루프 타이밍 구조 (main.c)

```
while(1)
├─ Protocol_Process()        : UART 패킷 파싱·디스패치 (블로킹 없음)
├─ Motor_CheckTimeout()      : 500ms 타임아웃 감시
│
├─ [10ms 주기] PID 제어 루프
│   ├─ Encoder_GetSpeed(LEFT)   → last_spd_L
│   ├─ Encoder_GetSpeed(RIGHT)  → last_spd_R
│   └─ Motor_PID_Update(L, R)
│
├─ [50ms 주기] 센서 데이터 송신
│   ├─ Encoder_GetCount() × 2  → delta 계산 → Protocol_SendOdom()
│   └─ MPU6050_ReadAll()        → Protocol_SendIMU()
│
└─ [500ms 주기] 디버그 출력 (USART2)
    └─ printf("[DIAG] ...")
```

> **주의:** `Encoder_GetSpeed()`는 내부 `prev` 변수를 갱신하므로, PID 루프 외에서 **추가 호출하면 안 된다.** 현재 코드에서 `last_spd_L/R`에 저장한 값을 재사용하는 구조는 올바르다. ✓

---

## 6. 개선 우선순위 요약

| 순위 | 파일 | 문제 | 영향도 |
|------|------|------|--------|
| 🔴 1 | `algo_pid.c` | Ki 게인 튜닝 필요 (현재 0 → 정상 상태 오차 발생) | 높음 |
| 🔴 2 | `algo_pid.c` | Anti-Windup 로직 없음 | 높음 |
| 🟡 3 | `drv_encoder.c` | `Encoder_ResetCount()` 후 Speed Spike 버그 | 중간 |
| 🟡 4 | `drv_motor.c` | 데드밴드 미처리 (저속 시 모터 미동작) | 중간 |
| 🟢 5 | `drv_motor.c` | `Motor_SetRaw()` 내 디버그 printf 성능 저하 | 낮음 |

---

## 7. PID 튜닝 절차 (권장 순서)

```
1단계: Kp 단독 튜닝 (Ki=0, Kd=0)
   - Kp를 0.5부터 시작해 목표 속도의 90% 이상 도달할 때까지 올림
   - 진동(oscilation)이 심해지면 Kp를 10~20% 내림

2단계: Ki 추가 (정상 상태 오차 제거)
   - Ki를 0.05~0.1 단위로 올리면서 정상 상태 오차 확인
   - Anti-Windup 로직 적용 후 추가할 것

3단계: Kd 추가 (선택, 진동이 남은 경우)
   - Kd는 소음에 민감하므로 엔코더 필터링이 없으면 주의

측정 방법 (USART2 [DIAG] 출력 활용):
   printf("[DIAG] spd L:%.1f R:%.1f mm/s  enc L:%d R:%d\r\n", ...);
   → 500ms마다 출력되므로 정상 상태 도달 여부 확인 가능
```

---

*분석 기준일: 2026-03-02*
*대상 브랜치: `BSP/feat/motor-pid`*
