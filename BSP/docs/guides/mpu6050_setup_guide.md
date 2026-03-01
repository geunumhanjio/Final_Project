# MPU6050 IMU 센서 설정 가이드

## 1. 개요

MPU6050은 3축 가속도계 + 3축 자이로스코프가 통합된 6축 IMU 센서다. I2C 통신으로 STM32와 연결하여 로봇의 자세(기울기, 회전 속도)를 측정한다.

**용도:** 오도메트리 보정, 자세 제어에 활용 예정

---

## 2. 하드웨어 연결

### 핀 연결

| MPU6050 (GY-521) | STM32 (NUCLEO-F401RE) | 설명 |
|---|---|---|
| VCC | 3.3V | 전원 |
| GND | GND | 접지 |
| SCL | PB8 | I2C1 클락 |
| SDA | PB9 | I2C1 데이터 |
| AD0 | GND (또는 미연결) | I2C 주소 = 0x68 |

### 주의사항
- **반드시 납땜할 것** — 핀 헤더를 납땜하지 않고 꽂기만 하면 접촉 불량으로 I2C 통신이 안 됨
- GY-521 보드에 I2C 풀업 저항이 내장되어 있으므로 외부 풀업 불필요

---

## 3. CubeMX 설정

### I2C1 활성화

1. Connectivity → **I2C1** → Mode: **I2C**
2. 핀 확인: PB8 (SCL), PB9 (SDA)
3. Parameter Settings:
   ```
   I2C Speed Mode: Fast Mode
   I2C Clock Speed: 400000 Hz (400 kHz)
   ```

### 코드 재생성

Generate Code 실행 후 자동 생성되는 파일:
- `Core/Src/i2c.c` — `MX_I2C1_Init()` 함수
- `Core/Inc/i2c.h` — `hi2c1` extern 선언
- `Makefile`에 `stm32f4xx_hal_i2c.c`, `stm32f4xx_hal_i2c_ex.c` 추가됨

---

## 4. 드라이버 구현

### 파일 구조

| 파일 | 역할 |
|---|---|
| `Core/Inc/mpu6050.h` | 레지스터 정의, 데이터 구조체, 함수 프로토타입 |
| `Core/Src/mpu6050.c` | I2C 통신으로 센서 초기화 및 데이터 읽기 |

### 주요 레지스터

| 레지스터 | 주소 | 용도 |
|---|---|---|
| WHO_AM_I | 0x75 | 센서 ID 확인 (값: 0x68) |
| PWR_MGMT_1 | 0x6B | 전원 관리 (SLEEP 해제) |
| SMPLRT_DIV | 0x19 | 샘플 레이트 분주비 |
| CONFIG | 0x1A | DLPF(디지털 저역 통과 필터) 설정 |
| GYRO_CONFIG | 0x1B | 자이로 측정 범위 |
| ACCEL_CONFIG | 0x1C | 가속도 측정 범위 |
| ACCEL_XOUT_H | 0x3B | 가속도 데이터 시작 주소 |

### 초기화 순서 (MPU6050_Init)

1. **WHO_AM_I(0x75) 읽기** → 0x68이면 센서 연결 확인
2. **PWR_MGMT_1에 0x00 쓰기** → SLEEP 모드 해제
3. **SMPLRT_DIV에 0x07 쓰기** → 샘플레이트 = 1kHz / (1+7) = 125Hz
4. **CONFIG에 0x00 쓰기** → DLPF 비활성
5. **GYRO_CONFIG에 0x00 쓰기** → 자이로 범위 +/-250 deg/s
6. **ACCEL_CONFIG에 0x00 쓰기** → 가속도 범위 +/-2g

### 데이터 읽기 (MPU6050_ReadAll)

ACCEL_XOUT_H(0x3B)부터 14바이트 연속 읽기:

```
[AccX_H][AccX_L][AccY_H][AccY_L][AccZ_H][AccZ_L][Temp_H][Temp_L][GyX_H][GyX_L][GyY_H][GyY_L][GyZ_H][GyZ_L]
```

- 빅엔디안 → int16_t 변환: `(buf[0] << 8) | buf[1]`
- 온도 데이터(buf[6..7])는 현재 사용하지 않음

### 데이터 해석

| 설정 | 감도(LSB) | 의미 |
|---|---|---|
| 가속도 +/-2g | 16384 LSB/g | 정지 시 Z축 ≈ 16384 (= 1g) |
| 자이로 +/-250 deg/s | 131 LSB/(deg/s) | 정지 시 ≈ 0 (약간의 드리프트 정상) |

---

## 5. main.c 사용 예시

```c
/* USER CODE BEGIN Includes */
#include "mpu6050.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
if (MPU6050_Init() == HAL_OK) {
    printf("MPU6050 initialized!\r\n");
} else {
    printf("MPU6050 init failed! Check wiring.\r\n");
}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 (while 루프 안) */
MPU6050_Data_t imu;
if (MPU6050_ReadAll(&imu) == HAL_OK) {
    printf("AX:%6d AY:%6d AZ:%6d GX:%6d GY:%6d GZ:%6d\r\n",
           imu.accel_x, imu.accel_y, imu.accel_z,
           imu.gyro_x, imu.gyro_y, imu.gyro_z);
}
/* USER CODE END 3 */
```

---

## 6. Makefile 수정

CubeMX 재생성 시 `mpu6050.c`가 C_SOURCES에서 빠질 수 있으므로, 재생성 후 아래 항목이 있는지 확인:

```makefile
C_SOURCES = \
...
Core/Src/mpu6050.c \
...
```

---

## 7. 트러블슈팅

| 증상 | 원인 | 해결 |
|---|---|---|
| `init failed` + I2C 스캔에 장치 없음 | 배선 문제 또는 납땜 안 됨 | 핀 헤더 납땜, 배선 재확인 |
| `init failed` + 0x69에서 장치 발견 | AD0 핀이 HIGH | AD0를 GND에 연결 |
| AX/AY/AZ 전부 0 | SLEEP 모드 미해제 | PWR_MGMT_1에 0x00 쓰기 확인 |
| 값이 계속 동일 | 데이터 읽기 실패 | I2C 속도 낮춰보기 (100kHz) |

### I2C 버스 스캔 (디버그용)

센서가 감지되지 않을 때 아래 코드로 버스에 연결된 장치를 확인할 수 있다:

```c
printf("I2C bus scan...\r\n");
for (uint8_t addr = 1; addr < 128; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
        printf("  Found device at 0x%02X\r\n", addr);
    }
}
```

---

## 8. 참고 자료

- `docs/mpu6050/RM-MPU-6000A.pdf` — MPU6050 레지스터 맵 데이터시트
