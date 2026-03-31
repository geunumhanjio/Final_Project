#ifndef __MPU6050_H__
#define __MPU6050_H__

#include <stdint.h>
#include "i2c.h"

/* MPU6050 I2C address (AD0=LOW) — HAL uses 8-bit address */
#define MPU6050_ADDR         (0x68 << 1)

/* Register map */
#define MPU6050_SMPLRT_DIV   0x19
#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H  0x43
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_WHO_AM_I     0x75

typedef struct {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
} MPU6050_Data_t;

HAL_StatusTypeDef MPU6050_Init(void);
HAL_StatusTypeDef MPU6050_ReadAll(MPU6050_Data_t *data);

#endif /* __MPU6050_H__ */
