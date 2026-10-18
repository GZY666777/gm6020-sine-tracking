# Third-party notices

This project is based on the RoboMaster C board example project and includes
STMicroelectronics driver code. The original copyright notices are kept in the
individual files.

## RoboMaster Development-Board-C-Examples

- Source: https://github.com/RoboMaster/Development-Board-C-Examples
- Used as the project skeleton: `14.CAN` CubeMX configuration, startup file,
  HAL configuration, `bsp_can.c` filter setup, Keil project structure.
- Copyright (C) DJI / RoboMaster. See the upstream repository for its license
  terms.

## STM32F4xx HAL Driver

- Source: STMicroelectronics STM32CubeF4, `STM32F4xx_HAL_Driver`.
- License: BSD 3-Clause. The license header is retained in every HAL source
  and header file.
- Additional files added for this project:
  - `stm32f4xx_hal_uart.c/.h`
  - `stm32f4xx_hal_iwdg.c/.h`

## CMSIS

- Source: ARM / STMicroelectronics CMSIS device headers.
- License: Apache-2.0 (CMSIS) with ST device header terms; original headers
  are retained.

## DJI GM6020 protocol

- CAN IDs, frame layout and scaling follow the public RoboMaster GM6020 motor
  documentation. This repository is not affiliated with DJI.
