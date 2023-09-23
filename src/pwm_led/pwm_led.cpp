/*-
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * Copyright (c) 2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "pico/stdlib.h"
#include <stdio.h>
#include "pico/time.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

namespace pin {
    static constexpr auto inline LED1 = 25;
    static constexpr auto inline LED2 = 27;
    static constexpr auto inline LED3 = 28;
}

struct LedPwm
{
    const int gpio;
    int value = 0;
    int fade_delta = 1;

    LedPwm(int gpio)
        : gpio(gpio)
    {
        gpio_set_function(gpio, GPIO_FUNC_PWM);

        // Figure out which slice we just connected to the LED pin
        uint slice_num = pwm_gpio_to_slice_num(gpio);

        // Get some sensible defaults for the slice configuration. By default, the
        // counter is allowed to wrap over its maximum range (0 to 2**16-1)
        pwm_config config = pwm_get_default_config();
        // Set divider, reduces counter clock to sysclock/this value
        pwm_config_set_clkdiv(&config, 4.f);
        // Load the configuration into our PWM slice, and set it running.
        pwm_init(slice_num, &config, true);
    }

    void update()
    {
        // Clear the interrupt flag that brought us here
        //pwm_clear_irq(pwm_gpio_to_slice_num(pin::LED2));

        value += fade_delta;
        if (value == 0 || value == 255)
        {
            fade_delta = -fade_delta;
        }
        pwm_set_gpio_level(gpio, value * value);
    }
};

int main() {
    LedPwm pwm1(pin::LED1);
    LedPwm pwm2(pin::LED2);
    LedPwm pwm3(pin::LED3);

    while (1) {
        pwm1.update();
        pwm2.update();
        pwm3.update();
        sleep_ms(5);
    }
}
