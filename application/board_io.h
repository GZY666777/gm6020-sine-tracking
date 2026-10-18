/**
 * @file    board_io.h
 * @brief   On-board RGB LED and user key (RoboMaster C board).
 *
 * LED pins are active-low: PH10 blue, PH11 green, PH12 red.
 * User key is PA0, active-low with a pull-up.
 */

#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdint.h>

void BoardIO_Init(void);

/** Returns 1 exactly once for each debounced key press. */
uint8_t BoardIO_KeyTakePress(void);

void BoardIO_SetLED(uint8_t red, uint8_t green, uint8_t blue);

#endif /* BOARD_IO_H */
