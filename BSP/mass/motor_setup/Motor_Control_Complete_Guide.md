# STM32 보드 - 모터 제어 사용 가이드

## 📋 목차
1. [하드웨어 연결](#1-하드웨어-연결)
2. [CubeMX 설정](#2-cubemx-설정)
3. [코드 통합](#3-코드-통합)
4. [빌드 및 업로드](#4-빌드-및-업로드)
5. [테스트](#5-테스트)
6. [트러블슈팅](#6-트러블슈팅)

---

## 1. 하드웨어 연결

### 사용 부품
- STM32 F401RE-Nucleo 보드
- L298N 모터 드라이버 x1
- DC 모터 x2
- Li-ion 18650 배터리 3.7V x2 (직렬 연결)


### 핀 연결

| **부품** | **부품핀** | **STM32 핀** | **CubeMX 설정** | 설명 |
| --- | --- | --- | --- | --- |
| 모터드라이버 (L298N) | ENA | PA0 | TIM2_CH1 (PWM Generation) | 왼쪽 속도 |
|  | ENB | PA1 | TIM2_CH2 (PWM Generation) | 오른쪽 속도 |
|  | IN1 | PC0 | GPIO_Output | 왼쪽 방향 1 |
|  | IN2 | PC1 | GPIO_Output | 왼쪽 방향 2 |
|  | IN3 | PC2 | GPIO_Output | 오른쪽 방향 1 |
|  | IN4 | PC3 | GPIO_Output | 오른쪽 방향 2 |

#### ⚠️ 배터리와 STM보드의 Gnd를 서로 연결해야함

---

## 2. CubeMX 설정

**자세한 설정은 `STM32_CubeMX_Setup_Guide.md` 참조**

핵심 설정:
- ✅ TIM2_CH1, CH2 → PWM Generation
- ✅ PC0~PC3 → GPIO Output
- ✅ USART2 → Async (115200 baud)
- ✅ 클럭: 84 MHz

---

## 3. 코드 통합

### 3.1 파일 추가
```
프로젝트/
├─ Core/
│  ├─ Inc/
│  │  └─ motor_control.h  ← 추가
│  └─ Src/
│     └─ motor_control.c  ← 추가
```

### 3.2 Makefile 수정 
``` 
C_SOURCES =  \
Core/Src/motor_control.c  ← 추가 
```

### 3.2 main.c 수정

**1) Include 추가 (상단)**
```c
/* USER CODE BEGIN Includes */
#include "motor_control.h"
#include <stdio.h>
/* USER CODE END Includes */
```

**2) 초기화 코드 (main 함수 내)**
```c
/* USER CODE BEGIN 2 */
if (Motor_Init() == MOTOR_OK) {
    printf("Motor OK!\r\n");
}
/* USER CODE END 2 */
```

**3) 메인 루프 (테스트 코드)**
```c
/* USER CODE BEGIN 3 */
while (1)
{
    // 예제: 2초 전진 → 1초 정지 → 2초 후진 → 1초 정지
    Motor_Forward(50);
    HAL_Delay(2000);
    
    Motor_Stop();
    HAL_Delay(1000);
    
    Motor_Backward(50);
    HAL_Delay(2000);
    
    Motor_Stop();
    HAL_Delay(1000);
}
/* USER CODE END 3 */
```

---

## 4. 빌드 및 업로드

### 빌드
```bash
# 빌드
make

# 클린
make clean

# 빌드 후 Bin 파일 위치
# build/[바이너리_이미지_이름].bin
```

### ST-Link 도구 설치
```
# 설치 
sudo apt update
sudo apt install stlink-tools

# 설치 확인
st-flash --version
```

### 로드 
```
st-flash write build/[바이너리_이미지_이름].bin 0x8000000
```

---

## 5. 기본 동작 확인

### 5.1 전진: 방향과 속도 체크

```c
// 테스트 1: 전진 50% 속도
Motor_Forward(50);
HAL_Delay(2000);
Motor_Stop();
```

**확인 사항:**
- ☑️ 모터 2개가 같은 방향으로 회전
- ☑️ 로봇이 직진 (또는 모터 단독 회전)
- ☑️ 속도 변화 시 모터 속도 변화

### 5.2 방향 테스트: 전진, 후진, 좌회전, 우회전

```c
// 전진
Motor_Forward(50);
HAL_Delay(1000);
Motor_Stop();

// 후진
Motor_Backward(50);
HAL_Delay(1000);
Motor_Stop();

// 좌회전
Motor_TurnLeft(40);
HAL_Delay(1000);
Motor_Stop();

// 우회전
Motor_TurnRight(40);
HAL_Delay(1000);
Motor_Stop();
```

### 5.3 시리얼 모니터 확인

(⚠️ printf 리다이렉션 설정 필요)

**PC 연결:**
- PC에서 시리얼 터미널 열기 
- 115200 baud로 설정

**출력 예:**
```
Motor OK!
Forward 50%...
Stop...
Backward 30%...
```

---

**작성:** 김지오  
**날짜:** 2026-02-04  
**버전:** 1.0
