// 031: 모터 동작 자꾸 멈추는 거 wasd로 테스트. (pid 제어 코드가 문제 원인으로 추정됨)

#if defined(MODE_WASD)

#include "test_wasd.h"
#include "drv_motor.h"
#include "drv_encoder.h"
#include "usart.h"
#include <stdio.h>

#define WASD_LINEAR   200   /* mm/s 전진/후진 속도 */
#define WASD_ANGULAR  200   /* mm/s 회전 시 각 바퀴 속도 */

bool stop = true; 

void Test_Wasd_Init(void)
{
    printf("=== MODE: WASD ===\r\n");
    printf("  w : 전진\r\n");
    printf("  s : 후진\r\n");
    printf("  a : 좌회전\r\n");
    printf("  d : 우회전\r\n");
    printf("  SPACE/x : 정지\r\n");
    printf("주의: 키 누를 때마다 명령 갱신. 500ms 무입력 시 자동 정지.\r\n");
    printf("==================\r\n");
}

void Test_Wasd_Loop(void)
{
    /* ── 키 입력 처리 (Protocol_Process 대체) ── */
    uint8_t key = 0;
    if (HAL_UART_Receive(&huart2, &key, 1, 0) == HAL_OK) {
        switch (key) {
        case 'w': case 'W':
            Motor_SetVelocity(WASD_LINEAR, WASD_LINEAR);
            printf("[KEY] w — 전진 %d mm/s\r\n", WASD_LINEAR);
            stop = false;
            break;
        case 's': case 'S':
            Motor_SetVelocity(-WASD_LINEAR, -WASD_LINEAR);
            printf("[KEY] s — 후진 %d mm/s\r\n", WASD_LINEAR);
            stop = false;
            break;
        case 'a': case 'A':
            Motor_SetVelocity(-WASD_ANGULAR, WASD_ANGULAR);
            printf("[KEY] a — 좌회전\r\n");
            stop = false;
            break;
        case 'd': case 'D':
            Motor_SetVelocity(WASD_ANGULAR, -WASD_ANGULAR);
            printf("[KEY] d — 우회전\r\n");
            stop = false;
            break;
        case ' ': case 'x': case 'X':
            Motor_SoftStop();
            printf("[KEY] x — 정지\r\n");
            stop = true;
            break;
        default:
            break;
        }
    }

    // Motor_CheckTimeout();

    uint32_t now = HAL_GetTick();

    /* PID 속도 제어 루프 — 10ms 주기 */
    static uint32_t last_pid   = 0;
    static float    last_spd_L = 0.0f;
    static float    last_spd_R = 0.0f;
    if (now - last_pid >= 10) {
        last_pid   = now;
        last_spd_L = Encoder_GetSpeed(ENCODER_LEFT);
        last_spd_R = Encoder_GetSpeed(ENCODER_RIGHT);
        Motor_PID_Update(last_spd_L, last_spd_R);
    }

    /* 진단 출력 — 500ms 주기 */
    static uint32_t last_print = 0;
    if (now - last_print >= 50) {
        last_print = now;
        if (!stop) {
            printf("[DIAG] spd L:%.1f R:%.1f mm/s  enc L:%d R:%d\r\n",
               last_spd_L, last_spd_R,
               Encoder_GetCount(ENCODER_LEFT),
               Encoder_GetCount(ENCODER_RIGHT));
        }
    }
}

#endif /* MODE_WASD */
