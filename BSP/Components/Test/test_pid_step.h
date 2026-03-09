#ifndef __TEST_PID_STEP_H__
#define __TEST_PID_STEP_H__

#if defined(MODE_PID_STEP)

#include <stdint.h>

/**
 * @brief PID 계단 응답 테스트 초기화
 * @param target_mmps 단일 목표 속도 (mm/s)
 */
void Test_PID_Step_Init(int16_t target_mmps);

/**
 * @brief PID 계단 응답 테스트 루프 — main() while(1) 에서 호출
 *
 * - 블루 버튼(PC13)으로 모터 On/Off 토글
 * - 10ms 주기: 엔코더 속도 읽기 + PID 업데이트
 * - 100ms 주기: UART2(printf)로 상태 출력
 *   출력 포맷: DATA,<tick>,<target>,<actual_L>,<actual_R>
 */
void Test_PID_Step_Loop(void);

#endif /* MODE_PID_STEP */

#endif /* __TEST_PID_STEP_H__ */
