/**
 * @file    sine_profile.h
 * @brief   Small sine helpers shared by the speed and position demos.
 */

#ifndef SINE_PROFILE_H
#define SINE_PROFILE_H

/**
 * @brief  Sine value at time t.
 * @param  amplitude      rpm for speed, deg for position
 * @param  frequency_hz   frequency in Hz
 * @param  t_s            time in seconds
 */
float Sine_Value(float amplitude, float frequency_hz, float t_s);

/**
 * @brief  Time derivative of Sine_Value (units per second).
 *
 * Used as velocity feed-forward for the position loop:
 * deg/s divided by 6 gives rpm.
 */
float Sine_Slope(float amplitude, float frequency_hz, float t_s);

#endif /* SINE_PROFILE_H */
