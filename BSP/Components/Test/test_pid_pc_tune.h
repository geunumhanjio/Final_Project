#ifndef __TEST_PID_PC_TUNE_H__
#define __TEST_PID_PC_TUNE_H__

#if defined(MODE_PID_PC_TUNE)

#include <stdint.h>

/**
 * @brief PC 연동 PID 자동 튜닝 초기화
 * @param target_mmps 목표 속도 (mm/s)
 */
void Test_PID_PcTune_Init(int16_t target_mmps);

/**
 * @brief PC 연동 PID 자동 튜닝 루프 — main() while(1) 에서 호출
 *
 * - 블루 버튼(PC13)으로 모터 On/Off 토글
 * - 10ms 주기: 엔코더 속도 읽기 + PID 업데이트
 * - 50ms 주기: UART2(printf)로 고밀도 CSV 데이터 전송
 *   출력 포맷: DATA,<tick>,<target>,<actual_L>,<actual_R>
 *
 * PC Python 스크립트가 "DATA," 로 시작하는 라인을 파싱하여
 * Ziegler-Nichols 등의 알고리즘으로 최적 PID 게인을 계산한다.
 */
void Test_PID_PcTune_Loop(void);

#endif /* MODE_PID_PC_TUNE */

#endif /* __TEST_PID_PC_TUNE_H__ */
