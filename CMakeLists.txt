cmake_minimum_required(VERSION 3.20)

project(GM6020_Sine C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(USE_HAL_DRIVER STM32F407xx)

add_compile_options(
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra
    -Wno-unused-parameter
)

add_compile_options(
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Release>:-g>
)

include_directories(
    Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
    application
    bsp/boards
)

set(APP_SOURCES
    Src/main.c
    Src/gpio.c
    Src/can.c
    Src/stm32f4xx_it.c
    Src/stm32f4xx_hal_msp.c
    Src/system_stm32f4xx.c
    application/app.c
    application/motor_gm6020.c
    application/sine_profile.c
    application/pid.c
    application/safety.c
    application/debug_uart.c
    application/board_io.c
    bsp/boards/bsp_can.c
)

set(HAL_SOURCES
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
)

add_executable(GM6020_Sine
    ${APP_SOURCES}
    ${HAL_SOURCES}
    cmake/startup_stm32f407xx.s
)

set_target_properties(GM6020_Sine PROPERTIES SUFFIX ".elf")

target_link_options(GM6020_Sine PRIVATE
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -T${CMAKE_SOURCE_DIR}/cmake/STM32F407IGHx_FLASH.ld
    -specs=nosys.specs
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_BINARY_DIR}/GM6020_Sine.map
)

target_link_libraries(GM6020_Sine PRIVATE m)

add_custom_command(TARGET GM6020_Sine POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE_NAME:GM6020_Sine>
            GM6020_Sine.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE_NAME:GM6020_Sine>
            GM6020_Sine.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE_NAME:GM6020_Sine>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating GM6020_Sine.bin / GM6020_Sine.hex"
)
