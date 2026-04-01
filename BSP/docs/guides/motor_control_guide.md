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

**Include 추가 (상단)**
```c
/* USER CODE BEGIN Includes */
#include "motor_control.h"
#include <stdio.h>
/* USER CODE END Includes */
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

## 6. API 레퍼런스

### `Motor_Init(void)` → `Motor_Status_t`
모터 드라이버를 초기화한다. TIM2 PWM 시작, 모든 GPIO LOW, PWM 0으로 설정.
```c
if (Motor_Init() == MOTOR_OK) { /* 성공 */ }
```

### `Motor_SetVelocity(int16_t left_mmps, int16_t right_mmps)` → `Motor_Status_t`
양쪽 바퀴 속도를 설정한다. 가속도 제한이 자동 적용되며, 명령 타임아웃이 리셋된다.
- **범위:** -600 ~ +600 mm/s (int16_t)
- **양수:** 전진, **음수:** 후진
- **비상 정지 중:** `MOTOR_ERROR` 반환, 명령 무시
```c
Motor_SetVelocity(300, 300);   // 직진 300mm/s
Motor_SetVelocity(200, -200);  // 제자리 회전
```

### `Motor_EmergencyStop(void)` → `Motor_Status_t`
즉시 브레이크 모드로 정지한다 (IN1=1, IN2=1). `Motor_ReleaseEmergency()` 호출 전까지 모든 속도 명령이 무시된다.

### `Motor_SoftStop(void)` → `Motor_Status_t`
Coast 모드로 정지한다 (IN1=0, IN2=0). 모터가 관성으로 멈춘다. 비상 정지 중에는 작동하지 않는다.

### `Motor_CheckTimeout(void)` → `bool`
마지막 속도 명령 이후 500ms가 경과했는지 확인한다. 타임아웃 시 `Motor_SoftStop()`을 자동 호출하고 `true`를 반환한다. 메인 루프에서 매번 호출해야 한다.

### `Motor_ReleaseEmergency(void)` → `Motor_Status_t`
비상 정지 플래그를 해제하고 Coast 모드로 전환한다. 이후 `Motor_SetVelocity()` 명령을 다시 받을 수 있다.

---

## 7. 안전 기능

### 가속도 제한
- 호출 시마다 속도 변화량을 **최대 200mm/s**로 제한
- 예: 0 → 600mm/s 명령 시 실제로는 0 → 200 → 400 → 600으로 점진 가속
- `MAX_ACCEL_MMPS_PER_10MS` 매크로로 조정 가능

### 명령 타임아웃 (Watchdog)
- 500ms간 새로운 `CMD_VELOCITY` 명령이 없으면 자동 정지 (`Motor_SoftStop()`)
- RPi 통신 단절 시 로봇이 멈추도록 하는 안전장치
- `MOTOR_CMD_TIMEOUT_MS` 매크로로 조정 가능

### 비상 정지
- `CMD_ESTOP` 수신 시 즉시 브레이크 (IN1=IN2=HIGH, PWM=0)
- 비상 정지 해제(`CMD_RELEASE`) 전까지 모든 속도 명령 무시
- 비상 정지 중에는 타임아웃 체크도 비활성

### 데드존 참고사항
- 낮은 속도 (약 100mm/s 이하)에서는 모터 토크가 부족하여 바퀴가 돌지 않을 수 있음
- 이는 L298N + DC 모터의 물리적 특성이며, 실측을 통해 최소 동작 속도를 파악해야 함
- 향후 PID 제어 도입 시 적분기가 데드존을 보상할 수 있음

---

## 8. 테스트

RPi에서 UART 패킷을 전송하여 모터를 테스트할 수 있다.

### Python 테스트 스크립트
`tools/rpi_motor_test.py`를 Raspberry Pi에서 실행:
```bash
python3 tools/rpi_motor_test.py
```
이 스크립트는 시리얼 포트를 통해 CMD_VELOCITY, CMD_STOP, CMD_ESTOP 패킷을 전송한다.

### 수동 테스트 순서
1. STM32에 펌웨어 플래시
2. RPi와 UART 배선 연결 (TX↔RX 교차, GND 공유)
3. 테스트 스크립트 실행 또는 직접 패킷 전송
4. USART2 시리얼 모니터로 디버그 출력 확인

---

**작성:** 김지오
**날짜:** 2026-02-04
**버전:** 1.1
