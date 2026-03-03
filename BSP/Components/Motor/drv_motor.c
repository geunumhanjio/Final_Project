#include "drv_motor.h"
#include "algo_pid.h"
#include "gpio.h"
#include <stdlib.h>  // abs()
#include <stdio.h>

/* Private defines -----------------------------------------------------------*/

// L298N GPIO 핀 정의
#define MOTOR_LEFT_IN1_PORT     GPIOC
#define MOTOR_LEFT_IN1_PIN      GPIO_PIN_0
#define MOTOR_LEFT_IN2_PORT     GPIOC
#define MOTOR_LEFT_IN2_PIN      GPIO_PIN_1

#define MOTOR_RIGHT_IN1_PORT    GPIOC
#define MOTOR_RIGHT_IN1_PIN     GPIO_PIN_2
#define MOTOR_RIGHT_IN2_PORT    GPIOC
#define MOTOR_RIGHT_IN2_PIN     GPIO_PIN_3

// 타이머 채널
#define MOTOR_LEFT_PWM_CHANNEL  TIM_CHANNEL_1  // ENA
#define MOTOR_RIGHT_PWM_CHANNEL TIM_CHANNEL_2  // ENB

/* Private variables ---------------------------------------------------------*/
static uint8_t  motor_initialized     = 0;
static uint8_t  emergency_stop_active = 0;
static uint32_t last_cmd_time_ms      = 0;

// PID 목표 속도 (Motor_SetVelocity가 저장, Motor_PID_Update가 소비)
static float target_left_mmps  = 0.0f;
static float target_right_mmps = 0.0f;

// PID 인스턴스
static PID_t pid_left;
static PID_t pid_right;

/* Private function prototypes -----------------------------------------------*/
static uint16_t SpeedMMPS_To_PWM(uint16_t speed_mmps);
static void Motor_SetRaw(Motor_Select_t motor, int16_t speed_mmps);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  mm/s를 PWM Pulse로 변환
 * @param  speed_mmps: mm/s 단위 속도 (절댓값)
 * @retval PWM pulse value (0~999)
 */
static uint16_t SpeedMMPS_To_PWM(uint16_t speed_mmps)
{
    if (speed_mmps > MOTOR_MAX_SPEED_MMPS) {
        speed_mmps = MOTOR_MAX_SPEED_MMPS;
    }
    return (uint16_t)((speed_mmps * MOTOR_PWM_PERIOD) / MOTOR_MAX_SPEED_MMPS);
}

/**
 * @brief  저수준 모터 제어 (방향 + PWM)
 * @param  motor: MOTOR_LEFT 또는 MOTOR_RIGHT
 * @param  speed_mmps: -600 ~ +600 (mm/s)
 */
static void Motor_SetRaw(Motor_Select_t motor, int16_t speed_mmps)
{
    GPIO_TypeDef* in1_port;
    GPIO_TypeDef* in2_port;
    uint16_t in1_pin;
    uint16_t in2_pin;
    uint32_t pwm_channel;
    
    // 모터별 GPIO 및 타이머 채널 선택
    if (motor == MOTOR_LEFT) {
        in1_port = MOTOR_LEFT_IN1_PORT;
        in1_pin = MOTOR_LEFT_IN1_PIN;
        in2_port = MOTOR_LEFT_IN2_PORT;
        in2_pin = MOTOR_LEFT_IN2_PIN;
        pwm_channel = MOTOR_LEFT_PWM_CHANNEL;
    } else {
        in1_port = MOTOR_RIGHT_IN1_PORT;
        in1_pin = MOTOR_RIGHT_IN1_PIN;
        in2_port = MOTOR_RIGHT_IN2_PORT;
        in2_pin = MOTOR_RIGHT_IN2_PIN;
        pwm_channel = MOTOR_RIGHT_PWM_CHANNEL;
    }
    
    // 속도 제한
    if (speed_mmps > MOTOR_MAX_SPEED_MMPS) {
        speed_mmps = MOTOR_MAX_SPEED_MMPS;
    } else if (speed_mmps < -MOTOR_MAX_SPEED_MMPS) {
        speed_mmps = -MOTOR_MAX_SPEED_MMPS;
    }
    
    // 방향 결정
    uint8_t forward = (speed_mmps >= 0) ? 1 : 0;
    
    // PWM 계산
    uint16_t abs_speed = (uint16_t)abs(speed_mmps);
    uint16_t pwm_value = SpeedMMPS_To_PWM(abs_speed);
    
    // 방향 설정
    if (forward) {
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_SET);
    }
    
    /* Debugging --------------------------------------------------------*/
    //printf("[Real Motor PWM Value and Speed] %s: pwm = %d, speed = %d [mm/s]\r\n", (motor == MOTOR_LEFT) ? "LEFT" : "RIGHT", pwm_value, speed_mmps);

    /* Debugging PWM Value ----------------------------------------------------------*/
    //forward = 1;  // 항상 정방향으로 테스트
    //pwm_value = 0;  // PWM 절반 (약 300mm/s 예상) 
    
    // PWM 듀티 설정
    __HAL_TIM_SET_COMPARE(&htim2, pwm_channel, pwm_value);
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  모터 드라이버 초기화
 */
