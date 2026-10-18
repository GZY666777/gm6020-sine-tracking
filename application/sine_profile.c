/**
 * @file    sine_profile.c
 * @brief   Sine trajectory generator.
 */

#include "sine_profile.h"

#include <math.h>

#define SINE_TWO_PI 6.28318530718f

float Sine_Value(float amplitude, float frequency_hz, float t_s)
{
    /* sinf() is provided by the ARM C library and uses the FPU on the F407. */
    return amplitude * sinf(SINE_TWO_PI * frequency_hz * t_s);
}

float Sine_Slope(float amplitude, float frequency_hz, float t_s)
{
    return amplitude * SINE_TWO_PI * frequency_hz *
           cosf(SINE_TWO_PI * frequency_hz * t_s);
}
