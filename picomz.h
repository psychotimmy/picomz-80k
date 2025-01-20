// A Sharp MZ-80K emulator for the Raspberry Pi Pico
// Release 1.0 - Written August - October 2024
// Release 1.1 - Written November 2024
// Release 1.2 - Written January 2025
//
// The license and copyright notice below apply to all files that make up this
// emulator, including documentation, excepting the z80 core, fatfs, sdcard 
// and Raspberry pico libraries. These have compatible open source licenses. 
//
// The contents of the SP-1002 Monitor and Character ROM are Copyright (c)
// 1979 Sharp Corporation and may be found in the source file sharpcorp.c
//
// This emulator has no other connection with Sharp Corporation.
//

// MIT License

// Copyright (c) 2024,2025 Tim Holyoake

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#ifndef USBDIAGOUTPUT
  #include "tusb_config.h" // Needs to come before tusb.h as it
#endif                     // overrides settings in tusb_options.h
#include <tusb.h>        
#include "pico.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/util/datetime.h"
#include "pico/scanvideo.h"
#include "pico/sync.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#ifdef RC2014VGA
  #include "hardware/i2c.h"  // Required for RC2014 RP2040/Pico VGA cards
#endif
#include "fatfs/ffconf.h"
#include "fatfs/ff.h"
#include "sdcard/sdcard.h"
#include "sdcard/pio_spi.h"
#include "zazu80/z80.h"

/* Low-level debugging code macro for printf() */
/* See CMakeLists.txt for compile time setting */
/* of USBDIAGOUTOUT */

#ifdef USBDIAGOUTPUT
  #define SHOW printf
#else
  #define SHOW //
#endif

/* Sharp MZ-80K memory locations */

#define MROMSIZE        4096  //   4   Kbytes Monitor ROM
#define CROMSIZE        2048  //   2   Kbytes Character ROM
#define URAMSIZE        49152 //   0.5 Kbytes Monitor + 48 Kbytes User RAM
#define VRAMSIZE        1024  //   1   Kbyte  Video RAM
#define FRAMSIZE        1024  //   1   Kbyte  FD ROM (not used at present)

/***************************************************/
/* Sharp MZ-80K memory map summary                 */
/*                                                 */
/* 0x0000 - 0x0FFF  Monitor ROM SP-1002 (or other) */
/*      0 - 4095    4096 bytes                     */
/* 0x1000 - 0x11FF  Monitor stack and work area    */
/*   4096 - 4607    512 bytes                      */
/* 0x1200 - 0xCFFF  User program area (inc. langs) */
/*   4608 - 53247   48640 bytes                    */
/* 0xD000 - 0xDFFF  Video device control area      */
/*  53248 -  57343     First 1024 bytes is VRAM    */
/*                     1000 bytes used for display */
/*                     Remaining 24 bytes "spare"  */
/*                     0xD400-0xDFFF unused addrs  */
/* 0xE000 - 0xEFFF  8255/8253 device control area  */
/*  57344 - 61439      Only first few addrs used   */
/* 0xF000 - 0xFFFF  FD controller ROM (if present) */
/*  61440 - 65535      Only first 1 Kbytes used    */
/*                                                 */
/***************************************************/

/* MZ-80K keyboard */
#define KBDROWS 10     // There are 10 rows sensed on the keyboard

/* USB keyboard buffer */
#define USBKBDBUF 12   // Should be ample

/* Emulator status information display - uses the last 40 scanlines */
/* equivalent to 5 rows of 40 characters */
#define EMUSSIZE 200   // Up to 200 bytes of status info stored

/* Start points in mzemustatus array of emulator status area lines */
#define EMULINE0      0
#define EMULINE1     40
#define EMULINE2     80
#define EMULINE3    120
#define EMULINE4    160

/* Tape header and maximum body sizes in bytes */
#define TAPEHEADERSIZE    128 // 128 bytes
#define TAPEBODYMAXSIZE 48640 // 47.5Kbytes

/* Holds global variables relating to the 8253 PIT */
typedef struct pit8253 {  
  uint16_t counter0; /* Two byte counter for sound frequency */
  uint8_t msb0;      /* Used to keep track of which byte of counter 0 */
                     /* we're writing to or reading from (E004) */

  uint16_t c2start;  /* Records value set in counter 2 when initialised */
  uint16_t counter2; /* Two byte counter for time */
  uint8_t msb2;      /* Used to keep track of which byte of counter 2 */
                     /* we're writing to or reading from (E006) */
  bool out2;         /* Start / stop counter 2 output */ 

  uint8_t e008call;  /* Incremented whenever E008 is read */

} pit8253;

