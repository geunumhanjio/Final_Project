# mcu_stm32f401re Quick Reference

## 1. Overview
- **Core**: ARM Cortex-M4 (84 MHz)
- **Memory**: 512KB Flash, 96KB SRAM
- **Role in Project**: 라즈베리파이로부터 UART 명령을 받아 PID 제어를 수행하며, L298N 모터 드라이버에 PWM을 인가하고 IMU(I2C)/Encoder(Timer) 데이터를 주기적으로 취합하는 메인 컨트롤러.

## 2. System Resource Map
- **System Clock**: HSI (16MHz) -> PLL -> 84MHz
- **I2C1**: MPU6050 (IMU 센서) 통신용
- **USART1**: 라즈베리파이 ↔ STM32 바이너리 패킷 통신용 (`115200 8-N-1`, RX Interrupt 활성화)
- **USART2**: PC USB 디버깅용 (printf)

## 3. Timer & PWM Allocation
- **TIM2**: 모터 드라이버 PWM 생성 (Ch1: 왼쪽, Ch2: 오른쪽) / 1kHz 주파수
- **TIM3**: 왼쪽 바퀴 엔코더 카운팅 (Encoder Mode)
- **TIM4**: 오른쪽 바퀴 엔코더 카운팅 (Encoder Mode)

## 4. Pinout & Connections
| Pin Name | Function | Connected To | Remarks |
|---|---|---|---|
| PA0 | TIM2_CH1 | L298N ENA | Left Motor PWM |
| PA1 | TIM2_CH2 | L298N ENB | Right Motor PWM |
| PC0 | GPIO_Output | L298N IN1 | Left Dir 1 |
| PC1 | GPIO_Output | L298N IN2 | Left Dir 2 |
| PC2 | GPIO_Output | L298N IN3 | Right Dir 1 |
| PC3 | GPIO_Output | L298N IN4 | Right Dir 2 |
| PA6 | TIM3_CH1 | Left Enc A | Left Encoder |
| PA7 | TIM3_CH2 | Left Enc B | Left Encoder |
| PB6 | TIM4_CH1 | Right Enc A | Right Encoder |
| PB7 | TIM4_CH2 | Right Enc B | Right Encoder |
| PB8 | I2C1_SCL | MPU6050 SCL | I2C Clock |
| PB9 | I2C1_SDA | MPU6050 SDA | I2C Data |
| PA9 | USART1_TX | RPi GPIO 15 | UART TX to RPi |
| PA10 | USART1_RX | RPi GPIO 14 | UART RX from RPi |
| PA2 | USART2_TX | USB VCP RX | printf Debug |
| PA3 | USART2_RX | USB VCP TX | printf Debug |

## 5. Interrupt Priorities
| Interrupt Source | Vector Name | Preemption Priority | Sub Priority |
|---|---|---|---|
| SysTick Timer | SysTick_IRQn | 0 | 0 |
| USART1 global interrupt | USART1_IRQn | 0 | 0 |
*(Note: Currently, all priorities are set to the CubeMX default of 0. As motor control and communication code are added, these priorities may need to be adjusted to prevent conflicts and ensure real-time performance.)*

## References
- [RM0368 Reference Manual](./mcu_stm32f401_RM0368_reference.pdf)
- [Nucleo Pinout Image](./mcu_stm32f401re_nucleo_pinout.png)
