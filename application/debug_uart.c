/**
 * @file    debug_uart.c
 * @brief   USART1 polling driver used for telemetry and commands.
 */

#include "debug_uart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"

UART_HandleTypeDef huart1;

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance != USART1)
    {
        return;
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PA9 = USART1_TX */
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* PB7 = USART1_RX */
    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &gpio);
}

void DebugUART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200u;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    (void)HAL_UART_Init(&huart1);
}

int DebugUART_ReadChar(void)
{
    uint8_t ch = 0u;

    if (HAL_UART_Receive(&huart1, &ch, 1u, 0u) == HAL_OK)
    {
        return (int)ch;
    }

    return -1;
}

void DebugUART_Write(const char *text)
{
    if (text == 0)
    {
        return;
    }

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)text,
                            (uint16_t)strlen(text), 50u);
}

void DebugUART_Printf(const char *fmt, ...)
{
    char buffer[192];
    va_list args;
    int length;

    if (fmt == 0)
    {
        return;
    }

    va_start(args, fmt);
    length = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (length <= 0)
    {
        return;
    }

    if (length >= (int)sizeof(buffer))
    {
        length = (int)sizeof(buffer) - 1;
    }

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)buffer,
                            (uint16_t)length, 50u);
}
