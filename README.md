# Introduction

This is my development environment for Raspberry Pico-based microcontrollers. It relies on Visual Studio Code to perform the debugging task - projects can be build both using the command line as well as within Visual Studio Code.

I use standard Raspberry Pico's with a Raspberry Pico Debug Probe. My development machine runs Debian/testing, which is currently _bookworm_ (Debian 12).

## Setting up

There are nested submodules in here, so start by executing:

```sh
git submodule update --init --recursive
```

### Prerequisites

You'll need the following prerequisites:

```sh
apt install gcc-arm-none-eabi binutils-arm-none-eabi gdb-multiarch cmake ninja-build
```

### Debugging as non-root user

In order to be able to debug as non-root user, create `/etc/udev/rules.d/70-picoprobe.rules` and put the following content in it:

```
ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000c", MODE="0666"
```

You'll need to reload the udev ruleset:

```sh
udevadm control --reload-rules && udevadm trigger
```

### Building Pico-specific OpenOCD

These instructions are based on the [Raspberry Pi Debug Probe documentation](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html#installing-tools), with the exception that I install to `/opt/openocd-pico` to avoid interfering with regular OpenOCD.

```sh
$ git clone https://github.com/raspberrypi/openocd.git --branch rp2040 --depth=1
$ cd openocd
$ ./bootstrap
$ ./configure --disable-werror --prefix=/opt/openocd-pico
$ make -j
$ sudo make install
```

## Building from the command line

```sh
mkdir build
cd build
cmake -GNinja -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/arm-none-eabi-gcc ..
ninja
```

## Connecting the Raspberry Pico to the debug probe

My Raspberry Pico's do not have the small 3-pin debug connector. I did solder a pinheader on them to connect the wires.

The connector is denoted _DEBUG_ and has a marker for pin 1. I connect the breakout cable as follows:

- Pin 1 to orange (SWCLK)
- Pin 2 to black (GND)
- Pin 3 to yellow (SWDIO)

## Developing / debugging using Visual Studio Code

Visual Studio Code tends to suggest which extensions are necessary, so I just run with that.

- Select CMake kit: `GCC 12.2.1 arm-none-eabi`
- In the status bar at the bottom, there will be a _bug_ and _play_ icon next to each other. Left to that is the _target to launch_. Click this to change the program to debug.
- Select _Run And Debug_ in the panel to the left (Control-Shift-D)
- At the top left, there is a green _play_ icon with _Pico Debug_ next to it. Click the icon to start (F5)

# Licensing

All my source code and assorted files (build scripts, etc) are licensed using [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Exceptions are denoted in the respective files.