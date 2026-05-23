cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(Picocalc_Clock C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(PICOCALC_CLOCK_BUILD_INFO
    ${CMAKE_CURRENT_BINARY_DIR}/generated/picocalc_clock_build_info.h
)
set(PICOCALC_CLOCK_BUILD_PURPOSE
    "0.8.0-analog-clock"
)

add_custom_target(generate_picocalc_clock_build_info ALL
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}
        -DOUT_FILE=${PICOCALC_CLOCK_BUILD_INFO}
        -DBUILD_PURPOSE=${PICOCALC_CLOCK_BUILD_PURPOSE}
        -P ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_build_info.cmake
    BYPRODUCTS ${PICOCALC_CLOCK_BUILD_INFO}
    VERBATIM
)

add_executable(Picocalc_Clock
    src/alarm_sound.cpp
    src/main.cpp
    src/rtc/ds3231.c
    src/platform/picocalc_audio_pwm.cpp
    src/platform/picocalc_display.cpp
    src/platform/picocalc_keyboard.cpp
    src/platform/picocalc_uart_log.cpp
)

target_include_directories(Picocalc_Clock PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_LIST_DIR}/src/platform
    ${CMAKE_CURRENT_LIST_DIR}/src/config
    ${CMAKE_CURRENT_LIST_DIR}/src/font
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc
)

add_dependencies(Picocalc_Clock generate_picocalc_clock_build_info)

target_compile_options(Picocalc_Clock PRIVATE
    -O2
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_compile_definitions(Picocalc_Clock PRIVATE
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_DEFAULT_UART_BAUD_RATE=115200
    PICOCLOCK_BUILD_RELEASE
    PICOCLOCK_COMMAND_UART_ENABLED=1
    PICOMENT_FONT_SAMPLE_BUILD=1
    PICOMENT_SCREENSHOT_CAPTURE_BUILD=0
)

target_link_libraries(Picocalc_Clock
    pico_stdlib
    hardware_gpio
    hardware_clocks
    hardware_dma
    hardware_i2c
    hardware_irq
    hardware_pio
    hardware_pwm
)

pico_generate_pio_header(Picocalc_Clock
    ${CMAKE_CURRENT_LIST_DIR}/src/platform/lcd_spi_min.pio
)

pico_enable_stdio_usb(Picocalc_Clock 0)
pico_enable_stdio_uart(Picocalc_Clock 1)

pico_add_extra_outputs(Picocalc_Clock)
