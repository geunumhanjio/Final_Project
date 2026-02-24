#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "tim.h"

typedef enum {
    ENCODER_LEFT,
    ENCODER_RIGHT
} Encoder_Select_t;

void    Encoder_Init(void);
int16_t Encoder_GetCount(Encoder_Select_t encoder);
void    Encoder_ResetCount(Encoder_Select_t encoder);
float   Encoder_GetSpeed(Encoder_Select_t encoder);  // mm/s, call every 10ms

#endif /* __ENCODER_H__ */
