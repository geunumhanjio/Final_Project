> **담당 역할:** BSP (Board Support Package) — STM32 기반 로봇 하드웨어 제어 펌웨어 개발
> **MCU:** STM32F401RE (NUCLEO-F401RE)

---

## 1. Open-loop → PID Closed-loop 전환

기존에는 RPi에서 받은 속도 명령(mm/s)을 PWM 값으로 직접 변환하는 방식이었다. 
- 하지만 이 방식은 속도 명령과 PWM 값이 매칭되지 않고, 
- 모터에 부하가 걸리거나 배터리 전압이 변해도 피드백이 없으니 실제 속도와 명령 속도가 달라져도 알 수 없는 구조였다.

이번에 엔코더 피드백을 활용한 PID 제어로 전환했다.

### 제어 구조 변화

```
[Before — Open-loop]
RPi CMD_VELOCITY (mm/s) → mm/s를 PWM으로 직접 변환 → L298N 출력
(실제 속도 확인 없음)

[After — Closed-loop]
RPi CMD_VELOCITY (mm/s) ← 목표 속도 설정
  ↓ 10ms마다
Encoder_GetSpeed() → 실제 속도 측정
  ↓
PID_Update() → 오차 보정
  ↓
Motor_PID_Update() → PWM 출력
  ↓ (피드백 루프)
```

### 엔코더 속도 계산

`Encoder_GetSpeed()`를 새로 추가했다. 10ms 주기로 타이머 카운터의 변화량을 읽어 mm/s 단위로 반환한다.

```
speed (mm/s) = (delta_tick / ENCODER_PPR) × 바퀴둘레(mm) / 0.01s
             = (delta_tick / 1920) × 204.2 / 0.01
```

PPR(Pulses Per Revolution) = 1920. 바퀴 둘레는 `WHEEL_CIRCUMFERENCE_MM` 매크로로 분리해 실측 후 수정 가능하게 했다.

### PID 모듈 분리

`pid.c` / `pid.h`를 독립 모듈로 구현했다.

```c
typedef struct {
    float Kp, Ki, Kd;
    float integral;
    float prev_error;
} PID_t;

float PID_Update(PID_t *pid, float setpoint, float measured, float dt);
```

### 메인 루프 연동

`main.c` while 루프에 10ms 타이밍 블록을 추가해 제어 주기를 보장한다.

```c
if (HAL_GetTick() - last_pid_tick >= 10) {
    last_pid_tick = HAL_GetTick();
    float spd_L = Encoder_GetSpeed(MOTOR_LEFT);
    float spd_R = Encoder_GetSpeed(MOTOR_RIGHT);
    Motor_PID_Update(MOTOR_LEFT, spd_L);
    Motor_PID_Update(MOTOR_RIGHT, spd_R);
}
```


### 현재 상태 및 남은 과제

PID 제어 루프 자체는 완성됐지만, 실제 하드웨어에서 동작을 확인하고 게인을 조정하는 작업이 남아 있다.

| 항목 | 상태 |
|------|------|
| PID 제어 루프 구현 | ✅ |
| PID 게인 (Kp, Ki, Kd) 튜닝 | 🚧 실험 필요 |

---

## 2. 협업을 위한 펌웨어 배포 체계 구축

ROS 팀과 BSP 팀이 하드웨어를 공유하고 있는 환경에서, ROS 팀이 BSP 팀에 요청하지 않고도 STM보드에 직접 적합한 버전의 펌웨어를 올려 테스트할 수 있도록 배포 체계를 구축했다. 

```
[ BSP 팀 ] — 빌드 → .bin 파일 RPi에 업로드 및 공지
      ↓
[ ROS 팀 ] — 스크립트만 실행 → STM32 플래시 완료
```

### 구성

| 파일 | 역할 |
|------|------|
| `flash_stm32.sh` | ST-Link 연결 확인 → 플래시 → 결과 출력 자동화 |
| `firmware_flashing_guide.md` | 연결 방법, 사용법, 트러블슈팅, 버전 이력 |
| `V1/ROS_Robot_Driver.bin` | v0.1.0 — Open-loop |
| `V2/ROS_Robot_Driver.bin` | v0.2.0 — PID 기본 구현 |

### 사용 방법

```bash
cd ~/firmware_version
sudo ./flash_stm32.sh V2/ROS_Robot_Driver.bin
```

스크립트가 ST-Link 연결 여부를 먼저 확인하고, 장치가 없으면 명확한 에러 메시지를 출력한다. 플래시 성공 시 STM32가 자동으로 리셋되어 새 펌웨어로 부팅된다.

