# driver_l298n Quick Reference

## 1. Overview
- **Function**: Dual H-Bridge Motor Driver
- **Role in Project**: STM32의 제어 명령(방향 및 PWM)을 바탕으로 로봇의 12V 좌/우 DC 모터에 실제 구동 전력을 공급.

## 2. Key Specifications
- **Operating Voltage (Logic)**: 5V
- **Operating Voltage (Motor)**: 최대 35V (프로젝트에서는 12V 배터리 사용)
- **Max Current**: 채널당 2A (Peak 3A)
- **Interface**: 
  - **Left Motor**: ENA(PA0, PWM), IN1(PC0), IN2(PC1)
  - **Right Motor**: ENB(PA1, PWM), IN3(PC2), IN4(PC3)
  - **PWM Config**: TIM2, 1kHz, 1000-step resolution (0~999)

## 3. Truth Table & Control
| IN1 (PC0) | IN2 (PC1) | ENA (PA0, PWM) | Left Motor State | Description |
|---|---|---|---|---|
| High (1) | Low (0) | 0 ~ 999 | Forward | 전진 (PWM 수치 비례) |
| Low (0) | High (1) | 0 ~ 999 | Reverse | 후진 (PWM 수치 비례) |
| High (1) | High (1) | X | Fast Stop | 급정지 (Brake) |
| Low (0) | Low (0) | X | Free Running Stop | 관성 정지 (Coast, Soft Stop) |

*(Note: 오른쪽 모터(IN3, IN4, ENB)도 위 논리와 완벽히 동일합니다.)*

## 4. Notes & Considerations (코딩 시 주의사항)
1. **Voltage Drop (전압 강하)**: L298N 내부 BJT 특성상 약 1.4~2.0V의 전압 강하가 발생합니다. 배터리가 12V라도 모터에는 최대 10V만 전달됩니다.
2. **Deadzone (데드존)**: 모터 자체 정지 마찰력과 결합되어, PWM 값이 너무 낮으면(예: 0~200) 돌지 않고 웅웅거리기만 할 수 있습니다. 코드에 `MIN_PWM` 상수를 설정해야 합니다. (자세한 내용은 `notes/motor_pid_control_guide.md` 참고)
3. **Heat Sink**: 1A 이상 연속 구동 시 발열이 심하므로 방열판 상태를 체크해야 합니다.

## References
- [L298 Chip Datasheet](./driver_l298n_chip_datasheet.pdf)
- [Module Pinout Image](./driver_l298n_module_pinout.png)