/* picomz.c */
extern z80 mzcpu;
extern uint8_t mzuserram[URAMSIZE];
extern uint8_t mzvram[VRAMSIZE];
extern uint8_t mzemustatus[EMUSSIZE];
/* GPIO pins for pwm sound generation (see 8253.c) */
extern uint8_t picotone1;
extern uint8_t picotone2;

/* sharpcorp.c */
extern const uint8_t mzmonitor[MROMSIZE];
extern const uint8_t cgromuk[CROMSIZE];
extern const uint8_t cgromjp[CROMSIZE];
extern bool ukrom;

/* keyboard.c */
extern uint8_t processkey[KBDROWS];
#ifdef USBDIAGOUTPUT
  extern void mzcdcmapkey(int32_t*, int8_t);
#else
  extern void mzrptkey(void);
  extern void mzhidmapkey(uint8_t, uint8_t);
#endif

/* cassette.c */
extern uint8_t crstate;
extern uint8_t cwstate; 
extern uint8_t header[TAPEHEADERSIZE];
extern uint8_t body[TAPEBODYMAXSIZE];
extern void reset_tape(void);
extern uint8_t cread(void);
extern void cwrite(uint8_t);
extern uint8_t tapeinit(void);
extern int16_t tapeloader(int16_t);
extern FRESULT mzsavedump(void);
extern FRESULT mzreaddump(void);
extern void mzspinny(uint8_t);

/* vgadisplay.c */
extern uint16_t whitepix;
extern uint16_t blackpix;
extern void vga_main(void);

/* 8255.c */
extern uint8_t portC;
extern uint8_t cmotor;
extern uint8_t csense;
extern uint8_t vgate;
extern uint8_t vblank;
#ifdef USBDIAGOUTPUT
  extern uint8_t scantimes;
#endif
extern uint8_t rd8255(uint16_t addr);
extern void wr8255(uint16_t addr, uint8_t data);

/* 8253.c */
extern pit8253 mzpit;
extern void p8253_init(void);
extern uint8_t rd8253(uint16_t addr);
extern void wr8253(uint16_t addr, uint8_t data);
extern uint8_t rdE008();
extern void wrE008(uint8_t data);

/* miscfuncs.c */
extern void mzpicoled(uint8_t);
extern void ascii2mzdisplay(uint8_t*, uint8_t*);
extern uint8_t mzsafefilechar(uint8_t);
extern uint8_t mzascii2mzdisplay(uint8_t);

/* pca9536.c - used by RC2014 RP2040/Pico VGA cards */
#ifdef RC2014VGA
  #define IO_MODE_IN 1
  #define IO_MODE_OUT 0

  #define IO_0 0
  #define IO_1 1
  #define IO_2 2
  #define IO_3 3

  #define SDA_PIN 18
  #define SCL_PIN 19

  #define PCA9536_ADDR 0x41
  #define REG_INPUT    0  // default
  #define REG_OUTPUT   1
  #define REG_POLARITY 2
  #define REG_CONFIG   3

  extern i2c_inst_t* i2c_bus;

  extern bool has_pca9536(i2c_inst_t *i2c);
  extern bool pca9536_setup_io(i2c_inst_t *i2c, uint8_t io, uint8_t io_mode);
  extern bool pca9536_output_io(i2c_inst_t *i2c, uint8_t io, bool value);
  extern bool pca9536_output_reset(i2c_inst_t *i2c, uint8_t mask);
  extern bool pca9536_input_io(i2c_inst_t *i2c, uint8_t io);
  extern void init_i2c_bus(); 
  extern void deinit_i2c_bus();
  extern int reg_write(i2c_inst_t *i2c,const uint addr,const uint8_t reg,
                       uint8_t *buf,const uint8_t nbytes);
  extern int reg_read(i2c_inst_t *i2c,const uint addr,const uint8_t reg,
                      uint8_t *buf,const uint8_t nbytes);
  extern int reg_read_timeout(i2c_inst_t *i2c,const uint addr,
                              const uint8_t reg,uint8_t *buf,
                              const uint8_t nbytes,const uint timeout_us);
#endif
