cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(base_node)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
    src/main.c
    src/nus_client.c
    src/mobile_link.c
    src/json_serial.c
    src/audio_out.c
    src/serial_audio.c
)