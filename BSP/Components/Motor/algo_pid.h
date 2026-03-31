#ifndef __PID_H__
#define __PID_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float Kp;
    float Ki;
    float Kd;
    float integral;
    float prev_error;
    float dt;           // seconds
} PID_t;

void  PID_Init(PID_t *pid, float Kp, float Ki, float Kd, float dt);
void  PID_Reset(PID_t *pid);
float PID_Update(PID_t *pid, float target, float measured);

#ifdef __cplusplus
}
#endif

#endif /* __PID_H__ */
