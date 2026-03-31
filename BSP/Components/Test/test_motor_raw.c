#if defined(MODE_MOTOR_RAW)

#include "test_motor_raw.h"
#include "drv_motor.h"
#include "drv_encoder.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

static int16_t s_test_speed = 200;
static uint8_t s_running    = 0;
static uint8_t s_btn_prev   = 0;

void Test_MotorRaw_Init(int16_t test_speed_mmps)
{
    s_test_speed = test_speed_mmps;
    s_running    = 0;
    s_btn_prev   = 0;

    printf("=== MODE: MOTOR_RAW ===\r\n");
    printf("PWM speed: %d  (no PID)\r\n", s_test_speed);
    printf("Blue button: motor ON/OFF toggle\r\n");
    printf("=======================\r\n");
}

void Test_MotorRaw_Loop(void)
{
    uint32_t now = HAL_GetTick();

    /* 블루 버튼 (PC13, active-low) 엣지 감지 */
    uint8_t btn_now = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ? 1 : 0;
    if (btn_now && !s_btn_prev) {
        s_running = !s_running;
        if (s_running) {
            Motor_SetRaw(MOTOR_LEFT,  s_test_speed);
            Motor_SetRaw(MOTOR_RIGHT, s_test_speed);
            printf("MOTOR ON  (raw=%d)\r\n", s_test_speed);
        } else {
            Motor_SetRaw(MOTOR_LEFT,  0);
            Motor_SetRaw(MOTOR_RIGHT, 0);
            printf("MOTOR OFF\r\n");
        }
    }
    s_btn_prev = btn_now;

    /* 500ms 주기: 엔코더 실측 속도 출력 */
    static uint32_t last_print = 0;
    static float    spd_L      = 0.0f;
    static float    spd_R      = 0.0f;
    if (now - last_print >= 10) {
        spd_L = Encoder_GetSpeed(ENCODER_LEFT);
        spd_R = Encoder_GetSpeed(ENCODER_RIGHT);
    }
    if (now - last_print >= 500) {
        last_print = now;
        printf("[RAW] pwm=%d  actual L:%.1f  R:%.1f mm/s\r\n",
               s_test_speed, spd_L, spd_R);
    }
}

#endif /* MODE_MOTOR_RAW */
