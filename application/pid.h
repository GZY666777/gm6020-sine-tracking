/**
 * @file    pid.h
 * @brief   Generic PID with output clamping and conditional anti-windup.
 */

#ifndef PID_H
#define PID_H

#include <stdint.h>

typedef struct
{
    float kp;
    float ki;
    float kd;
    float out_min;
    float out_max;

    float integ;       /* integral term, already multiplied by ki */
    float prev_meas;
    uint8_t has_prev;
} pid_t;

void PID_Init(pid_t *pid, float kp, float ki, float kd,
              float out_min, float out_max);

void PID_Reset(pid_t *pid);

/**
 * @brief  One PID step.
 * @param  pid        controller
 * @param  setpoint   target value
 * @param  measured   measured value
 * @param  dt_s       sample time in seconds
 * @param  feedforward  feed-forward term added after the PID terms
 * @return saturated output
 *
 * Derivative is calculated on the measurement (no setpoint kick).
 * Conditional integration stops the integrator while the output is
 * saturated in the same direction as the error.
 */
float PID_Update(pid_t *pid, float setpoint, float measured,
                 float dt_s, float feedforward);

#endif /* PID_H */
