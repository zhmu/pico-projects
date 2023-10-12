
/*
 * As outlined in https://linux.die.net/man/4/mouse:
 *
 * The mouse driver can recognize a mouse by dropping RTS to low and raising it
 * again. About 14 ms later the mouse will send 0x4D (aqMaq) on the data line.
 * (After a further 63 ms, a Microsoft-compatible 3-button mouse will send 0x33
 * (aq3aq))
 * 
 * The relative mouse movement is sent as dx (positive means right) and dy
 * (positive means down). Various mice can operate at different speeds. To
 * select speeds, cycle through the speeds 9600, 4800, 2400 and 1200 bit/s, each
 * time writing the two characters from the table below and waiting 0.1 seconds.
 * The following table shows available speeds and the strings that select them:
 *
 * bit/s 	string
 * 9600 	*q
 * 4800 	*p
 * 2400 	*o
 * 1200 	*n
 * 
 * Default: 1200N1, 7 bits
 *
 * byte  d6   d5   d4   d3   d2   d1   d0
 *    1   1   lb   rb  dy7  dy6  dx7  dx6
 *    2   0  dx5  dx4  dx3  dx2  dx1  dx0
 *    3   0  dy5  dy4  dy3  dy2  dy1  dy0
 *   (4   0    1    0    0    0    0    0) - if middle button is down
 *
 * Logitech protocol
 * Logitech serial 3-button mice use a different extension of the Microsoft
 * protocol: when the middle button is up, the above 3-byte packet is sent. When
 * the middle button is down a 4-byte packet is sent, where the 4th byte has
 * value 0x20 (or at least has the 0x20 bit set). In particular, a press of the
 * middle button is reported as 0,0,0,0x20 when no other buttons are down. 
 *
 * 
 * http://www.osdever.net/documents/PNP-ExternalSerial-v1.00.pdf
 * 
 * - Set DTR=1
 * - Wait for DSR=1
 * 
 * Nodig:
 * 
 * - RXD, TXD
 * - DTR, RTS moeten ontvangen kunnen worden
 *   Als RTS van 0 -> 1, dan signature zenden (0x33)
 * - DSR moet gezet kunnen worden (?)
 * 
 * https://web.stanford.edu/class/ee281/projects/aut2002/yingzong-mouse/media/Serial%20Mouse%20Detection.pdf
 * - 2.2.1.3
 * - 
 */

#include "serial.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <utility>
#include "mouse.h"
#include "fifo.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"

namespace serial 
{
    namespace pin {
        static constexpr auto inline DTR = 3;

        static auto inline UART = uart1;
        static auto inline UART_IRQ = UART1_IRQ;
        static constexpr auto inline UART_Baudrate = 1'200;
        static constexpr auto inline UART_TX = 4;
        static constexpr auto inline UART_RX = 5;
    }

    namespace {
        Fifo<16> transmitFifo;
        Fifo<16> receiveFifo;

        void TransmitEnqueuedByte()
        {
            const auto ch = transmitFifo.pop();
            uart_putc_raw(pin::UART, ch);
            uart_set_irq_enables(pin::UART, true, !transmitFifo.empty());
        }

        void EnqueueByte(uint8_t ch)
        {
            const auto bufferEmpty = transmitFifo.empty();
            transmitFifo.push(std::move(ch));
            if (bufferEmpty) {
                TransmitEnqueuedByte();
            }
        }
    }

    void OnUartIrq()
    {
        while(uart_is_readable(pin::UART)) {
            auto ch = uart_getc(pin::UART);
            transmitFifo.push(std::move(ch));
        }

        if (uart_is_writable(pin::UART)) {
            if (transmitFifo.empty()) {
                // Disable TX-empty interrupt, we have nothing left to send
                uart_set_irq_enables(pin::UART, true, false);
            } else {
                TransmitEnqueuedByte();
            }
        }
    }

    SerialMouse::SerialMouse()
    {
        gpio_init(pin::DTR);
        gpio_set_dir(pin::DTR, GPIO_IN);
        gpio_set_function(pin::UART_RX, GPIO_FUNC_UART);
        gpio_set_function(pin::UART_TX, GPIO_FUNC_UART);

        irq_set_exclusive_handler(pin::UART_IRQ, OnUartIrq);
        irq_set_enabled(pin::UART_IRQ, true);
        Reset();
    }

    void SerialMouse::Reset()
    {
        // Resets the port to 1200N1, for mice
        uart_init(pin::UART, pin::UART_Baudrate);
        uart_set_format(pin::UART, 7, 1, UART_PARITY_NONE);
        uart_set_fifo_enabled(pin::UART, false);
        // IRQ will be enabled once it is needed
        uart_set_irq_enables(pin::UART, true, false);
    }

    void SerialMouse::SendEvent(const mouse::MouseEvent& event)
    {
        const auto x = event.delta_x / 2;
        const auto y = event.delta_y / 2;

        /*
         *
         * byte  d6   d5   d4   d3   d2   d1   d0
         *    1   1   lb   rb  dy7  dy6  dx7  dx6
         *    2   0  dx5  dx4  dx3  dx2  dx1  dx0
         *    3   0  dy5  dy4  dy3  dy2  dy1  dy0
         *   (4   0    1    0    0    0    0    0) - if middle button is down
         */
        uint8_t byte0 = 0b100'0000;
        if (event.button & mouse::ButtonLeft) byte0 |= 0b010'0000;
        if (event.button & mouse::ButtonRight) byte0 |= 0b001'0000;
        byte0 |= ((y >> 6) & 0b11) << 2;
        byte0 |= ((x >> 6) & 0b11) << 0;

        const uint8_t byte1 = (x & 0b0011'1111);
        const uint8_t byte2 = (y & 0b0011'1111);
        const uint8_t byte3 = (event.button & mouse::ButtonMiddle) ? 0b010'0000 : 0;

        const auto irq_state = save_and_disable_interrupts();
        EnqueueByte(byte0);
        EnqueueByte(byte1);
        EnqueueByte(byte2);
        if (byte3) EnqueueByte(byte3);
        restore_interrupts(irq_state);
    }

    void SerialMouse::Run()
    {
        const auto dtr = gpio_get(pin::DTR);
        if (std::exchange(previous_dtr_state, dtr) == dtr) return;
        if (!dtr) {
            printf("serial: sending mouse handshake\n");
            const auto irq_state = save_and_disable_interrupts();
            Reset();
            EnqueueByte('M');
            EnqueueByte('3');
            restore_interrupts(irq_state);
        }
    }
}