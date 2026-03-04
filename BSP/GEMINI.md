# GEMINI.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Aiming to bolster industrial safety, this project introduces an integrated disaster management system that leverages Hanwha Vision’s AI-powered CCTV for real-time hazard detection. By seamlessly synchronizing detected event coordinates with ROS-based autonomous robots, the platform facilitates immediate site intervention
As a driver developer responsible for the system’s lowest-level hardware operations, I implement the robot’s physical movement using an STM32 NUCLEO-F401RE MCU interfaced with various sensors and actuators. My key tasks include collecting encoder data for odometry calculation, acquiring attitude control data through IMU sensor communication, and controlling motors via the L298M motor driver according to system commands. Communication with the ROS environment is relayed through a Raspberry Pi 4, with which I interface via serial.communication

## Build Commands

```bash
make              # Build ELF, HEX, and BIN to build/
make clean        # Remove build directory
make -j$(nproc)   # Parallel build
```

Outputs: `build/ROS_Robot_Driver.elf`, `.hex`, `.bin`
                                 
**Toolchain:** `arm-none-eabi-gcc` (ARM embedded GCC). No host-side tests exist; testing is done on hardware.

**Flash via OpenOCD:**
```bash
openocd -f "board/st_nucleo_f4.cfg" -c "program build/ROS_Robot_Driver.elf verify reset"
```

## Architecture

**Layered structure:**

- **Application** (`Core/Src/main.c`) — Entry point, 주변장치 초기화 (GPIO, TIM2, USART1, USART2 등), 메인 루프에서 `Protocol_Process()`(UART 패킷 파싱), `Motor_CheckTimeout()`, `Motor_PID_Update()`(10ms 주기 PID 속도 제어), 50ms 센서 데이터 전송 반복 실행.
- **UART Protocol** (`Core/Src/uart_protocol.c`, `Core/Inc/uart_protocol.h`) — RPi↔STM32 바이너리 패킷 통신. 인터럽트 기반 바이트 수신 → 상태머신 파싱 → 명령 디스패치. 자세한 내용은 아래 UART Protocol 섹션 참조.
- **Motor Control** (`Core/Src/motor_control.c`, `Core/Inc/motor_control.h`) — L298N 모터 드라이버 및 PID 제어 추상화. `Motor_SetVelocity()`, `Motor_EmergencyStop()`, `Motor_SoftStop()`, `Motor_CheckTimeout()`, `Motor_ReleaseEmergency()`, `Motor_PID_Update()` 제공. 가속도 제한(PID 대체) 및 명령 타임아웃(500ms) 내장.
- **HAL Drivers** (`Drivers/STM32F4xx_HAL_Driver/`) — STMicroelectronics HAL for STM32F4. Do not edit; these are CubeMX-managed.
- **CMSIS** (`Drivers/CMSIS/`) — ARM Cortex-M4 core definitions and startup code.

**CubeMX-generated code:** `main.c`, `stm32f4xx_hal_msp.c`, `stm32f4xx_it.c`, `usart.c`, `tim.c`, `gpio.c`, Makefile은 `ROS_Robot_Driver.ioc`에서 생성됨. 사용자 코드는 반드시 `/* USER CODE BEGIN */` ~ `/* USER CODE END */` 사이에 작성해야 CubeMX 재생성 시 보존됨.

## Hardware Pin Mapping 

