# Retro USB interface

I like to use my old retro computers (early to mid 1990ies) for development and gaming purposes. One thing that irks me, is the abysmal quality of PS/2 and RS-232 mice available. As it is very difficult to find a device that provides an enjoyable experience, I decided (like many others) to create an USB -> RS-232 interface.

## Hardware

- Raspberry Pico
- MAX3232CPE
- BS170 MOSFET (for DTR)

## Wiring

The Kicad schematics are in the `hw/` folder.

## Keyboard support

There is preliminary keyboard support in the source tree, which is not enabled by default. The goal was to be able to use both USB mice and keyboards with this project, but this was not finished.

## Flashing

Hold the BOOTSEL button on the Pico, connect an USB cable to your Linux system and invoke the following command:

```
sudo picotool load build/src/retro-usb-interface/retro-usb-interface.uf2
```
