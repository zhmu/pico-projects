include(${PICO_TINYUSB_PATH}/hw/bsp/family_support.cmake)

set(PROJECT retro-usb-interface)

add_executable(${PROJECT}
        src/main.cpp
        src/umass.cpp
        src/uhid.cpp
)
target_link_libraries(${PROJECT} PUBLIC pico_stdlib pico_time tinyusb_host tinyusb_board)

# TinyUSB expects to be able to include tusb_config.h from wherever, so
# add it to the include path
target_include_directories(${PROJECT} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src/config)
