/* MZ-700 emulator - main program  */
/* Tim Holyoake, October 2025      */

#include "picomz.h"

uint8_t mzmemory[MEMORYSIZE];   // MZ-700 memory - 64Kbytes
uint8_t mzemustatus[EMUSSIZE];  // Emulator status area
uint16_t colourpix[8];          // Pixel colour array
uint8_t picotone1;              // gpio pins for pwm sound
uint8_t picotone2;

z80 mzcpu;                      // Z80 CPU context 
volatile void* unusedv;
volatile z80*  unusedz;

uint8_t mzmodel;                // MZ model type - MZ700 = 3
bool ukrom = true;              // Default is UK CGROM

/* Write a byte to RAM or an output device */
void __not_in_flash_func 
     (mem_write) (void* unusedv, uint16_t addr, uint8_t value)
{
  /* Write to RAM below 0xE000  and above 0xE008 */
  if ((addr < 0xE000) || (addr > 0xE008)) {
    mzmemory[addr] = value;
    return;
  }

  /* Write to the Intel 8255  (0xE000 - 0xE003) */
  if (addr<0xE004) {
    wr8255(addr,value);
    return;
  }

  /* Write to the Intel 8253  (0xE004 - 0xE007) */
  if (addr<0xE008) {
    wr8253(addr,value);
    return;
  }

  /* Write to the speaker (and other peripherals not implemented) */
  if (addr == 0xE008) {
    wrE008(value);
    return;
  }

  return;
}

/* Read a byte from memory or input device */
uint8_t __not_in_flash_func (mem_read) (void* unusedv, uint16_t addr)
{

  if ((addr < 0xE000) || (addr > 0xE008) || (addr == 0xE007))
    return(mzmemory[addr]);

  /* Intel 8255 */
  if (addr < 0xE004) 
    return(rd8255(addr));

  /* Intel 8253 */
  if (addr < 0xE007) 
    return(rd8253(addr));

  /* Sound */
  if (addr == 0xE008) 
    return(rdE008());

}

/* SIO write to device */
void sio_write(z80* unusedz, uint8_t addr, uint8_t val)
{
  /* SIO not used by MZ-700, so should never get here */
  SHOW("Error: In sio_write at 0x%04x with value 0x%02x\n",addr,val);
  return;
}

/* SIO read from device */
uint8_t sio_read(z80* unusedz, uint8_t addr)
{
  /* SIO not used by MZ-700, so should never get here */
  SHOW("Error: In sio_read at 0x%04x\n",addr);
  return(0);
}

/* Sharp MZ-700 emulator main loop */
int main(void) 
{
  uint8_t toggle;          // Used to toggle the pico's led for error
                           // conditions found on startup

#if defined (PICO1)
  // Set system clock to 220MHz
  // See also CMakeLists.txt
  set_sys_clock_pll(1350000000,6,1);
#endif

#if defined (PICO2)
  // Set system clock to 125MHz
  // See also CMakeLists.txt
  set_sys_clock_pll(1500000000,6,2);
#endif

  stdio_init_all();

  busy_wait_ms(1250);              // Wait for inits to complete before
                                   // outputting diagnostics etc.

  gpio_init(PICO_DEFAULT_LED_PIN); // Init onboard pico LED (GPIO 25).
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  SHOW("\nHello! My friend\n");
  SHOW("Hello! My computer\n\n");

  mzmodel=MZ700;

  // Initialise MZ-700 memory (64K)
  memset(mzmemory,0x00,MEMORYSIZE);

  // Copy monitor ROM to 0x0000 onwards
  for (int cprom=0; cprom < MROMSIZE; cprom++)
    mzmemory[cprom]=mzmonitor700[cprom];

  // Initialise mzemustatus area (bottom 40 scanlines)
  memset(mzemustatus,0x00,EMUSSIZE);

#ifdef RC2014VGA
  // Check for I2C capability on RC2014 RP2040 VGA board
  init_i2c_bus();
  if (has_pca9536(i2c_bus)) {
    SHOW("PCA9536 detected\n");
    pca9536_output_reset(i2c_bus,0b0011); // preinitialize output at LOW
    pca9536_setup_io(i2c_bus,IO_0,IO_MODE_OUT); // USB_POWER
    pca9536_setup_io(i2c_bus,IO_1,IO_MODE_OUT); // ACTIVE BUZZER (not used)
    pca9536_setup_io(i2c_bus,IO_2,IO_MODE_IN);  // not used
    pca9536_setup_io(i2c_bus,IO_3,IO_MODE_IN);  // not used 

    pca9536_output_io(i2c_bus,IO_0,true); // Allow output to USB keyboard
    // Note that speaker will be attached to GPIOs 23/24 if a RP2040-based
    // RC2014 VGA terminal is used - define picotone globals here BEFORE
    // 8253 PIT is initialised.
    picotone1=23;
    picotone2=24;
  }
  else {
    SHOW("PCA9536 NOT detected\n");
    deinit_i2c_bus();
    // Note that speaker will be attached to GPIOs 18/19 if a Pico-based
    // RC2014 VGA terminal with an sd card backpack is used - define
    // picotone globals here BEFORE 8253 PIT is initialised.
    picotone1=18;
    picotone2=19;
  }
#else
   // Global definitions for Pimoroni VGA board
   picotone1=27;
   picotone2=28;
#endif

  // Initialise 8253 PIT
  p8253_init();
  SHOW("8253 PIT initialised\n");

  // Initialise the Z80 processor
  z80_init(&mzcpu);
  mzcpu.read_byte = mem_read;
  mzcpu.write_byte = mem_write;
  mzcpu.port_in = sio_read;
  mzcpu.port_out = sio_write;
  mzcpu.pc = 0x0000;
  SHOW("Z80 processor initialised\n");

  // Initialise USB keyboard
  memset(processkey,0xFF,KBDROWS);

  tusb_init();

  SHOW("USB keyboard connected\n");

  mzpicoled(0);

  // Mount the sd card to act as a tape source
  FRESULT tapestatus;
  tapestatus=tapeinit(); 
  if (tapestatus != FR_OK) {
    SHOW("Error: sd card failed to initialise\n");
    // We've been unable to mount the sd card, so signal this with
    // 1s long pulses on the pico led. Emulator will need restarting
    // as without the sd card it's not much use!
    toggle=1;
    mzpicoled(toggle);
    while (true) {
      sleep_ms(1000);
      toggle=!toggle;
      mzpicoled(toggle);
    }
  }
  SHOW("microSD card mounted ok\n");

  // Define pixel colours - 8 used by the MZ-700
  colourpix[0]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,0);        //black
  colourpix[1]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,255);      //blue
  colourpix[2]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,0,0);      //red
  colourpix[3]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,0,255);    //magenta
  colourpix[4]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,0);      //green
  colourpix[5]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,255);    //cyan
  colourpix[6]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,255,0);    //yellow
  colourpix[7]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,255,255);  //white

  // Start VGA output on the second core
  multicore_launch_core1(vga_main);
  SHOW("VGA output started on second core\n\n");

  // Main emulator loop

  for(;;) {

    z80_step(&mzcpu);		  // Execute next z80 opcode

    tuh_task();                   // Check for new keyboard events
    mzrptkey();                   // Check for a repeating key event
                                  // or a NUM LOCK event
  }

  return(0); // No return from here
}
