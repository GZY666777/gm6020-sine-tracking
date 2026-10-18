/**
 * @file    board_io.c
 * @brief   RGB LED and user key helpers.
 */

#include "board_io.h"

#include "stm32f4xx_hal.h"

#define LED_R_PIN   GPIO_PIN_12
#define LED_G_PIN   GPIO_PIN_11
#define LED_B_PIN   GPIO_PIN_10
#define LED_PORT    GPIOH

#define KEY_PIN     GPIO_PIN_0
#define KEY_PORT    GPIOA
#define KEY_DEBOUNCE_MS 20u

void BoardIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* LEDs off first: the hardware is active-low. */
    HAL_GPIO_WritePin(LED_PORT, LED_R_PIN | LED_G_PIN | LED_B_PIN,
                      GPIO_PIN_SET);

    gpio.Pin = LED_R_PIN | LED_G_PIN | LED_B_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);

    gpio.Pin = KEY_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEY_PORT, &gpio);
}

void BoardIO_SetLED(uint8_t red, uint8_t green, uint8_t blue)
{
    HAL_GPIO_WritePin(LED_PORT, LED_R_PIN,
                      (red != 0u) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_G_PIN,
                      (green != 0u) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_B_PIN,
                      (blue != 0u) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

uint8_t BoardIO_KeyTakePress(void)
{
    static uint8_t raw_last = 1u;
    static uint8_t stable = 1u;
    static uint32_t last_change_ms = 0u;
    static uint8_t press_pending = 0u;

    uint32_t now = HAL_GetTick();
    uint8_t raw = (HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN) == GPIO_PIN_SET) ? 1u : 0u;

    if (raw != raw_last)
    {
        raw_last = raw;
        last_change_ms = now;
    }

    if ((now - last_change_ms) >= KEY_DEBOUNCE_MS && raw != stable)
    {
        stable = raw;
        if (stable == 0u)
        {
            press_pending = 1u;
        }
    }

    if (press_pending != 0u)
    {
        press_pending = 0u;
        return 1u;
    }

    return 0u;
}
