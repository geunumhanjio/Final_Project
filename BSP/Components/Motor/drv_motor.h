#ifndef __MOTOR_CONTROL_H
#define __MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "algo_pid.h"
#include <stdbool.h>

/* Exported defines ----------------------------------------------------------*/
#define MOTOR_MAX_SPEED_MMPS    500   // 최대 속도 (mm/s) - 실측 후 조정 (이론적으로는 모터 RPM과 바퀴 지름 기반으로 계산)
#define MOTOR_PWM_PERIOD        999   // TIM2 ARR 값과 동일
#define MOTOR_CMD_TIMEOUT_MS    500   // 명령 타임아웃 (500ms)

// PID 게인 — 모터별 독립 설정
// 튜닝 순서: Kp 먼저 (Ki=Kd=0) → 정상 상태 오차 있으면 Ki 추가 → 진동 시 Kd 추가
#define PID_LEFT_KP   0.6f
#define PID_LEFT_KI   4.0f
#define PID_LEFT_KD   0.0f

#define PID_RIGHT_KP  0.6f
#define PID_RIGHT_KI  4.0f   // 오른쪽 모터 특성 차이 시 조정
#define PID_RIGHT_KD  0.0f

#define PID_DT  0.01f   // 10ms (main.c PID 루프 주기와 일치해야 함)

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

void Motor_SetRaw(Motor_Select_t motor, int16_t speed_mmps);

/**
 * @brief  양쪽 바퀴 속도 설정 (ROS2 /cmd_vel 처리용)
 * @param  left_mmps: 왼쪽 바퀴 목표 속도 (-600 ~ +600 mm/s)
 * @param  right_mmps: 오른쪽 바퀴 목표 속도 (-600 ~ +600 mm/s)
 * @note   이 함수 호출 시 자동으로 watchdog 타임아웃 리셋됨
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_SetVelocity(int16_t left_mmps, int16_t right_mmps);

/**
 * @brief  비상 정지 (즉시 브레이크)
 * @note   모든 제어 명령 무시하고 즉시 정지
 * @note   Motor_ReleaseEmergency() 호출 전까지 모든 속도 명령 무시
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_EmergencyStop(void);

/**
 * @brief  부드러운 정지 (감속 후 정지)
 * @note   가속도 제한 적용하여 부드럽게 감속
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_SoftStop(void);

/**
 * @brief  명령 타임아웃 확인 (주기적으로 호출 필요)
 * @note   100Hz 타이머에서 호출하여 타임아웃 시 자동 정지
 * @retval true: 타임아웃 발생, false: 정상
 */
bool Motor_CheckTimeout(void);

/**
 * @brief  비상 정지 해제
 * @retval Motor_Status_t
 */
Motor_Status_t Motor_ReleaseEmergency(void);

/**
 * @brief  PID 속도 제어 루프 (10ms마다 호출)
 * @param  left_measured_mmps:  왼쪽 바퀴 실측 속도 (Encoder_GetSpeed 결과)
 * @param  right_measured_mmps: 오른쪽 바퀴 실측 속도 (Encoder_GetSpeed 결과)
 */
void Motor_PID_Update(float left_measured_mmps, float right_measured_mmps);

/**
 * @brief  모터 PID 제어기 누적 오차(I) 초기화
 */
void Motor_PID_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CONTROL_H */