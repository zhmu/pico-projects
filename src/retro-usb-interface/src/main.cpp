/*-
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2023 Rink Springer
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "bsp/board_api.h" // for board_init()
#include "tusb.h"

namespace pin {
    static constexpr auto inline LED1 = 25;
}

namespace
{
    struct LedBlinkTask
    {
        constexpr static inline auto intervalMs = 1'000;
        uint32_t start_ms = 0;
        bool led_state = false;

        void Run()
        {
            const auto uptimeInMs = to_ms_since_boot(get_absolute_time());
            if (uptimeInMs - start_ms < intervalMs) return; // not enough time
            start_ms += intervalMs;

            gpio_put(pin::LED1, led_state);
            led_state = !led_state;
        }
    };
}

int main()
{
    board_init();

    printf("Retro USB interface: initializing\n");

    tuh_init(BOARD_TUH_RHPORT);
    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    gpio_init(pin::LED1);
    gpio_set_dir(pin::LED1, GPIO_OUT);
    printf("Retro USB interface: ready\n");

    LedBlinkTask blinkTask;
    while (1) {
        tuh_task();
        blinkTask.Run();
    }
}