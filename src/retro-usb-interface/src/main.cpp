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
#include <array>
#include <deque>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "bsp/board.h" // for board_init()
#include "serial.h"
#include "mouse.h"
#include "tusb.h"

namespace pin {
    static constexpr auto inline LED1 = 25;

    static constexpr auto inline KeyboardClockN = 10;
    static constexpr auto inline KeyboardDataN = 11;
    static constexpr auto inline KeyboardClockReadN = 12;
    static constexpr auto inline KeyboardDataReadN = 13;
    static constexpr auto inline DebugOut1 = 15;
    static constexpr auto inline DebugOut2 = 16;
    static constexpr auto inline DebugOut3 = 17;
}

namespace
{
    std::deque<uint8_t> bytesToSend;

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

    struct KeyboardTask
    {
        constexpr static inline auto intervalMs = 100;
        uint32_t start_ms = 0;
        bool led_state = false;

        KeyboardTask()
        {
            gpio_init(pin::KeyboardClockN);
            gpio_init(pin::KeyboardDataN);
            gpio_init(pin::KeyboardClockReadN);
            gpio_init(pin::KeyboardDataReadN);
            gpio_init(pin::DebugOut1);
            gpio_init(pin::DebugOut2);
            gpio_init(pin::DebugOut3);
            gpio_set_dir(pin::KeyboardClockN, GPIO_OUT);
            gpio_set_dir(pin::KeyboardDataN, GPIO_OUT);
            gpio_set_dir(pin::DebugOut1, GPIO_OUT);
            gpio_set_dir(pin::DebugOut2, GPIO_OUT);
            gpio_set_dir(pin::DebugOut3, GPIO_OUT);
            gpio_set_dir(pin::KeyboardClockReadN, GPIO_IN);
            gpio_set_dir(pin::KeyboardDataReadN, GPIO_IN);
            gpio_put(pin::DebugOut1, 0);
            gpio_put(pin::DebugOut2, 0);
            gpio_put(pin::DebugOut3, 0);

            // Place both GPIO's high to signal idle bus
            gpio_put(pin::KeyboardClockN, 1);
            gpio_put(pin::KeyboardDataN, 1);
        }

        static void ClockPulse()
        {
            gpio_put(pin::KeyboardClockN, 0);
            busy_wait_us(400);
            gpio_put(pin::KeyboardClockN, 1);
            busy_wait_us(400);
        }

        static int SendByte(uint8_t scancode)
        {
            gpio_put(pin::DebugOut3, 1);
            busy_wait_us(100);

            // Calculate parity
            int parity = 1;
            for(int n = 0; n < 8; ++n) {
                if (scancode & (1 << n))
                    parity ^= 1;
            }

            // Wait until the clock is high
            while(gpio_get(pin::KeyboardClockReadN) == 0);

            // Generate the data packt
            //uint16_t v = 0b11'00011110'0;
            uint16_t v = 0b10'00000000'0;
            /*             ^^ \------/ ^*/
            /*        stop-/|   data   |*/
            /*            parity     start*/
            v |= (static_cast<uint16_t>(scancode) << 1);
            v |= (parity << 9);
            for(int n = 0; n < 11; ++n) {
                if (gpio_get(pin::KeyboardClockReadN) == 0) {
                    printf("SendByte: host messing with the clock, aborting\n");
                    // Release bus
                    gpio_put(pin::KeyboardDataN, 1);
                    gpio_put(pin::KeyboardClockN, 1);
                    // Pulse debug2 to make this visible
                    gpio_put(pin::DebugOut2, 1);
                    busy_wait_us(10);
                    gpio_put(pin::DebugOut2, 0);
                    return 0;
                }
                gpio_put(pin::KeyboardDataN, v & 1);
                ClockPulse();
                v >>= 1;
            }

            // Release data/clock
            gpio_put(pin::KeyboardDataN, 1);
            gpio_put(pin::KeyboardClockN, 1);
            busy_wait_us(400);
            gpio_put(pin::DebugOut3, 0);
            return 1;
        }

        void Run()
        {
            static int prev_clock = 0;

            uint32_t intr = save_and_disable_interrupts();
            if (gpio_get(pin::KeyboardClockReadN) == 0 && gpio_get(pin::KeyboardDataReadN) == 0)
            {
                // Clock is LO here (driven by the host), we do not drive data...
                printf("Detected host RTS\n");
                gpio_put(pin::DebugOut1, 1);

                // Wait until host releases clock
                while(gpio_get(pin::KeyboardClockReadN) == 0);

                // Clock in the data
                uint16_t result = 0;
                int num_ones = 0;
                for(unsigned int n = 0; n < 10; ++n) {
                    if (gpio_get(pin::KeyboardDataReadN)) {
                        result |= (1 << n);
                        ++num_ones;
                    }
                    ClockPulse();
                }
                result >>= 1; // throw away first bit, it's always zero
                printf("result %x num_ones %d -> data byte %x\n", result, num_ones, result & 0xff);

                // Wait until DATA is released (it goes back hi)
                while(gpio_get(pin::KeyboardDataReadN) == 0);

                gpio_put(pin::DebugOut2, 1);

                // Acknowledge by bringing data low
                gpio_put(pin::KeyboardDataN, 0);
                // Generate clock
                ClockPulse();

                gpio_put(pin::KeyboardDataN, 1);

                gpio_put(pin::DebugOut2, 0);
                gpio_put(pin::DebugOut1, 0);

                result &= 0xff;
                if (result == 0xff) {
                    printf("RESET command\n");
                    bytesToSend.push_back(0xfa);
                    bytesToSend.push_back(0xaa);
                } else {
                    printf("unknown command %x\n");
                    bytesToSend.push_back(0xfa);
                }
            }

#if 0
            const auto uptimeInMs = to_ms_since_boot(get_absolute_time());
            if (uptimeInMs - start_ms < intervalMs) return; // not enough time
            start_ms += intervalMs;
#endif
            if (!bytesToSend.empty())
            {
                uint8_t byte = bytesToSend.front();
                printf("sending byte %x\n", byte);
                if (SendByte(byte)) {
                    bytesToSend.pop_front();
                }
            }

#if 0
            printf("KEY!\n");
            // Scancode set 2
            constexpr auto codes = std::to_array<uint8_t>({ 0x33, 0x24, 0x4b, 0x4b, 0x44, 0x5a });
            for(auto c: codes) {
                SendByte(c);
                SendByte(0x80 | c);
            }
#endif
            restore_interrupts(intr);
        }
    };
}

int main()
{
    board_init();

    printf("Retro USB interface: initializing\n");

    //tuh_init(BOARD_TUH_RHPORT);

    gpio_init(pin::LED1);
    gpio_set_dir(pin::LED1, GPIO_OUT);

    LedBlinkTask blinkTask;
    serial::SerialMouse serialMouse;
    KeyboardTask keyboardTask;

    printf("Retro USB interface: ready\n");
    while (1) {
        //tuh_task();
        blinkTask.Run();
        //serialMouse.Run();
        keyboardTask.Run();

#if 0
        if (auto event = mouse::RetrieveAndResetPendingEvent(); event) {
            serialMouse.SendEvent(*event);
        }
#endif
    }
}
