# Retro USB interface

I like to use my old retro computers (early to mid 1990ies) for development and gaming purposes. One thing that irks me, is the abysmal quality of PS/2 and RS-232 mice available. As it is very difficult to find a device that provides an enjoyable experience, I decided (like many others) to create an USB -> RS-232 interface.

## Hardware

- Raspberry Pico
- MAX3232CPE
- BS170 MOSFET (for DTR)

## Wiring

Inputs, from Pico side

* MAX3232: T1OUT (14) --> RS-232 RXD (2)
* MAX3232: T2OUT (7) --> RS-232 DTR (4)
* MAX3232: T1IN (11) --> Pico GP4 (UART1 TX)
* MAX3232: T2IN (10) --> Pico GP3

Outputs, from Pico side

* MAX3232: R1IN (13) --> RS-232 TXD (3)
* MAX3232: R2IN (8) --> RS-232 GP5 (UART1 RX)
* MAX3232: R1OUT (12) --> Pico RXD
* MAX3232: R2OUT (9) --> Pico ???
* 