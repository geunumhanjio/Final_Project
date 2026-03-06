#include "drv_encoder.h"

#define ENCODER_PPR             3840.0f   // 4x quadrature (480 CPR × 4)
#define WHEEL_CIRCUMFERENCE_MM  204.2f    // π × 65mm (지름 6.5cm)
#define ENCODER_DT_S            0.01f     // 10ms 제어 주기

/* 타이머(하드웨어)의 엔코더 인터페이스 시작 */
void Encoder_Init(void)
{
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);  // 파라미터가 TIM_CHANNEL_ALL이므로 TI1와 TI2 모두 활성화 (4체배)
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

/* 타이머 CNT 값 읽기 (-> 모터 회전 속도 계산) */
int16_t Encoder_GetCount(Encoder_Select_t encoder)
{
    if (encoder == ENCODER_LEFT) {
        return (int16_t)__HAL_TIM_GET_COUNTER(&htim3);  // 정회전/역회전 구분 위해 int16_t로 캐스팅
    } else {
        return -(int16_t)__HAL_TIM_GET_COUNTER(&htim4);  // 왼쪽과 오른쪽 엔코더는 서로 회전 방향이 반대이므로 x(-1)
    }
}

/* 타이머 CNT 값 영으로 초기화 */
void Encoder_ResetCount(Encoder_Select_t encoder)
{
    if (encoder == ENCODER_LEFT) {
        __HAL_TIM_SET_COUNTER(&htim3, 0);
    } else {
        __HAL_TIM_SET_COUNTER(&htim4, 0);
    }
}

/* 10ms마다 호출되며 직전 호출 대비 tick 변화량을 mm/s로 변환 */
float Encoder_GetSpeed(Encoder_Select_t encoder)
{
    static int16_t prev_left  = 0;
    static int16_t prev_right = 0;

    int16_t current = Encoder_GetCount(encoder);
    int16_t delta;

    if (encoder == ENCODER_LEFT) {
        delta = current - prev_left;
        prev_left = current;
    } else {
        delta = current - prev_right;
        prev_right = current;
    }

    // delta [tick] / PPR [tick/rev] * circumference [mm/rev] / dt [s] = mm/s
    return ((float)delta / ENCODER_PPR) * WHEEL_CIRCUMFERENCE_MM / ENCODER_DT_S;
}
