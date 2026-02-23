#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "tim.h"

typedef enum {
    ENCODER_LEFT,
    ENCODER_RIGHT
} Encoder_Select_t;

void Encoder_Init(void);  // 타이머(하드웨어)의 엔코더 인터페이스 시작
int16_t Encoder_GetCount(Encoder_Select_t encoder);  // 타이머 CNT 값 읽기 (-> 모터 회전 속도 계산)
void Encoder_ResetCount(Encoder_Select_t encoder);  // 타이머 CNT 값 영으로 초기화

#endif /* __ENCODER_H__ */
