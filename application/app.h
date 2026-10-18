/**
 * @file    app.h
 * @brief   Application layer: mode sequencing, control loop, telemetry.
 */

#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef enum
{
    APP_STATE_WAIT = 0,   /* waiting for stable feedback, output 0 */
    APP_STATE_RUN = 1,    /* closed-loop control active */
    APP_STATE_FAULT = 2,  /* latched fault, output 0 */
    APP_STATE_STOP = 3    /* user stop, output 0 */
} app_state_t;

typedef enum
{
    APP_MODE_SPEED = 0,
    APP_MODE_POSITION = 1,
    APP_MODE_AUTO = 2
} app_mode_t;

void App_Init(void);

/** Must be called at exactly 1 kHz (from SysTick). */
void App_ControlStep(void);

/** Background work: UART commands, key, LED, CSV log. */
void App_MainLoop(void);

/** Select 0 = speed, 1 = position, 2 = auto demo. */
void App_RequestMode(uint8_t mode);

void App_Stop(void);
void App_ClearFault(void);

uint8_t App_GetState(void);
uint8_t App_GetMode(void);
uint8_t App_IsAuto(void);

#endif /* APP_H */
