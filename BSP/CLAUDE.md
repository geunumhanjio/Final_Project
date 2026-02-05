# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview 3
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

## Architecture  ☑️☑️

**Layered structure:**

- **Application** (`Core/Src/main.c`) — Entry point, peripheral init (GPIO, TIM2, USART2), main loop. Currently a skeleton awaiting motor control integration.
- **Motor Control Library** (`mass/motor_setup/motor_control.h/.c`) — Custom abstraction over L298N driver. Not yet integrated into `Core/`. Provides `Motor_Init()`, `Motor_Forward()`, `Motor_Stop()`, etc.
- **HAL Drivers** (`Drivers/STM32F4xx_HAL_Driver/`) — STMicroelectronics HAL for STM32F4. Do not edit; these are CubeMX-managed.
- **CMSIS** (`Drivers/CMSIS/`) — ARM Cortex-M4 core definitions and startup code.

**CubeMX-generated code:** `main.c`, `stm32f4xx_hal_msp.c`, `stm32f4xx_it.c`, and the Makefile are generated from `ROS_Robot_Driver.ioc`. User code must go between `/* USER CODE BEGIN */` and `/* USER CODE END */` markers or it will be overwritten on regeneration.

## Hardware Pin Mapping   ☑️☑️

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
| 라즈베리파이 4 | GPIO 14 (Phy 8) | PA9 | USART1_TX |  |
|  | GPIO 15 (Phy 10) | PA10 | USART1_RX |  |
| PC | USB | PA2 / PA3 | USART2 (VCP) | printf를 활용한 디버깅 |

## Key Configuration   ☑️☑️☑️

- **System clock:** 84 MHz (HSI 16 MHz → PLL: M=16, N=336, P=4)
- **PWM:** 1 kHz on TIM2 (prescaler=83, period=999), 1000-step resolution
- **Memory:** 512 KB Flash, 96 KB RAM, 512B heap, 1024B stack
- **Linker script:** `STM32F401XX_FLASH.ld`

## Motor Control API (mass/motor_setup/) ☑️☑️ 

```c
Motor_Init();                          // Start PWM on TIM2 CH1 & CH2
Motor_SetSpeed(MOTOR_LEFT, 50);        // 0-100% speed
Motor_SetDirection(MOTOR_LEFT, MOTOR_DIR_FORWARD);
Motor_Forward(speed);                  // Both motors forward
Motor_Backward(speed);                 // Both motors backward
Motor_TurnLeft(speed);                 // In-place left turn
Motor_TurnRight(speed);                // In-place right turn
Motor_Stop();                          // Brake (IN1=IN2=1)
Motor_Coast();                         // Coast (IN1=IN2=0)
```

L298N truth table: IN1=1/IN2=0 → forward, IN1=0/IN2=1 → reverse, both high → brake, both low → coast.

## Documentation   ☑️☑️

Reference materials live in `mass/`:
- `mass/motor_setup/` — Motor control library, integration guide, CubeMX setup guide
- `mass/project_structure/` — Project organization, git workflow, quick start checklist
- Hardware docs (MPU6050 datasheet, motor specs, system diagram)

## Planned Features (Not Yet Implemented)

- Encoder integration (TIM3/TIM4 encoder mode, 1920 PPR)
- MPU6050 IMU over I2C
- ROS topic publishing (/odom, /imu) and /cmd_vel subscription over UART
- PID speed control
