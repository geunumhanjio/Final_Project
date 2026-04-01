#ifndef __TEST_HW_VERIFY_H__
#define __TEST_HW_VERIFY_H__

#if defined(MODE_HW_VERIFY)

/**
 * @brief 하드웨어 검증 테스트 초기화
 */
void Test_HwVerify_Init(void);

/**
 * @brief 하드웨어 검증 테스트 루프 — main() while(1) 에서 호출
 *
 * 자동 진행 순서 (총 ~11초):
 *  [1/5] IMU MPU6050  — Init + ReadAll + accel_z 확인
 *  [2/5] ENC_L 전진   — Motor L 200mm/s 2초 → 카운트 > 100
 *  [3/5] ENC_R 전진   — Motor R 200mm/s 2초 → 카운트 > 100
 *  [4/5] MOTOR_L 후진 — Motor L -200mm/s 1.5초 → 카운트 < -100
 *  [5/5] MOTOR_R 후진 — Motor R -200mm/s 1.5초 → 카운트 < -100
 *  SUMMARY — 전체 PASS/FAIL 출력
 */
void Test_HwVerify_Loop(void);

#endif /* MODE_HW_VERIFY */

#endif /* __TEST_HW_VERIFY_H__ */
