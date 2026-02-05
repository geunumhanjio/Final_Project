/**
  ******************************************************************************
  * @file           : motor_control.c
  * @brief          : L298N 모터 드라이버 제어 라이브러리 구현
  * @author         : 김지오
  * @date           : 2026-02-04
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "motor_control.h"
#include "gpio.h"

/* Private defines -----------------------------------------------------------*/
// L298N 진리표
// IN1 | IN2 | 동작
// ----|-----|------
//  1  |  0  | 정방향
//  0  |  1  | 역방향
//  0  |  0  | Coast (관성 정지)
//  1  |  1  | Brake (빠른 정지)

/* Private variables ---------------------------------------------------------*/
static uint8_t motor_initialized = 0;

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  속도 값을 PWM Pulse로 변환
 * @param  speed_percent: 0~100 (%)
 * @retval PWM pulse value (0~999)
 */
static inline uint16_t Speed_To_PWM(uint8_t speed_percent)
{
    if (speed_percent > MOTOR_MAX_SPEED) {
        speed_percent = MOTOR_MAX_SPEED;
    }
    return (uint16_t)((speed_percent * MOTOR_PWM_PERIOD) / 100);
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  모터 드라이버 초기화
 */
Motor_Status_t Motor_Init(void)
{
    // PWM 시작 (TIM2 CH1, CH2)
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) {
        return MOTOR_ERROR;
    }
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2) != HAL_OK) {
        return MOTOR_ERROR;
    }
    
    // 초기 상태: 모든 모터 정지
    Motor_Stop();
    
    motor_initialized = 1;
    return MOTOR_OK;
}

/**
 * @brief  개별 모터 속도 설정
 */
Motor_Status_t Motor_SetSpeed(Motor_Select_t motor, uint8_t speed)
{
    if (!motor_initialized) {
        return MOTOR_ERROR;
    }
    
    uint16_t pwm_value = Speed_To_PWM(speed);
    
    if (motor == MOTOR_LEFT) {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pwm_value);
    } else {
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pwm_value);
    }
    
    return MOTOR_OK;
}

/**
 * @brief  개별 모터 방향 설정
 */
Motor_Status_t Motor_SetDirection(Motor_Select_t motor, uint8_t direction)
{
    if (!motor_initialized) {
        return MOTOR_ERROR;
    }
    
    GPIO_TypeDef* gpio_port;
    uint16_t in1_pin, in2_pin;
    
    // 모터별 GPIO 핀 선택
    if (motor == MOTOR_LEFT) {
        gpio_port = GPIOC;
        in1_pin = GPIO_PIN_0;  // PC0 (IN1)
        in2_pin = GPIO_PIN_1;  // PC1 (IN2)
    } else {
        gpio_port = GPIOC;
        in1_pin = GPIO_PIN_2;  // PC2 (IN3)
        in2_pin = GPIO_PIN_3;  // PC3 (IN4)
    }
    
    // 방향 설정
    switch (direction) {
        case MOTOR_DIR_FORWARD:
            HAL_GPIO_WritePin(gpio_port, in1_pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(gpio_port, in2_pin, GPIO_PIN_RESET);
            break;
            
        case MOTOR_DIR_BACKWARD:
            HAL_GPIO_WritePin(gpio_port, in1_pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(gpio_port, in2_pin, GPIO_PIN_SET);
            break;
            
        case MOTOR_DIR_BRAKE:
            HAL_GPIO_WritePin(gpio_port, in1_pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(gpio_port, in2_pin, GPIO_PIN_SET);
            break;
            
        default:
            return MOTOR_ERROR;
    }
    
    return MOTOR_OK;
}

/**
 * @brief  양쪽 모터 동시 제어
 */
Motor_Status_t Motor_SetBoth(uint8_t left_speed, uint8_t right_speed, 
                             uint8_t left_dir, uint8_t right_dir)
{
    Motor_Status_t status;
    
    // 방향 설정
    status = Motor_SetDirection(MOTOR_LEFT, left_dir);
    if (status != MOTOR_OK) return status;
    
    status = Motor_SetDirection(MOTOR_RIGHT, right_dir);
    if (status != MOTOR_OK) return status;
    
    // 속도 설정
    status = Motor_SetSpeed(MOTOR_LEFT, left_speed);
    if (status != MOTOR_OK) return status;
    
    status = Motor_SetSpeed(MOTOR_RIGHT, right_speed);
    if (status != MOTOR_OK) return status;
    
    return MOTOR_OK;
}

/**
 * @brief  로봇 전진
 */
Motor_Status_t Motor_Forward(uint8_t speed)
{
    return Motor_SetBoth(speed, speed, 
                        MOTOR_DIR_FORWARD, MOTOR_DIR_FORWARD);
}

/**
 * @brief  로봇 후진
 */
Motor_Status_t Motor_Backward(uint8_t speed)
{
    return Motor_SetBoth(speed, speed, 
                        MOTOR_DIR_BACKWARD, MOTOR_DIR_BACKWARD);
}

/**
 * @brief  제자리 좌회전
 */
Motor_Status_t Motor_TurnLeft(uint8_t speed)
{
    return Motor_SetBoth(speed, speed, 
                        MOTOR_DIR_BACKWARD, MOTOR_DIR_FORWARD);
}

/**
 * @brief  제자리 우회전
 */
Motor_Status_t Motor_TurnRight(uint8_t speed)
{
    return Motor_SetBoth(speed, speed, 
                        MOTOR_DIR_FORWARD, MOTOR_DIR_BACKWARD);
}

/**
 * @brief  모든 모터 정지 (브레이크)
 */
Motor_Status_t Motor_Stop(void)
{
    // 속도 0으로 설정
    Motor_SetSpeed(MOTOR_LEFT, 0);
    Motor_SetSpeed(MOTOR_RIGHT, 0);
    
    // 브레이크 모드
    Motor_SetDirection(MOTOR_LEFT, MOTOR_DIR_BRAKE);
    Motor_SetDirection(MOTOR_RIGHT, MOTOR_DIR_BRAKE);
    
    return MOTOR_OK;
}

/**
 * @brief  부드러운 정지 (관성 정지)
 */
Motor_Status_t Motor_Coast(void)
{
    // 속도 0으로 설정
    Motor_SetSpeed(MOTOR_LEFT, 0);
    Motor_SetSpeed(MOTOR_RIGHT, 0);
    
    // Coast 모드 (IN1=0, IN2=0)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);  // IN1
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);  // IN2
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);  // IN3
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);  // IN4
    
    return MOTOR_OK;
}
