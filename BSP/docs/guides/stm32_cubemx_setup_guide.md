# STM32 CubeMX 설정 가이드 - 모터 제어 (TIM2 PWM, GPIO)

## 1. 프로젝트 생성
1. STM32CubeMX 실행
2. New Project → STM32F401 시리즈 선택 (보드에 맞게)
3. 프로젝트 이름: `ROS_Robot_Driver`

---

## 2. 핀 설정 (Pinout & Configuration)

### 📌 모터 PWM 설정 (속도 제어)

**PA0 - TIM2_CH1 (왼쪽 모터 속도)**
- 핀 클릭 → `TIM2_CH1`으로 설정

**PA1 - TIM2_CH2 (오른쪽 모터 속도)**
- 핀 클릭 → `TIM2_CH2`로 설정

**TIM2 설정:**
1. 좌측 메뉴 → `Timers` → `TIM2` 클릭
2. `Clock Source`: Internal Clock
3. `Channel1`, `Channel2`: PWM Generation CH1, CH2
4. Parameter Settings:
   ```
   Prescaler (PSC): 83
   Counter Period (ARR): 999
   → PWM 주파수 = 84MHz / (84 × 1000) = 1kHz
   
   Pulse (CCR1, CCR2): 0 (초기값)
   PWM Mode: PWM mode 1
   ```

---

### 📌 모터 방향 GPIO 설정

**왼쪽 모터 방향:**
- **PC0** → `GPIO_Output` (IN1)
- **PC1** → `GPIO_Output` (IN2)

**오른쪽 모터 방향:**
- **PC2** → `GPIO_Output` (IN3)
- **PC3** → `GPIO_Output` (IN4)

**GPIO 설정:**
1. 각 핀 우클릭 → User Label 지정
   - PC0: `MOTOR_LEFT_IN1`
   - PC1: `MOTOR_LEFT_IN2`
   - PC2: `MOTOR_RIGHT_IN1`
   - PC3: `MOTOR_RIGHT_IN2`

2. GPIO 설정 (System Core → GPIO → PC0~PC3):
   ```
   GPIO output level: Low
   GPIO mode: Output Push Pull
   GPIO Pull-up/Pull-down: No pull-up and no pull-down
   Maximum output speed: Low
   ```

---

### 📌 디버깅용 USART2 설정

**PA2/PA3 - USART2 (PC와 디버깅)**
- PA2: `USART2_TX`
- PA3: `USART2_RX`

**USART2 설정:**
```
Mode: Asynchronous
Baud Rate: 115200 Bits/s
Word Length: 8 Bits
Parity: None
Stop Bits: 1
```

---

## 3. 클럭 설정 (Clock Configuration)

1. Clock Configuration 탭 이동
2. 다음 설정:
   ```
   Input frequency: 8 MHz (HSE 사용 시) 또는 16 MHz (HSI)
   HCLK: 84 MHz (최대 속도)
   APB1 Timer clocks: 84 MHz
   ```

---

## 4. 코드 생성

1. **Project Manager** 탭
   - Project Name: `ROS_Robot_Driver`
   - Toolchain/IDE: `Makefile`
   - Generate peripheral initialization as a pair of '.c/.h' files per peripheral: **체크 ☑️** (printf 디버깅 시)

2. **GENERATE CODE** 버튼 클릭

---

## 5. 생성된 파일 구조

```
Core/
├─ Inc/
│  ├─ main.h
│  ├─ gpio.h
│  ├─ tim.h
│  └─ usart.h
└─ Src/
   ├─ main.c
   ├─ gpio.c
   ├─ tim.c
   └─ usart.c
```

