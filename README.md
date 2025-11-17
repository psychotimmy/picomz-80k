# Pico MZ 
## A Sharp MZ-80K, MZ-80A and MZ-700 Emulator for the Raspberry Pico and Pico 2 Microcontrollers

A Raspberry Pico (RP2040) and Pico 2 (RP2350) implementation of the Sharp MZ-80K, MZ-80A and MZ-700. It makes use of a Pimoroni VGA demo base, RC2014 Pi Pico VGA card or the RC2014 RP2040 VGA card.

## Documentation

Full user and systems documentation is provided in the [documentation subdirectory](https://github.com/psychotimmy/picomz-80k/tree/main/documentation).

### Using the Pico MZ as a MZ-80K or MZ-80A

Flash one of:

**picomz-80ka-pimoroni.uf2** for a Pico mounted on a Pimoroni VGA demo base

**pico2mz-80ka-pimoroni.uf2** for a Pico 2 mounted on a Pimoroni VGA demo base

**picomz-80ka-rc2014.uf2** for a RC2014 RP2040 VGA card or a RC2014 Pi Pico VGA card with a microSD card breakout mounted on a RC2014 backplane

Ensure a microSD card containing one or more Sharp MZ series digital software files (.mzf/.mzt/.m12) is installed. **The Pico MZ will not boot to the monitor screen unless a microSD card is installed.**

When a USB keyboard and VGA display is connected, you will see:

**  MONITOR SP-1002  **

\*

on the screen.

The MZ-80K is the default emulator configuration. 

To boot the emulator into MZ-80A mode, press and hold the 'A' button on your Pico's carrier board at power on, until the SA-1510 monitor announces itself with a beep and:

**  MONITOR SA-1510  **

\*

is displayed on the screen.

### Using the Pico MZ as a MZ-700

Flash one of:

**picomz-700-pimoroni.uf2** for a Pico mounted on a Pimoroni VGA demo base

**pico2mz-700-pimoroni.uf2** for a Pico 2 mounted on a Pimoroni VGA demo base

Ensure a microSD card containing one or more Sharp MZ series digital software files (.mzf/.mzt/.m12) is installed. **The Pico MZ will not boot to the monitor screen unless a microSD card is installed.**

When a USB keyboard and VGA display is connected, you will see:

**  MONITOR 1Z-013A  **

\*

### All versions 

If the Pico's green led (or the RC2014 RP2040 VGA card's white led) is flashing quickly (200ms between flashes), it means that a USB keyboard has not been recognised. 

If the Pico's green led (or the RC2014 RP2040 VGA card's white led) is flashing slowly (1s between flashes), then your microSD card cannot be read or has not been installed.

If **either** of these error conditions occur, **the Pico MZ will not display the monitor prompt until the problem is resolved.** 

To find a file to load from the microSD card, use the F1 key to browse its contents forwards, F2 to go backwards. 

## Brief developer notes

Pico MZ version 3.0.0 works with the current Pico SDK 2.2.0 master branch (latest stable)

(Pico MZ versions 1.1.0 to 2.0.1 work with Pico SDK 2.1.x. Earlier releases use Pico SDK 1.5.1)

### Release highlights

Release 1.1.0 introduced Pico 2 support.

Release 1.2.0 introduced RC2014 RP2040 VGA terminal card support.

Release 1.2.3 introduced RC2014 Pi Pico VGA terminal card support.

Release 1.2.4 introduced the ability (F6 key) to toggle between the UK and Japanese CGROMs on the MZ-80K. 

Release 2.0.0 introduced MZ-80A emulation (select by holding down button A at power on).

Release 3.0.0 introduced MZ-700 emulation (separate .uf2 files for MZ-700 and MZ-80K/A versions).

## Instructions for rebuilding the Pico MZ (see also the documentation subdirectory)

### Pre-requisites for Raspberry Pi OS (Debian Bookworm)

CMake (version 3.13 or later) and a gcc cross compiler.
```
   sudo apt install cmake
   sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```   
The Pico MZ relies on the latest stable release of the Raspberry Pico SDK. This, along with
the Pico Extras repository must be available on your computer if you wish to re-compile the emulator.

Assuming that you are already in the subdirectory in which you wish to install the Pico SDK, Pico Extras 
and Pico MZ repositories, issue the commands:
```
   git clone --recursive https://github.com/raspberrypi/pico-sdk.git -b master
   git clone https://github.com/raspberrypi/pico-extras.git -b master
```   
Then clone **either** the current release of the Pico MZ repository:
```
   git clone https://github.com/psychotimmy/picomz-80k.git -b 3.0.0
```
**or** the latest stable version:
```
   git clone https://github.com/psychotimmy/picomz-80k.git -b main
```
Ensure that the Pico SDK and Pico Extras subdirectories have been exported. 

For example, if these libraries have been installed under /home/pi, use:
```
   export PICO_SDK_PATH=/home/pi/pico-sdk
   export PICO_EXTRAS_PATH=/home/pi/pico-extras
```   
For Pico (RP2040) builds, issue the commands:
```
   cd picomz-80k
   mkdir build2040
   cd build2040
   cmake -DPICO_BOARD=vgaboard ..
   make
```
For Pico 2 builds, issue the commands:
```
   cd picomz-80k
   mkdir build2350
   cd build2350
   cmake -DPICO_BOARD=vgaboard -DPICO_PLATFORM=rp2350 ..
   make
```
There should now be four (Pico) or two (Pico 2) .uf2 files in your build directory for the emulator that can be installed on the appropriate hardware

## Project Background

[The Pico MZ](https://z80.timholyoake.uk/the-pico-mz-80k/)

### This README was last updated on 17th November 2025.
