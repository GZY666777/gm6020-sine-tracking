/**
 * @file    debug_uart.h
 * @brief   USART1 debug console / CSV telemetry (115200 8N1).
 *
 * Pins on the RoboMaster C board:
 *   PA9  -> USART1_TX
 *   PB7  -> USART1_RX
 * Connect a USB-TTL adapter: adapter RX to PA9, adapter TX to PB7, GND to GND.
 */

#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#include <stdint.h>

void DebugUART_Init(void);

/** Non-blocking read: returns 0..255, or -1 when the RX register is empty. */
int DebugUART_ReadChar(void);

void DebugUART_Write(const char *text);

/** printf-style output.  Integer formats only, to keep the binary small. */
void DebugUART_Printf(const char *fmt, ...);

#endif /* DEBUG_UART_H */
