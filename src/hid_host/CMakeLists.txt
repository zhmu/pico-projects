#set(FAMILY rp2040)
#set(BOARD pico_sdk)
#set(TINYUSB_FAMILY_PROJECT_NAME_PREFIX "tinyusb_host_")

include(${PICO_TINYUSB_PATH}/hw/bsp/family_support.cmake)

#set(TOP ${PICO_TINYUSB_PATH})
#add_subdirectory(${PICO_TINYUSB_PATH}/examples/host tinyusb_host_examples)

set(PROJECT hid-host)
#family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

# Example source
add_executable(${PROJECT}
        src/hid_app.c
        src/main.c
        src/msc_app.c
        src/cdc_app.c
)

# Make sure TinyUSB can find our tusb_config.h
target_include_directories(${PROJECT} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(hid-host PUBLIC pico_stdlib tinyusb_host tinyusb_board)

#pico_add_extra_outputs(hid-host)
#family_configure_target(${PROJECT} noos)