Motor_Status_t Motor_Init(void)
{
    // PWM 타이머 시작
    if (HAL_TIM_PWM_Start(&htim2, MOTOR_LEFT_PWM_CHANNEL) != HAL_OK) {
        return MOTOR_ERROR;
    }
    if (HAL_TIM_PWM_Start(&htim2, MOTOR_RIGHT_PWM_CHANNEL) != HAL_OK) {
        return MOTOR_ERROR;
    }
    
    // 초기 상태: 모든 GPIO LOW, PWM 0
    HAL_GPIO_WritePin(MOTOR_LEFT_IN1_PORT, MOTOR_LEFT_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_LEFT_IN2_PORT, MOTOR_LEFT_IN2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN1_PORT, MOTOR_RIGHT_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN2_PORT, MOTOR_RIGHT_IN2_PIN, GPIO_PIN_RESET);
    
    __HAL_TIM_SET_COMPARE(&htim2, MOTOR_LEFT_PWM_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&htim2, MOTOR_RIGHT_PWM_CHANNEL, 0);
    
    // 초기 상태 설정
    motor_initialized     = 1;
    emergency_stop_active = 0;
    target_left_mmps      = 0.0f;
    target_right_mmps     = 0.0f;
    last_cmd_time_ms      = HAL_GetTick();

    PID_Init(&pid_left,  PID_KP, PID_KI, PID_KD, PID_DT);
    PID_Init(&pid_right, PID_KP, PID_KI, PID_KD, PID_DT);

    return MOTOR_OK;
}

/**
 * @brief  양쪽 바퀴 목표 속도 설정 (PID 루프가 실제 제어)
 */
Motor_Status_t Motor_SetVelocity(int16_t left_mmps, int16_t right_mmps)
{
    if (!motor_initialized)    return MOTOR_ERROR;
    if (emergency_stop_active) return MOTOR_ERROR;

    last_cmd_time_ms  = HAL_GetTick();
    target_left_mmps  = (float)left_mmps;
    target_right_mmps = (float)right_mmps;

    return MOTOR_OK;
}

/**
 * @brief  비상 정지 (즉시 브레이크)
 */
Motor_Status_t Motor_EmergencyStop(void)
{
    if (!motor_initialized) return MOTOR_ERROR;

    emergency_stop_active = 1;
    target_left_mmps      = 0.0f;
    target_right_mmps     = 0.0f;
    PID_Reset(&pid_left);
    PID_Reset(&pid_right);

    // PWM 즉시 0
    __HAL_TIM_SET_COMPARE(&htim2, MOTOR_LEFT_PWM_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&htim2, MOTOR_RIGHT_PWM_CHANNEL, 0);

    // 브레이크 모드 (IN1=1, IN2=1)
    HAL_GPIO_WritePin(MOTOR_LEFT_IN1_PORT,  MOTOR_LEFT_IN1_PIN,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_LEFT_IN2_PORT,  MOTOR_LEFT_IN2_PIN,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN1_PORT, MOTOR_RIGHT_IN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN2_PORT, MOTOR_RIGHT_IN2_PIN, GPIO_PIN_SET);

    return MOTOR_OK;
}

/**
 * @brief  부드러운 정지 — 목표 속도를 0으로 설정, PID가 감속 처리
 */
Motor_Status_t Motor_SoftStop(void)
{
    if (!motor_initialized)    return MOTOR_ERROR;
    if (emergency_stop_active) return MOTOR_ERROR;

    target_left_mmps  = 0.0f;
    target_right_mmps = 0.0f;

    return MOTOR_OK;
}

/**
 * @brief  명령 타임아웃 확인
 */
bool Motor_CheckTimeout(void)
{
    if (!motor_initialized) {
        return false;
    }
    
    // 비상 정지 상태면 타임아웃 체크 안 함
    if (emergency_stop_active) {
        return false;
    }
    
    // 타임아웃 확인
    uint32_t elapsed = HAL_GetTick() - last_cmd_time_ms;
    
    if (elapsed > MOTOR_CMD_TIMEOUT_MS) {
        // 타임아웃 발생 → 자동 정지
        Motor_SoftStop();
        return true;
    }
    
    return false;
}

/**
 * @brief  비상 정지 해제
 */
Motor_Status_t Motor_ReleaseEmergency(void)
{
    if (!motor_initialized) return MOTOR_ERROR;

    emergency_stop_active = 0;
    PID_Reset(&pid_left);
    PID_Reset(&pid_right);

    // Coast 모드로 전환 (안전하게)
    HAL_GPIO_WritePin(MOTOR_LEFT_IN1_PORT,  MOTOR_LEFT_IN1_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_LEFT_IN2_PORT,  MOTOR_LEFT_IN2_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN1_PORT, MOTOR_RIGHT_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN2_PORT, MOTOR_RIGHT_IN2_PIN, GPIO_PIN_RESET);

    last_cmd_time_ms = HAL_GetTick();

    return MOTOR_OK;
}

/**
 * @brief  PID 속도 제어 루프 — 10ms마다 main.c에서 호출
 */
void Motor_PID_Update(float left_measured_mmps, float right_measured_mmps)
{
    if (!motor_initialized || emergency_stop_active) return;

    float left_out  = PID_Update(&pid_left,  target_left_mmps,  left_measured_mmps);
    float right_out = PID_Update(&pid_right, target_right_mmps, right_measured_mmps);

    Motor_SetRaw(MOTOR_LEFT,  (int16_t)left_out);
    Motor_SetRaw(MOTOR_RIGHT, (int16_t)right_out);
}