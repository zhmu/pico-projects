/*-
 * SPDX-License-Identifier: CC-BY-4.0
 *
 * Copyright (c) 2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "pico/stdlib.h"

namespace pin {
    static auto inline UART = uart0;
    static constexpr auto inline UART_Baudrate = 115'200;
    static constexpr auto inline UART_TX = 0;
    static constexpr auto inline UART_RX = 1;
}

int main()
{
    uart_init(pin::UART, pin::UART_Baudrate);
    gpio_set_function(pin::UART_TX, GPIO_FUNC_UART);
    gpio_set_function(pin::UART_RX, GPIO_FUNC_UART);

    while(true)
    {
        uart_puts(pin::UART, "Hello world!\n");
        sleep_ms(1000);
    }
    return 0;
}
