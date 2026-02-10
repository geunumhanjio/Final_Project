#include "mpu6050.h"

#define MPU6050_TIMEOUT  100  /* ms */

static HAL_StatusTypeDef MPU6050_WriteReg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, MPU6050_TIMEOUT);
}

static HAL_StatusTypeDef MPU6050_ReadReg(uint8_t reg, uint8_t *val)
{
    return HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, val, 1, MPU6050_TIMEOUT);
}

HAL_StatusTypeDef MPU6050_Init(void)
{
    uint8_t who;
    HAL_StatusTypeDef st;

    /* WHO_AM_I check — should return 0x68 */
    st = MPU6050_ReadReg(MPU6050_WHO_AM_I, &who);
    if (st != HAL_OK)   return st;
    if (who != 0x68)     return HAL_ERROR;

    /* Wake up (clear SLEEP bit) */
    st = MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
    if (st != HAL_OK) return st;

    /* Sample rate = 1 kHz / (1 + 7) = 125 Hz */
    st = MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x07);
    if (st != HAL_OK) return st;

    /* DLPF disabled */
    st = MPU6050_WriteReg(MPU6050_CONFIG, 0x00);
    if (st != HAL_OK) return st;

    /* Gyro: +/-250 deg/s */
    st = MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x00);
    if (st != HAL_OK) return st;

    /* Accel: +/-2 g */
    st = MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00);
    if (st != HAL_OK) return st;

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_ReadAll(MPU6050_Data_t *data)
{
    uint8_t buf[14];
    HAL_StatusTypeDef st;

    /* Read 14 bytes starting from ACCEL_XOUT_H (0x3B):
       AccX_H, AccX_L, AccY_H, AccY_L, AccZ_H, AccZ_L,
       Temp_H, Temp_L,
       GyX_H, GyX_L, GyY_H, GyY_L, GyZ_H, GyZ_L        */
    st = HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H,
                           I2C_MEMADD_SIZE_8BIT, buf, 14, MPU6050_TIMEOUT);
    if (st != HAL_OK) return st;

    data->accel_x = (int16_t)((buf[0]  << 8) | buf[1]);
    data->accel_y = (int16_t)((buf[2]  << 8) | buf[3]);
    data->accel_z = (int16_t)((buf[4]  << 8) | buf[5]);
    /* buf[6..7] = temperature — skipped */
    data->gyro_x  = (int16_t)((buf[8]  << 8) | buf[9]);
    data->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    data->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);

    return HAL_OK;
}
