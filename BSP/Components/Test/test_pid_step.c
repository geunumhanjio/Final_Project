#if defined(MODE_PID_STEP)

#include "test_pid_step.h"
#include "drv_motor.h"
#include "drv_encoder.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

static int16_t s_target   = 200;    // 여기서 초기값 설정해도 소용없음 !!!!!!!!! main의 init 함수의 인자로 타겟 속도가 결정됨
static uint8_t s_running  = 0;
static uint8_t s_btn_prev = 0;

void Test_PID_Step_Init(int16_t target_mmps)
{
    s_target   = target_mmps;
    s_running  = 0;
    s_btn_prev = 0;

    printf("=== MODE: PID_STEP ===\r\n");
    printf("Setpoint: %d mm/s\r\n", s_target);
    printf("Blue button: motor ON/OFF toggle\r\n");
    printf("DATA format: DATA,tick,target,actual_L,actual_R\r\n");
    printf("======================\r\n");
}

void Test_PID_Step_Loop(void)
{
    uint32_t now = HAL_GetTick();

    /* 블루 버튼 (PC13, active-low) 엣지 감지 */
    uint8_t btn_now = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ? 1 : 0;
    if (btn_now && !s_btn_prev) {
        s_running = !s_running;
        if (s_running) {
            Motor_SetVelocity(s_target, s_target);
            printf("MOTOR ON  (target=%d mm/s)\r\n", s_target);
        } else {
            Motor_SoftStop();  // PID_Reset(), 타겟 속도 영으로 설정
            printf("MOTOR OFF\r\n");
        }
    }
    s_btn_prev = btn_now;

    /* 10ms 주기: 엔코더 읽기 + PID 업데이트 */
    static uint32_t last_pid = 0;
    static float    spd_L    = 0.0f;
    static float    spd_R    = 0.0f;
    if (now - last_pid >= 10) {
        last_pid = now;
        spd_L    = Encoder_GetSpeed(ENCODER_LEFT);
        spd_R    = Encoder_GetSpeed(ENCODER_RIGHT);
        if (s_running) {
            Motor_PID_Update(spd_L, spd_R);
        }
    }

    /* 100ms 주기: CSV 데이터 UART2(printf)로 전송 */
    static uint32_t last_print = 0;
    if (now - last_print >= 100) {
        last_print = now;
        printf("DATA,%lu,%d,%.1f,%.1f\r\n",
               (unsigned long)now, s_target, spd_L, spd_R);
    }
}

#endif /* MODE_PID_STEP */
