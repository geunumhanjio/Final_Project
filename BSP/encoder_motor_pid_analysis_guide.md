# 엔코더 · 모터 · PID 코드 분석 가이드

## 분석 순서 — 아래에서 위로 (하드웨어 → 제어)

```
[하드웨어]  엔코더(TIM3/4) ──→ 속도 측정
                                    ↓
[제어]      PID ──────────────→ 출력값 계산
                                    ↓
[하드웨어]  모터(TIM2 PWM + GPIO) ← 출력 적용
```

거꾸로 올라가지 말고, **엔코더 → PID → 모터** 순서로 읽어야 각 층이 왜 존재하는지 보인다.

---

## 1단계: 엔코더 (`encoder.c`)

**질문 3가지를 들고 읽기:**
1. 타이머가 어떻게 회전수를 숫자로 바꾸는가? (`TIM_CHANNEL_ALL`, `__HAL_TIM_GET_COUNTER`)
2. `int16_t`로 캐스팅하는 이유가 뭔가? (역방향 회전 시 어떤 값이 나오는가)
3. `Encoder_GetSpeed()`의 `delta` 계산이 왜 정확한가? (정적 변수 `prev_left/right`의 역할)

**직접 확인할 것:**
```
delta_tick 10ms 동안 몇 tick 나오는가?
→ 600mm/s 목표라면 몇 tick/10ms 인가?
  (600 / 204.2 * 1920 * 0.01 ≈ 56 tick)
```

이 숫자를 손으로 계산해두면 나중에 USART2 출력값이 맞는지 바로 판단할 수 있다.

---

## 2단계: PID (`pid.c` → `motor_control.h`의 게인 정의)

**코드가 3줄짜리 수식임을 먼저 인식하기:**
```c
error      = target - measured          // 현재 얼마나 틀렸나
integral  += error * dt                 // 지금까지 누적된 오차
derivative = (error - prev_error) / dt  // 오차가 얼마나 빠르게 변하는가

output = Kp*error + Ki*integral + Kd*derivative
```

**지금 설정(`Kp=1.0, Ki=0, Kd=0`)으로 손 계산:**
```
target=300, measured=0   → error=300 → output=300 mm/s
target=300, measured=200 → error=100 → output=100 mm/s
target=300, measured=290 → error=10  → output=10  mm/s
```

출력이 mm/s 단위로 `Motor_SetRaw()`에 들어가는 구조임을 이 계산으로 확인한다.

**핵심 질문:**
- `Ki=0`이면 정상 상태에서 오차가 0이 될 수 있는가? (마찰이 있을 때)
- `integral`에 상한이 없으면 Ki를 키울 때 어떤 문제가 생기는가?

---

## 3단계: 모터 (`motor_control.c`)

**읽는 순서:**
```
Motor_Init()          → 초기 상태 확인 (PWM 0, GPIO LOW)
Motor_SetVelocity()   → target만 저장하고 끝? 왜?
Motor_PID_Update()    → PID 출력 → Motor_SetRaw() 연결
Motor_SetRaw()        → 방향(GPIO) + 속도(PWM) 분리 로직
SpeedMMPS_To_PWM()    → mm/s → 0~999 변환 공식 확인
```

`Motor_SetVelocity()`가 PWM을 직접 건드리지 않는 이유가 핵심이다.
target을 저장만 하고 실제 출력은 10ms마다 PID가 결정한다 — 이 분리가 closed-loop의 핵심 구조다.

---

## 4단계: 전체 루프를 `main.c`에서 시간축으로 읽기

```c
while(1) {
    Protocol_Process();       // RPi 명령 → Motor_SetVelocity() 호출
    Motor_CheckTimeout();     // 500ms 무응답 → SoftStop

    if (10ms 경과) {
        speed = Encoder_GetSpeed()   // 측정
        Motor_PID_Update(speed)      // PID → PWM 적용
    }

    if (50ms 경과) { 센서 데이터 RPi 송신 }
    if (500ms 경과) { USART2 디버그 출력 }
}
```

"RPi에서 속도 명령이 오면 어떤 순서로 모터가 반응하는가"를 처음부터 끝까지 추적하면 전체 구조가 한눈에 들어온다.

---

## 실전 팁 — USART2 출력을 기준점으로 삼기

```
[DIAG] rx_pkt:5 err:0  spd L:285.3 R:291.7 mm/s  enc L:1423 R:1418
```

| 항목 | 확인 포인트 |
|---|---|
| `spd` | 목표값에 수렴하는가 → PID 동작 확인 |
| `enc` | 단조 증가하는가 → 엔코더 방향 확인 |
| `rx_pkt` | 증가하는가 → UART 통신 확인 |

코드를 읽고 나서 이 숫자들이 예상과 맞는지 비교하면 이해가 훨씬 빠르다.