| **부품** | **부품 핀** | **STM32 핀** | **CubeMX 설정** | **설명** |
| --- | --- | --- | --- | --- |
| 모터드라이버 (L298N) | ENA | PA0 | TIM2_CH1 (PWM Generation) | 왼쪽 모터 속도 제어 |
|  | ENB | PA1 | TIM2_CH2 (PWM Generation) | 오른쪽 모터 속도 제어 |
|  | IN1 | PC0 | GPIO_Output | 왼쪽 모터 방향 제어 1 |
|  | IN2 | PC1 | GPIO_Output | 왼쪽 모터 방향 제어 2 |
|  | IN3 | PC2 | GPIO_Output | 오른쪽 모터 방향 제어 1 |
|  | IN4 | PC3 | GPIO_Output | 오른쪽 모터 방향 제어 2 |
| 휠 엔코더 | 왼쪽 A상 | PA6 | TIM3_CH1 (Encoder Mode) | 왼쪽 바퀴 회전수 카운팅 |
|  | 왼쪽 B상 | PA7 | TIM3_CH2 (Encoder Mode) | 왼쪽 바퀴 회전수 카운팅 |
|  | 오른쪽 A상 | PB6 | TIM4_CH1 (Encoder Mode) | 오른쪽 바퀴 회전수 카운팅 |
|  | 오른쪽 B상 | PB7 | TIM4_CH2 (Encoder Mode) | 오른쪽 바퀴 회전수 카운팅 |
| IMU 센서 (MPU6050) | SCL | PB8 | I2C1_SCL | I2C 클락 라인 |
|  | SDA | PB9 | I2C1_SDA | I2C 데이터 라인 |
| 라즈베리파이 4 | GPIO 14 (Phy 8, TX) | PA10 | USART1_RX |  |
|  | GPIO 15 (Phy 10, RX) | PA9 | USART1_TX |  |
| PC | USB | PA2 / PA3 | USART2 (VCP) | printf를 활용한 디버깅 |

## Key Configuration  

- **System clock:** 84 MHz (HSI 16 MHz → PLL: M=16, N=336, P=4)
- **PWM:** 1 kHz on TIM2 (prescaler=83, period=999), 1000-step resolution
- **Memory:** 512 KB Flash, 96 KB RAM, 512B heap, 1024B stack
- **Linker script:** `STM32F401XX_FLASH.ld`


L298N truth table: IN1=1/IN2=0 → forward, IN1=0/IN2=1 → reverse, both high → brake, both low → coast.

## UART Protocol (RPi ↔ STM32)

USART1 (PA9 TX, PA10 RX), 115200 baud, 인터럽트 기반 수신.

**패킷 구조:**
```
[Header1: 0xFF][Header2: 0xFE][CMD][LEN][DATA (0~8 bytes)][CHECKSUM]
```
- Checksum: CMD ^ LEN ^ DATA[0] ^ ... ^ DATA[n-1] (XOR)
- 최소 패킷: 5바이트 (데이터 없을 때), 최대: 13바이트

**명령 (RPi → STM32, 0x01~0x7F):**

| CMD | 코드 | LEN | DATA | 설명 |
|-----|------|-----|------|------|
| `CMD_VELOCITY` | 0x01 | 4 | [vL_low, vL_high, vR_low, vR_high] | 양쪽 바퀴 속도 설정 (int16, Little-Endian, mm/s) |
| `CMD_STOP` | 0x02 | 0 | — | 부드러운 정지 (coast) |
| `CMD_ESTOP` | 0x03 | 0 | — | 비상 정지 (brake) |
| `CMD_RELEASE` | 0x04 | 0 | — | 비상 정지 해제 |

**응답 (STM32 → RPi, 0x80~0xFF):** 50ms 주기로 전송 중

| RSP | 코드 | 설명 |
|-----|------|------|
| `RSP_ODOM` | 0x81 | 오도메트리 데이터 |
| `RSP_IMU` | 0x82 | IMU 데이터 |

**구현 흐름:** `USART1_IRQHandler` → `HAL_UART_IRQHandler` → `HAL_UART_RxCpltCallback` → `Protocol_FeedByte` (ISR 상태머신) → `ready_packet` 버퍼에 저장 → 메인루프에서 `Protocol_Process()` → `Protocol_Dispatch()` → 모터 제어 함수 호출

## Motor Control

**속도 범위:** -600 ~ +600 mm/s (int16_t), PWM 0~999 (1000단계)

