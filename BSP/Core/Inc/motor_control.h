/**
  ******************************************************************************
  * @file           : motor_control.h
  * @brief          : L298N 모터 드라이버 제어 라이브러리
  * @author         : 김지오
  * @date           : 2026-02-04
  ******************************************************************************
  * @description
  * - L298N 듀얼 모터 드라이버를 사용한 차동구동 로봇 제어
  * - PWM을 통한 속도 제어 (0~100%)
  * - GPIO를 통한 방향 제어 (전진/후진)
  ******************************************************************************
  */

#ifndef __MOTOR_CONTROL_H
#define __MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"

/* Exported defines ----------------------------------------------------------*/
#define MOTOR_MAX_SPEED         100   // 최대 속도 (%)
#define MOTOR_PWM_PERIOD        999   // TIM2 ARR 값과 동일

/* 방향 정의 */
#define MOTOR_DIR_FORWARD       0
#define MOTOR_DIR_BACKWARD      1
#define MOTOR_DIR_BRAKE         2

/* Exported types ------------------------------------------------------------*/
typedef enum {
    MOTOR_LEFT = 0,
    MOTOR_RIGHT = 1
} Motor_Select_t;

typedef enum {
    MOTOR_OK = 0,
    MOTOR_ERROR = 1
} Motor_Status_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  모터 드라이버 초기화
 * @note   CubeMX에서 생성된 TIM2 초기화 후 호출
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_Init(void);

/**
 * @brief  개별 모터 속도 설정
 * @param  motor: MOTOR_LEFT 또는 MOTOR_RIGHT
 * @param  speed: 0~100 (%)
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_SetSpeed(Motor_Select_t motor, uint8_t speed);

/**
 * @brief  개별 모터 방향 설정
 * @param  motor: MOTOR_LEFT 또는 MOTOR_RIGHT
 * @param  direction: MOTOR_DIR_FORWARD, MOTOR_DIR_BACKWARD, MOTOR_DIR_BRAKE
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_SetDirection(Motor_Select_t motor, uint8_t direction);

/**
 * @brief  양쪽 모터 동시 제어 (속도 + 방향)
 * @param  left_speed: 왼쪽 모터 속도 (0~100%)
 * @param  right_speed: 오른쪽 모터 속도 (0~100%)
 * @param  left_dir: 왼쪽 모터 방향
 * @param  right_dir: 오른쪽 모터 방향
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_SetBoth(uint8_t left_speed, uint8_t right_speed, 
                             uint8_t left_dir, uint8_t right_dir);

/**
 * @brief  로봇 전진
 * @param  speed: 0~100 (%)
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_Forward(uint8_t speed);

/**
 * @brief  로봇 후진
 * @param  speed: 0~100 (%)
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_Backward(uint8_t speed);

/**
 * @brief  제자리 좌회전
 * @param  speed: 0~100 (%)
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_TurnLeft(uint8_t speed);

/**
 * @brief  제자리 우회전
 * @param  speed: 0~100 (%)
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_TurnRight(uint8_t speed);

/**
 * @brief  모든 모터 정지 (브레이크)
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_Stop(void);

/**
 * @brief  부드러운 정지 (관성 정지)
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_Coast(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CONTROL_H */
