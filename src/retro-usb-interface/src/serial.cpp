
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

void umass_read_sector(uint32_t sector_nr, uint8_t* buffer);

namespace serial 
{
    namespace pin {
        static constexpr auto inline DTR = 3;

        static auto inline UART = uart1;
        static auto inline UART_IRQ = UART1_IRQ;
        static constexpr auto inline UART_TX = 4;
        static constexpr auto inline UART_RX = 5;

        static constexpr auto inline UART_Mouse_Baudrate = 1'200;
        static constexpr auto inline UART_Mouse_DataBits = 7;
        static constexpr auto inline UART_Mouse_StopBits = 1;
        static constexpr auto inline UART_Mouse_Parity = UART_PARITY_NONE;

        static constexpr auto inline UART_Storage_Baudrate = 115'200;
        static constexpr auto inline UART_Storage_DataBits = 8;
        static constexpr auto inline UART_Storage_StopBits = 1;
        static constexpr auto inline UART_Storage_Parity = UART_PARITY_NONE;
    }

    namespace {
        Fifo<16> transmitFifo;
        Fifo<16> receiveFifo;
        std::array<uint8_t, 512> sector_buffer;

        uint16_t UpdateCRC16(uint16_t crc, uint8_t byte)
        {
            crc = crc ^ (byte << 8);
            for(int n = 0; n < 8; ++n) {
                const auto carry = crc & 0x8000;
                crc = crc << 1;
                if (carry) {
                    crc = crc ^ 0x1021;
                }
            }
            return crc;
        }

        void ResetUart(int baudrate, int dataBits, int stopBits, uart_parity_t parity)
        {
            int x = uart_init(pin::UART, baudrate);
            uart_set_format(pin::UART, dataBits, stopBits, parity);
            uart_set_fifo_enabled(pin::UART, false);
            // TX IRQ will be enabled once it is needed
            uart_set_irq_enables(pin::UART, true, false);

            transmitFifo.clear();
            receiveFifo.clear();
        }


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
            printf("{%x}", ch);
            receiveFifo.push(std::move(ch));
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
        // Resets the port to 1200N1, for mice
        ResetUart(pin::UART_Mouse_Baudrate, pin::UART_Mouse_DataBits, pin::UART_Mouse_StopBits, pin::UART_Mouse_Parity);
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

        irq_set_enabled(pin::UART_IRQ, false);
        EnqueueByte(byte0);
        EnqueueByte(byte1);
        EnqueueByte(byte2);
        if (byte3) EnqueueByte(byte3);
        irq_set_enabled(pin::UART_IRQ, true);
    }

    void SerialMouse::Run()
    {
        const auto dtr = gpio_get(pin::DTR);
        if (std::exchange(previous_dtr_state, dtr) != dtr && !dtr) {
            printf("serial: sending mouse handshake\n");
            irq_set_enabled(pin::UART_IRQ, false);
            ResetUart(pin::UART_Mouse_Baudrate, pin::UART_Mouse_DataBits, pin::UART_Mouse_StopBits, pin::UART_Mouse_Parity);
            EnqueueByte('M');
            EnqueueByte('3');
            irq_set_enabled(pin::UART_IRQ, true);
            return;
        }

        irq_set_enabled(pin::UART_IRQ, false);
        const auto len = receiveFifo.bytes_left();
        if (len >= 2 && receiveFifo.peek(0) == '*' && receiveFifo.peek(1) == '^') {
            printf("serial: got umass handshake\n");
            // Use a busy-waiting send here - we need to ensure the bytes
            // receive their target before we reprogram the UART
            uart_write_blocking(pin::UART, reinterpret_cast<const uint8_t*>("KO"), 2);

            // Give remove side some time to read the data before we clear the FIFO
            sleep_ms(100);

            // Reprogram to storage mode
            ResetUart(pin::UART_Storage_Baudrate, pin::UART_Storage_DataBits, pin::UART_Storage_StopBits, pin::UART_Storage_Parity);
        } else if (len >= 5 && receiveFifo.peek(0) == 'R') {
            receiveFifo.drop(1);
            uint32_t sector_nr = static_cast<uint32_t>(receiveFifo.pop()) << 24;
            sector_nr |= static_cast<uint32_t>(receiveFifo.pop()) << 16;
            sector_nr |= static_cast<uint32_t>(receiveFifo.pop()) << 8;
            sector_nr |= static_cast<uint32_t>(receiveFifo.pop()) << 0;
            printf("serial: receive %d\n", sector_nr);
            umass_read_sector(sector_nr + 63, sector_buffer.data());
            uint16_t crc = 0;
            for(size_t n = 0; n < sector_buffer.size(); ++n) {
                EnqueueByte(sector_buffer[n]);
                crc = UpdateCRC16(crc, sector_buffer[n]);
            }
            EnqueueByte(crc >> 8);
            EnqueueByte(crc & 0xff);
        }
        irq_set_enabled(pin::UART_IRQ, true);
    }
}