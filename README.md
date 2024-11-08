# Pico MZ-80K 
## A Sharp MZ-80K Emulator for the Raspberry Pico Microcontroller

A Raspberry Pico implementation of the Sharp MZ-80K sold in the UK from 1979, utilising the Pimoroni VGA demo base.

![A picture of a Pimoroni VGA demo base with a Raspberry Pico (with headers) installed.](https://z80.timholyoake.uk/wp-content/uploads/2024/09/20240905_101721-1024x633.jpg)

## Documentation

Full user and systems documentation is provided in the [documentation subdirectory](https://github.com/psychotimmy/picomz-80k/tree/main/documentation).

## Brief user notes

Ensure a microSD card containing one or more Sharp MZ-80K software (.mzf) files is installed in the slot on the Pimoroni VGA demo base. 

If you have successfully flashed either **picomz-80k.uf2** or **picomz-80k-diag.uf2** to the pico, when a USB keyboard (use picomz-80k.uf2) or terminal emulator (use picomz-80k-diag.uf2) plus VGA display is connected, you will see:

** MONITOR SP-1002 **

\*

on the screen.  

If the Pico's green led is flashing quickly (200ms between flashes), this means that a USB keyboard has not been connected or recognised via a terminal emulator. 

If the Pico's green led is flashing slowly (1s between flashes), then your microSD card cannot be read.

If either of these error conditions occur, the emulator will not display the monitor prompt until the problem is resolved. 

To find a file to load from the microSD card, use the F1 key to browse its contents forwards, F2 to go backwards. 

## Brief developer notes

While the Raspberry Pico SDK 2.0.0 will successfully build Pico MZ-80K, identical code runs approximately 8% more slowly than under SDK 1.5.1. Pico MZ-80K also exhibits some instability when built with SDK 2.0.0 that is not present when built with SDK 1.5.1. 

As such, it is ***strongly recommended*** that you use SDK 1.5.1 if you wish to rebuild Pico MZ-80K at this time.

## Instructions for rebuilding Pico MZ-80K (see also the documentation subdirectory)

### Pre-requisites for Raspberry Pi OS (Debian Bookworm)

CMake (version 3.13 or later) and a gcc cross compiler.
```
   sudo apt install cmake
   sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
```   
The Pico MZ-80K emulator has been tested using the Raspberry Pico SDK version 1.5.1. This and
the pico-extras repository must be available on your computer if you wish to compile the emulator.

Assuming that you are already in the subdirectory you wish to install the Pico SDK under, issue the
commands:
```
   git clone https://github.com/raspberrypi/pico-sdk.git -b 1.5.1
   git clone https://github.com/raspberrypi/pico-extras.git -b sdk-1.5.1
```   
Then clone the current version of the Pico MZ-80K repository.
```
   git clone https://github.com/psychotimmy/picomz-80k.git
```   
Ensure that the Pico SDK and Pico Extras subdirectories have been exported. 

For example, if these libraries have been installed under /home/pi, use:
```
   export PICO_SDK_PATH=/home/pi/pico-sdk
   export PICO_EXTRAS_PATH=/home/pi/pico-extras
```   
Then issue the commands:
```
   cd pico-sdk
   git submodule update --init --recursive
   cd ../picomz-80k
   mkdir build
   cd build
   cmake -DPICO_BOARD=vgaboard ..
   make
```
There should now be two .uf2 files in the build directory for the emulator that can be installed on
your Pico.

**picomz-80k.uf2** is for normal use. It assumes a UK USB keyboard connected directly to the Pico.

**picomz-80k-diag.uf2** can only be used through a terminal emulator (such as minicom) as diagnostic messages are output to the Pico's USB port.

## Project Background

[RetroChallenge 2024/10 project log](https://z80.timholyoake.uk/retrochallenge-2024-10/)

### This README was last updated on 30th October 2024.
