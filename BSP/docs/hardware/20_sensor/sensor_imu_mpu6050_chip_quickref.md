# sensor_imu_mpu6050 Quick Reference

## 1. Overview
- **Function**: 6-axis MotionTracking device (3-axis Gyroscope + 3-axis Accelerometer)
- **Role in Project**: 로봇의 현재 자세(기울기), 회전 속도, 가속도를 주기적으로(50ms) 읽어 라즈베리파이로 전송(`RSP_IMU` 패킷)하여 시스템 상태 모니터링 및 제어에 사용.

## 2. Key Specifications
- **Operating Voltage**: 3.3V ~ 5V (GY-521 보드에 3.3V 레귤레이터 내장)
- **Communication Interface**: I2C (Standard 100kHz / Fast 400kHz)
- **I2C Device Address**: `0xD0` (Write), `0xD1` (Read) *(Note: AD0 핀이 GND에 연결되었을 때의 8-bit 주소 기준. 7-bit 기준으로는 0x68)*

## 3. Key Register Map (필수 레지스터)
| Register Name | Address (Hex) | Read/Write | Description |
|---|---|---|---|
| SMPLRT_DIV | 0x19 | R/W | Sample Rate Divider (예: 0x07로 설정 시 1kHz) |
| GYRO_CONFIG | 0x1B | R/W | 자이로스코프 측정 범위 설정 (예: ±250°/s) |
| ACCEL_CONFIG| 0x1C | R/W | 가속도계 측정 범위 설정 (예: ±2g) |
| ACCEL_XOUT_H| 0x3B | R | X축 가속도 데이터 시작점 (이곳부터 14바이트 연속 Read 시 6축 데이터 확보) |
| PWR_MGMT_1 | 0x6B | R/W | 전원 관리 (0x00을 써서 Sleep 모드 해제 필수!) |
| WHO_AM_I | 0x75 | R | 센서 연결 확인용 레지스터 (정상 통신 시 `0x68` 반환) |

## 4. Notes & Considerations (코딩 시 주의사항)
1. **Sleep Mode 탈출**: MPU6050은 전원이 인가되면 기본적으로 Sleep 상태입니다. 초기화(Init) 함수에서 가장 먼저 해야 할 일은 `PWR_MGMT_1(0x6B)`에 `0x00`을 기록하는 것입니다.
2. **Burst Read**: X, Y, Z 데이터를 각각 따로 읽으면 I2C 오버헤드가 크고 측정 시점이 틀어집니다. `0x3B`부터 14 bytes를 한 번의 `HAL_I2C_Mem_Read()`로 읽어오는 것이 실무 표준입니다.
3. **Scale Factor 변환**: 센서에서 읽은 `int16_t` Raw 데이터 자체는 물리량이 아닙니다. 현재 프로젝트 백로그에 누락된 부분인데, 나중에 설정된 측정 범위(예: ±2g)에 맞춰 Raw 값에 `16384.0` 등을 나누어 실제 $m/s^2$ 이나 $^\circ/s$ 로 변환하는 상수가 코드상에 정의되어야 합니다.

## References
- [MPU6000 Chip Manual](./sensor_imu_mpu6000_chip.pdf)
- [GY-521 Module Specs](./sensor_imu_gy521_module.pdf)
