/*-
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * Copyright (c) 2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "pico/stdlib.h"

namespace pin {
    static constexpr auto inline LED1 = 25;
    static constexpr auto inline LED2 = 27;
    static constexpr auto inline LED3 = 28;
}

int main()
{
    gpio_init(pin::LED1);
    gpio_init(pin::LED2);
    gpio_init(pin::LED3);
    gpio_set_dir(pin::LED1, GPIO_OUT);
    gpio_set_dir(pin::LED2, GPIO_OUT);
    gpio_set_dir(pin::LED3, GPIO_OUT);
    int n = 0;
    while (true) {
        gpio_put(pin::LED1, n == 0);
        gpio_put(pin::LED2, n == 1);
        gpio_put(pin::LED3, n == 2);
        n = (n + 1) % 3;
        sleep_ms(1000);
    }
    return 0;
}
