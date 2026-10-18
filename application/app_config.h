/**
 * @file    app_config.h
 * @brief   All tunable parameters for the GM6020 sine-tracking demo.
 *
 * Keep every "magic number" here so the report can list one parameter table.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------------------------------------------------- */
/* Motor / CAN                                                               */
/* ------------------------------------------------------------------------- */
/* Motor ID set with RoboMaster Assistant. GM6020 default is 1.
 * Feedback ID is 0x204 + GM6020_ID, control frame is 0x1FF (ID 1..4)
 * or 0x2FF (ID 5..7). */
#define GM6020_ID                     1

/* Change to -1 if the motor is mounted in the opposite direction. */
#define MOTOR_SIGN                    (+1)

/* GM6020 voltage command range is [-30000, 30000] (about +/-24 V). */
#define GM6020_VOLTAGE_MAX            30000

/* ------------------------------------------------------------------------- */
/* Sine trajectories                                                         */
/* ------------------------------------------------------------------------- */
/* Speed loop demo: 200 rpm amplitude, 0.2 Hz. */
#define SINE_SPEED_AMP_RPM            200.0f
#define SINE_SPEED_FREQ_HZ            0.2f

/* Position loop demo: +/-60 deg, 0.1 Hz (about +/-37.7 deg/s peak). */
#define SINE_POS_AMP_DEG              60.0f
#define SINE_POS_FREQ_HZ              0.1f

/* ------------------------------------------------------------------------- */
/* Control gains (1 kHz loop)                                                */
/* ------------------------------------------------------------------------- */
#define CONTROL_DT_S                  0.001f

/* Inner speed loop: error in rpm -> voltage counts. */
#define SPEED_KP                      80.0f
#define SPEED_KI                      800.0f
#define SPEED_KD                      0.0f

/* Feed-forward: voltage counts per rpm.  GM6020 no-load is about 320 rpm
 * at 24 V, so 30000/320 ~= 94.  90 is a good starting point. */
#define SPEED_FF_COUNTS_PER_RPM       90.0f

/* Outer position loop: error in deg -> speed command in rpm. */
#define POS_KP                        4.0f
#define POS_KI                        0.0f
#define POS_KD                        0.0f
#define POS_MAX_RPM                   250.0f

/* ------------------------------------------------------------------------- */
/* Protection / watchdog                                                     */
/* ------------------------------------------------------------------------- */
/* Motor feedback must arrive at least every 100 ms. */
#define SAFETY_FEEDBACK_TIMEOUT_MS    100u

/* Temperature protection (GM6020 reports 0..100 degC). */
#define SAFETY_TEMP_WARN_C            70u
#define SAFETY_TEMP_FAULT_C           80u

/* Position-mode soft limit.  Target is +/-60 deg, so 100 deg means the
 * motor is 40 deg away from the reference before we cut the output. */
#define SAFETY_POS_LIMIT_DEG          100.0f
#define SAFETY_POS_CLEAR_DEG          80.0f

/* Abort after 50 consecutive failed CAN transmissions (50 ms at 1 kHz). */
#define SAFETY_TX_FAIL_LIMIT          50u

/* Independent watchdog: LSI 32 kHz / 32 = 1 kHz, reload 250 -> 250 ms.
 * It is refreshed only by the 1 kHz control step, so a firmware hang
 * resets the MCU. */
#define SAFETY_IWDG_PRESCALER         IWDG_PRESCALER_32
#define SAFETY_IWDG_RELOAD            250u

/* ------------------------------------------------------------------------- */
/* Demo sequencing / logging                                                 */
/* ------------------------------------------------------------------------- */
/* Auto demo: 10 s speed sine (2 periods at 0.2 Hz), then 10 s position
 * sine (1 period at 0.1 Hz), repeating.  Both phases end at zero, so the
 * mode switch is bumpless. */
#define AUTO_PHASE_S                  10.0f

/* Wait for stable feedback before enabling the output. */
#define WAIT_STABLE_MS                300u

/* Serial CSV log period. */
#define LOG_PERIOD_MS                 20u

#endif /* APP_CONFIG_H */
