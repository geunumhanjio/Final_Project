#ifndef __TEST_MOTOR_RAW_H__
#define __TEST_MOTOR_RAW_H__

#if defined(MODE_MOTOR_RAW)

#include <stdint.h>

/**
 * @brief Raw PWM 모터 테스트 초기화
 * @param test_speed_mmps 테스트할 Raw 속도값 (PWM 듀티 기준, mm/s 스케일)
 */
void Test_MotorRaw_Init(int16_t test_speed_mmps);

/**
 * @brief Raw PWM 모터 테스트 루프 — main() while(1) 에서 호출
 *
 * - 블루 버튼(PC13)으로 모터 On/Off 토글
 * - PID 없이 Motor_SetRaw() 로 직접 PWM 출력
 * - 500ms 주기: 엔코더 실측 속도 UART2(printf) 출력
 */
void Test_MotorRaw_Loop(void);

#endif /* MODE_MOTOR_RAW */

#endif /* __TEST_MOTOR_RAW_H__ */
