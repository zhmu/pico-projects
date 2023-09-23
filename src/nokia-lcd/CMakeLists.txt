add_executable(nokia_lcd
        nokia_lcd.cpp
        fonts/nokia-fonts.c
)

# pull in common dependencies and additional pwm hardware support
target_link_libraries(nokia_lcd pico_stdlib hardware_pwm)

# create map/bin/hex file etc.
pico_add_extra_outputs(nokia_lcd)