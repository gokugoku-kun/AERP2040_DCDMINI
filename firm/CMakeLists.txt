cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
include(pico_extras_import.cmake)

project(pico_dcdmini C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(pico_dcdmini)

pico_generate_pio_header(pico_dcdmini ${CMAKE_CURRENT_LIST_DIR}/audio_pwm.pio)
pico_generate_pio_header(pico_dcdmini ${CMAKE_CURRENT_LIST_DIR}/lcd.pio)

target_sources(pico_dcdmini PRIVATE
        main.c
        audio_pwm.c
        lcd.c
        lcd2.c
        led.c
        )

target_compile_definitions(pico_dcdmini PRIVATE
        PICO_SD_CLK_PIN=17
        PICO_SD_CMD_PIN=18
        PICO_SD_DAT0_PIN=19
        )

target_link_libraries(pico_dcdmini PRIVATE
        pico_stdlib
        pico_platform
        pico_audio
        pico_sd_card
        hardware_dma
        hardware_spi
        hardware_pio
        hardware_i2c
        )
pico_enable_stdio_usb(pico_dcdmini 1)
pico_enable_stdio_uart(pico_dcdmini 0)
pico_add_extra_outputs(pico_dcdmini)
