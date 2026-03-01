# 엔코더 모듈 설정 가이드

## 1. 개요

쿼드러처 엔코더는 모터 축에 부착된 디스크의 회전을 감지하여 바퀴의 회전 방향과 회전수를 측정한다. STM32의 타이머 하드웨어 엔코더 모드를 활용하여 CPU 부하 없이 자동으로 카운트한다.

**용도:** 오도메트리 (이동 거리/속도 추정), 향후 PID 속도 제어에 활용 예정

---

## 2. 쿼드러처 엔코더 기본 개념

쿼드러처 엔코더는 A상, B상 두 신호를 출력하며, MCU는 이를 입력으로 받아 두 신호의 위상차를 이용해 회전 방향을 측정한다.

```
정방향 회전:
  ──┐  ┌──┐  ┌──  : A상
    └──┘  └──┘
──┐  ┌──┐  ┌──    : B상
  └──┘  └──┘  
  A가 B보다 앞섬 → 카운트 증가

역방향 회전:
──┐  ┌──┐  ┌──    : A상
  └──┘  └──┘
  ──┐  ┌──┐  ┌──  : B상
    └──┘  └──┘
  B가 A보다 앞섬 → 카운트 감소
```

- **PPR (Pulses Per Revolution):** 480
- **4체배 (x4):** STM32 TI12 모드에서 A/B 양쪽의 상승/하강 에지 모두 카운트
- **총 분해능:** 480 PPR x 4 = **1920 counts/revolution**

---

## 3. 하드웨어 연결

### 핀 연결

| 엔코더 | 출력 | STM32 핀 | 타이머 | 설명 |
|--------|------|----------|--------|------|
| 왼쪽 | A상 | PA6 | TIM3_CH1 | 왼쪽 바퀴 엔코더 A |
| 왼쪽 | B상 | PA7 | TIM3_CH2 | 왼쪽 바퀴 엔코더 B |
| 오른쪽 | A상 | PB6 | TIM4_CH1 | 오른쪽 바퀴 엔코더 A |
| 오른쪽 | B상 | PB7 | TIM4_CH2 | 오른쪽 바퀴 엔코더 B |

### 전원
- VCC: 3.3V 또는 5V (엔코더 사양에 따라)
- GND: STM32 GND와 공유

---

## 4. CubeMX 설정

### TIM3 (왼쪽 엔코더) / TIM4 (오른쪽 엔코더)

TIM3, TIM4 모두 동일한 설정을 사용한다.

1. Timers → **TIM3** (또는 TIM4) → Combined Channels: **Encoder Mode**
2. Parameter Settings:

```
Encoder Mode:      Encoder Mode TI12 (A/B 양쪽 에지 카운트 = 4체배)
Counter Period:    65535 (16비트 최대값)
Prescaler:         0

Input Capture Channel 1:
  Polarity:        Rising Edge
  IC Selection:    Direct
  Prescaler:       No division
  Filter:          10 (노이즈 필터링)

Input Capture Channel 2:
  Polarity:        Rising Edge
  IC Selection:    Direct
  Prescaler:       No division
  Filter:          10
```

### 왜 Filter=10인가?

노이즈에 의한 오카운트를 방지한다. 필터 값은 입력 신호를 N클럭 동안 안정적인 상태로 유지해야 유효한 에지로 인정하는 설정이다. 모터 진동이 심한 환경에서 필수적이다.

---

## 5. 드라이버 API

### 파일 구조

| 파일 | 역할 |
|------|------|
| `Core/Inc/encoder.h` | 타입 정의, 함수 프로토타입 |
| `Core/Src/encoder.c` | 엔코더 초기화 및 카운트 읽기 |

### API 함수

#### `Encoder_Init(void)`
엔코더 타이머(TIM3, TIM4)를 시작한다. `main.c`에서 `MX_TIM3_Init()`, `MX_TIM4_Init()` 호출 이후에 실행해야 한다.

```c
Encoder_Init();
// 내부: HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
//       HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
```

#### `Encoder_GetCount(Encoder_Select_t encoder)` → `int16_t`
현재 엔코더 카운트 값을 반환한다. 타이머 카운터 레지스터를 직접 읽는다.

```c
int16_t left  = Encoder_GetCount(ENCODER_LEFT);   // TIM3 카운터
int16_t right = Encoder_GetCount(ENCODER_RIGHT);  // TIM4 카운터 (부호 반전)
```

#### `Encoder_ResetCount(Encoder_Select_t encoder)`
엔코더 카운트를 0으로 리셋한다.

```c
Encoder_ResetCount(ENCODER_LEFT);
Encoder_ResetCount(ENCODER_RIGHT);
```

---

## 6. 데이터 해석

### 카운트 ↔ 회전수

```
1920 counts = 1 revolution (480 PPR x 4체배)
```

### 오른쪽 엔코더 부호 반전

`Encoder_GetCount(ENCODER_RIGHT)`는 TIM4 카운터 값에 음수 부호를 적용한다:

```c
return -(int16_t)__HAL_TIM_GET_COUNTER(&htim4);
```

**이유:** 좌우 모터가 대칭으로 장착되어 있어, 로봇이 전진할 때 왼쪽 엔코더는 양수, 오른쪽 엔코더는 음수 방향으로 카운트된다. 부호를 반전하여 **전진 = 양수**로 통일한다.

### 현재 제한사항

| 제한 | 설명 |
|------|------|
| 오버플로우 미처리 | 카운터가 int16_t 범위(-32768~32767)를 넘으면 래핑됨. 장시간 운행 시 주의 |
| 거리 변환 미구현 | counts → mm 변환 없음. 바퀴 둘레 측정 후 구현 필요 |
| 속도 계산 미구현 | 주기적 카운트 차이로 속도 추정 가능하나 아직 미구현 |

---

## 7. main.c 사용 예시

```c
/* USER CODE BEGIN Includes */
#include "encoder.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
Encoder_Init();
printf("Encoder started (TIM3=Left, TIM4=Right)\r\n");
/* USER CODE END 2 */

/* USER CODE BEGIN 3 (while 루프 안) */
printf("L:%6d  R:%6d\r\n",
       Encoder_GetCount(ENCODER_LEFT),
       Encoder_GetCount(ENCODER_RIGHT));
/* USER CODE END 3 */
```

---

## 8. Makefile 수정

CubeMX 재생성 시 `encoder.c`가 C_SOURCES에서 빠질 수 있으므로, 재생성 후 아래 항목이 있는지 확인:

```makefile
C_SOURCES = \
...
Core/Src/encoder.c \
...
```

---

## 9. 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| 카운트가 변하지 않음 | `Encoder_Init()` 미호출 | main.c에서 초기화 확인 |
| 카운트가 항상 양수만 | Encoder Mode가 TI1 또는 TI2 | CubeMX에서 TI12로 변경 |
| 카운트가 불규칙하게 점프 | 노이즈 | Filter 값 증가 (10→15) |
| 전진 시 좌우 부호가 다름 | A/B상 배선이 반대 | 해당 엔코더의 A/B 배선 교환 |
| 오른쪽만 카운트 안 됨 | TIM4 핀 충돌 또는 미설정 | PB6/PB7이 Encoder Mode인지 확인 |

---

## 10. 참고 자료

- `docs/hardware/` — DFROBOT 엔코더 TT 모터 사양서 (480 PPR)
- STM32F4 Reference Manual — TIM encoder mode (Section 18.3.12)
