#include "algo_pid.h"
#include "drv_motor.h"  // MOTOR_MAX_SPEED_MMPS

void PID_Init(PID_t *pid, float Kp, float Ki, float Kd, float dt)
{
    pid->Kp         = Kp;
    pid->Ki         = Ki;
    pid->Kd         = Kd;
    pid->dt         = dt;
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
}

void PID_Reset(PID_t *pid)
{
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
}

float PID_Update(PID_t *pid, float target, float measured)
{
    // Feedforward: 목표 속도 자체를 일정 비율로 기본 출력에 더해줌
    // (모터가 목표 속도에 도달하기 위해 기본적으로 필요한 PWM 값)
    float feedforward = target * 0.3f * 0; // 경험적으로 0.8~1.0 사이

    float error      = target - measured;
    pid->integral   += error * pid->dt;

    // Integral windup 방지 — Ki*integral이 출력 한계를 넘지 않도록 클램핑
    if (pid->Ki > 0.0f) {
        float integral_limit = (float)MOTOR_MAX_SPEED_MMPS / pid->Ki;
        if      (pid->integral >  integral_limit) pid->integral =  integral_limit;
        else if (pid->integral < -integral_limit) pid->integral = -integral_limit;
    }

    float derivative = (error - pid->prev_error) / pid->dt;
    pid->prev_error  = error;

    float output = feedforward 
                 + pid->Kp * error
                 + pid->Ki * pid->integral
                 + pid->Kd * derivative;

    // Clamp output to valid motor speed range
    if      (output >  (float)MOTOR_MAX_SPEED_MMPS) output =  (float)MOTOR_MAX_SPEED_MMPS;
    else if (output < -(float)MOTOR_MAX_SPEED_MMPS) output = -(float)MOTOR_MAX_SPEED_MMPS;

    return output;
}
