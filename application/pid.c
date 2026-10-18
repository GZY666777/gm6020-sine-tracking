/**
 * @file    pid.c
 * @brief   Generic PID implementation.
 */

#include "pid.h"

void PID_Init(pid_t *pid, float kp, float ki, float kd,
              float out_min, float out_max)
{
    if (pid == 0)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid->integ = 0.0f;
    pid->prev_meas = 0.0f;
    pid->has_prev = 0u;
}

void PID_Reset(pid_t *pid)
{
    if (pid == 0)
    {
        return;
    }

    pid->integ = 0.0f;
    pid->prev_meas = 0.0f;
    pid->has_prev = 0u;
}

float PID_Update(pid_t *pid, float setpoint, float measured,
                 float dt_s, float feedforward)
{
    float error;
    float p_term;
    float d_term;
    float i_new;
    float output;

    if (pid == 0 || dt_s <= 0.0f)
    {
        return 0.0f;
    }

    error = setpoint - measured;

    p_term = pid->kp * error;

    d_term = 0.0f;
    if (pid->kd != 0.0f)
    {
        if (pid->has_prev == 0u)
        {
            pid->prev_meas = measured;
            pid->has_prev = 1u;
        }
        d_term = -pid->kd * (measured - pid->prev_meas) / dt_s;
        pid->prev_meas = measured;
    }

    i_new = pid->integ + pid->ki * error * dt_s;

    output = p_term + i_new + d_term + feedforward;

    if (output > pid->out_max)
    {
        output = pid->out_max;
        if (error > 0.0f)
        {
            /* Stop integrating further into saturation. */
            i_new = pid->integ;
        }
    }
    else if (output < pid->out_min)
    {
        output = pid->out_min;
        if (error < 0.0f)
        {
            i_new = pid->integ;
        }
    }

    if (i_new > pid->out_max)
    {
        i_new = pid->out_max;
    }
    else if (i_new < pid->out_min)
    {
        i_new = pid->out_min;
    }

    pid->integ = i_new;

    return output;
}
