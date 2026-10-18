/**
 * @file    motor_gm6020.h
 * @brief   DJI GM6020 CAN driver (voltage control + feedback parsing).
 *
 * Wiring / protocol notes:
 *   - GM6020 uses classic CAN, 1 Mbit/s by default.
 *   - Voltage command frame: 0x1FF for motor ID 1..4, 0x2FF for ID 5..7.
 *     Each frame carries four int16 big-endian commands; motor ID N uses
 *     bytes 2*(N-1) and 2*(N-1)+1 (slot 0..3).
 *   - Feedback frame: 0x204 + ID.  Data: ecd[0..1], rpm[2..3],
 *     current[4..5], temperature[6].
 *   - ecd is 0..8191 for one mechanical revolution (8192 counts/rev).
 */

#ifndef MOTOR_GM6020_H
#define MOTOR_GM6020_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

typedef struct
{
    uint16_t ecd;            /* 0..8191 mechanical angle */
    int16_t speed_rpm;       /* rotor speed, rpm */
    int16_t current_raw;     /* -16384..16384 */
    uint8_t temperature;     /* degC */
    uint16_t last_ecd;
    uint32_t last_rx_ms;     /* HAL tick of last valid feedback */
    uint32_t rx_count;       /* valid feedback frames received */
} gm6020_feedback_t;

/** Bind the driver to a CAN peripheral and a motor ID (1..7). */
void GM6020_Init(CAN_HandleTypeDef *hcan, uint8_t motor_id);

/**
 * @brief  Send a voltage command.
 * @param  voltage_cmd  -30000..30000 (about -24 V..+24 V)
 * @return 1 on success, 0 if the mailbox was full / HAL error.
 */
uint8_t GM6020_SendVoltageCmd(int16_t voltage_cmd);

/** Latest feedback snapshot (read individual fields; 32-bit reads are atomic). */
const gm6020_feedback_t *GM6020_GetFeedback(void);

/** Mechanical angle in degrees, 0..360 (coordinate corrected by MOTOR_SIGN). */
float GM6020_GetAngleDeg(void);

/** Rotor speed in rpm (coordinate corrected by MOTOR_SIGN). */
float GM6020_GetSpeedRpm(void);

/** 1 if a feedback frame arrived within timeout_ms. */
uint8_t GM6020_IsOnline(uint32_t now_ms, uint32_t timeout_ms);

uint8_t GM6020_GetMotorId(void);

#endif /* MOTOR_GM6020_H */
