#include "pid.h"
#include "motor_control.h"  // MOTOR_MAX_SPEED_MMPS

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
    float error      = target - measured;
    pid->integral   += error * pid->dt;
    float derivative = (error - pid->prev_error) / pid->dt;
    pid->prev_error  = error;

    float output = pid->Kp * error
                 + pid->Ki * pid->integral
                 + pid->Kd * derivative;

    // Clamp output to valid motor speed range
    if      (output >  (float)MOTOR_MAX_SPEED_MMPS) output =  (float)MOTOR_MAX_SPEED_MMPS;
    else if (output < -(float)MOTOR_MAX_SPEED_MMPS) output = -(float)MOTOR_MAX_SPEED_MMPS;

    return output;
}
