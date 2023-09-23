add_executable(pwm_led
        pwm_led.cpp
        )

# pull in common dependencies and additional pwm hardware support
target_link_libraries(pwm_led pico_stdlib hardware_pwm)

# create map/bin/hex file etc.
pico_add_extra_outputs(pwm_led)