#include "encoder.h"

void Encoder_Init(void)
{
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

int16_t Encoder_GetCount(Encoder_Select_t encoder)
{
    if (encoder == ENCODER_LEFT) {
        return (int16_t)__HAL_TIM_GET_COUNTER(&htim3);
    } else {
        return -(int16_t)__HAL_TIM_GET_COUNTER(&htim4);
    }
}

void Encoder_ResetCount(Encoder_Select_t encoder)
{
    if (encoder == ENCODER_LEFT) {
        __HAL_TIM_SET_COUNTER(&htim3, 0);
    } else {
        __HAL_TIM_SET_COUNTER(&htim4, 0);
    }
}
