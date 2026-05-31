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
    "0.8.21-refactor-clock-styles"
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
    src/alarm/alarm_model.cpp
    src/alarm/alarm_ui.cpp
    src/alarm_sound.cpp
    src/app/screenshot_service.cpp
    src/app/settings_editor.cpp
    src/app/set_time_editor.cpp
    src/app/uart_commands.cpp
    src/clock/analog_render.cpp
    src/clock/calendar_render.cpp
    src/clock/clock_help.cpp
    src/clock/clock_render.cpp
    src/clock/clock_time.cpp
    src/diagnostics/screenshot_capture.cpp
    src/life/life_runtime.cpp
    src/life_board.cpp
    src/main.cpp
    src/platform/backlight_control.cpp
    src/platform/battery.cpp
    src/platform/fatfs_time.cpp
    src/platform/startup_probe.cpp
    src/rtc/ds3231.c
    src/platform/picocalc_audio_pwm.cpp
    src/platform/picocalc_display.cpp
    src/platform/picocalc_keyboard.cpp
    src/platform/sd/fatfs_diskio.cpp
    src/platform/sd/picocalc_sdcard.cpp
    src/platform/picocalc_uart_log.cpp
    src/settings/settings_store.cpp
    third_party/ChanFatFS/src/ff.c
    third_party/ChanFatFS/src/ffunicode.c
)

target_include_directories(Picocalc_Clock PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_LIST_DIR}/src/platform
    ${CMAKE_CURRENT_LIST_DIR}/src/config
    ${CMAKE_CURRENT_LIST_DIR}/src/font
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc
    ${CMAKE_CURRENT_LIST_DIR}/third_party/ChanFatFS/include
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
    PICOMENT_SCREENSHOT_CAPTURE_BUILD=1
    PICOMENT_ENABLE_SD_WRITE=1
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
    hardware_spi
)

pico_generate_pio_header(Picocalc_Clock
    ${CMAKE_CURRENT_LIST_DIR}/src/platform/lcd_spi_min.pio
)

pico_enable_stdio_usb(Picocalc_Clock 0)
pico_enable_stdio_uart(Picocalc_Clock 1)

pico_add_extra_outputs(Picocalc_Clock)