**주요 동작:**
- **속도 제어:** 엔코더 피드백을 활용한 PID 제어 (`Motor_PID_Update()` 10ms 실행)
- **명령 타임아웃:** 500ms간 새 명령 없으면 자동 SoftStop (안전장치)
- **비상 정지:** `Motor_EmergencyStop()` 호출 시 즉시 브레이크, `Motor_ReleaseEmergency()` 전까지 모든 속도 명령 무시

## Documentation

Reference materials live in `docs/`:
- `docs/architecture.md` — 시스템 아키텍처 (블록 다이어그램, SW 레이어, 타이밍)
- `docs/guides/` — 모듈별 셋업 가이드 (모터, 엔코더, IMU, CubeMX)
- `docs/protocol/` — UART 통신 프로토콜 명세
- `docs/hardware/` — 데이터시트, 회로도

## Implementation Status

**구현 완료:**
- Encoder 기본 구현 (TIM3/TIM4 encoder mode, 1920 PPR) — 카운트 읽기/리셋 동작 및 `Encoder_GetSpeed()`를 통한 속도(mm/s) 계산 완료
- 속도 제어 루프 추가: 10ms 주기로 `Motor_PID_Update()` 실행 완료 (`pid.c`, `pid.h` 추가)
- MPU6050 IMU over I2C — 6축 raw 데이터 읽기 동작
- STM32 → RPi 응답 패킷 송신 (RSP_ODOM, RSP_IMU) — 50ms 주기로 엔코더/IMU 데이터 전송 중

**미구현:**
- 오도메트리 계산 (엔코더 카운트 → 이동 거리/각도 변환)
- IMU 물리량 변환 (raw → deg/s, m/s²)
- 엔코더 오버플로우 처리
- ROS topic publishing (/odom, /imu) — RPi 측 노드에서 구현 필요

## Git

### Git 가이드 요청사항
사용자는 Git 초보자이다. 작업 중 Git 관련 동작(커밋, 브랜치 생성/전환, 병합, PR 등)이 필요할 때마다 **왜 하는지, 무엇을 하는지** 간단히 설명한 뒤 실행한다. 전문 용어를 사용할 때는 한 줄 설명을 덧붙인다.

### 브랜치 전략

| 브랜치 명 | 설명 | 예시 |
| --- | --- | --- |
| `main` | master 브랜치 | `main` |
| `<역할>_prod` | 각 역할의 prod 브랜치 | `BSP_prod`, `BE_prod` |
| `<역할>_dev` | 각 역할의 dev 브랜치 | `BSP_dev`, `BE_dev` |
| `<역할>/feat/#이슈번호` | feat 브랜치 | `BSP/feat/#12` |
| `<역할>/fix/#이슈번호` | fix 브랜치 | `BSP/fix/#13` |

### 커밋 컨벤션
ㄴ
- 커밋은 기능별로 나눠서 순서대로 진행한다. 

| 깃모지 | 코드 | 커밋내용 |
| --- | --- | --- |
| :sparkles: | `:sparkles:` | [Feat] 회원 가입 기능 구현 |
| :bug: | `:bug:` | [Fix] 버그 수정 |
| :memo: | `:memo:` | [Docs] 문서 관련 |
| :card_file_box: | `:card_file_box:` | [Mod] 파일 수정 |
| :art: | `:art:` | [Style] 스타일 변경 |
| :test_tube: | `:test_tube:` | [Test] 테스트 관련 코드 |
| :zap: | `:zap:` | [Perf] 성능 개선 |
| :see_no_evil: | `:see_no_evil:` | [Git] gitignore 추가 |
| :recycle: | `:recycle:` | [Refactor] 코드 리팩토링 |

**커밋 메시지 형식:** `:깃모지코드: [타입] 설명`

예시: `:sparkles: [Feat] 모터 제어 라이브러리 추가`
