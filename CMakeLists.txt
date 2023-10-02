cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/pico-sdk)
set(PICO_EXTRAS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/pico-extras)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(pico_examples C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

pico_sdk_init()

add_subdirectory(src)
