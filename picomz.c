/*  MZ-80K & MZ-80A emulator - main program  */
/* Tim Holyoake, August 2024 - February 2025 */

#include "picomz.h"

uint8_t mzuserram[URAMSIZE];    // MZ-80 monitor workspace and user RAM
uint8_t mzvram[VRAMSIZE];       // MZ-80 video RAM (only 1K used on 80-K)
uint8_t mzemustatus[EMUSSIZE];  // Emulator status area
uint16_t whitepix;              // Pixel colours
uint16_t blackpix;
uint8_t picotone1;              // gpio pins for pwm sound
uint8_t picotone2;

z80 mzcpu;                      // Z80 CPU context 
volatile void* unusedv;
volatile z80*  unusedz;

uint8_t mzmodel;                // MZ model type - default is MZ-80K
bool ukrom = true;              // Default is UK CGROM

/* Write a byte to RAM or an output device */
void __not_in_flash_func (mem_write) (void* unusedv, uint16_t addr, uint8_t value)
{
  /* Can't write to monitor space on the MZ-80K */
  if ((addr < 0x1000) && (mzmodel == MZ80K)) 
    return;
  else if (addr < 0x1000) {
    /* Possible on MZ-80A */
    mzmonitor80a[addr] = value;
    return;
  }

  /* Monitor workspace and user RAM */
  if (addr < 0xD000) {
    mzuserram[addr-0x1000] = value;
    return;
  }

  /* Video RAM */
  /* Now deals with writing outside the real range, rather than returning */
  /* an error as previously on the MZ-80K. 0x03FF masks to 1k VRAM */
  if ((addr < 0xE000) && (mzmodel == MZ80K)) {
    mzvram[addr&0x03FF] = value;
    return;
  }
  else if ((addr < 0xD800) && (mzmodel == MZ80A)) {
    /* 0x07FF masks to 2k VRAM. The monitor writes 0xCF (207) to higher   */
    /* 0xDxxx addresses on startup, which corrupts the display if the     */
    /* MZ-80K method of dealing with these addresses is used. These are   */
    /* treated as unused addresses instead. Maybe Sharp's plan was to     */
    /* have an 'A' model with colour video at some point? The MZ-700 did. */
    mzvram[addr&0x7FF] = value;
    return;
  }
  else if ((addr < 0xE000) && (mzmodel == MZ80A)) {
    //SHOW("** Writing 0x%02x to unused address 0x%04x **\n",value,addr);
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

  /* Write to the user socket ROM is attempted on startup on the MZ-80A */
  if ((addr == 0xE800) && (mzmodel == MZ80A)) {
    SHOW("** Writing 0x%02x to user ROM socket at 0x%04x **\n",value,addr);
    return;
  }

  /* Unused addresses. Note that a real MZ-80K doesn't decode all the   */
  /* address lines properly, so writes to these addresses can affect    */
  /* others. Poor practice though - and I haven't found any MZ-80K code */
  /* in the 'wild' yet that relies on this side effect.                 */
  /* Currently this trap includes the FD ROM space for MZ-80K & MZ-80A  */

  SHOW("** Writing 0x%02x to unused address 0x%04x **\n",value,addr);
  return;
}

/* Read a byte from memory or input device */
uint8_t __not_in_flash_func (mem_read) (void* unusedv, uint16_t addr)
{
  /* Monitor address space (ROM on 80K, RAM on 80A) */
  if ((addr < 0x1000) && (mzmodel == MZ80K)) 
    return(mzmonitor80k[addr]);
  else if ((addr < 0x1000) && (mzmodel == MZ80A))
    return(mzmonitor80a[addr]);

  /* Monitor and user RAM */
  if (addr < 0xD000) 
    return(mzuserram[addr-0x1000]);

  /* Video RAM */
  if ((addr < 0xE000) && (mzmodel == MZ80K)) 
    /* Reads and maps unused addresses between 0xD400 and 0xE000 as per */
    /* the real MZ-80K hardware */
    return(mzvram[addr&0x03FF]);
  else if ((addr < 0xD800) && (mzmodel == MZ80A))
    /* MZ-80A fully decodes the 0xDxxx space even though only the first */
    /* 2Kbytes is used ... perhaps a colour upgrade was once intended?  */
    return(mzvram[addr&0x07FF]);
  else if ((addr < 0xE000) && (mzmodel == MZ80A))
    return(0xC7);

  /* Intel 8255 */
  if (addr < 0xE004) 
    return(rd8255(addr));

  /* Intel 8253 */
  if (addr < 0xE007) 
    return(rd8253(addr));

  /* Unused address */
  if (addr < 0xE008) {
    SHOW("Reading unused address 0x%04x\n",addr);
    return(0xC7);
  }

  /* Sound */
  if (addr == 0xE008) 
    return(rdE008());

  /* MZ-80A specific addresses follow */
  if (mzmodel == MZ80A) {

    /* Memory swap - Monitor code goes to 0xC000 */
    if (addr == 0xE00C) {
      SHOW("MZ-80A monitor swapped out to 0xC000\n");
      for (uint8_t i=0; i<MROMSIZE; i++)
        mzuserram[(addr-0x1000)+i]=mzmonitor80a[i];
      return(0xFF);
    }

    /* Memory swap - 0xC000+4K goes to 0x0000 */
    if (addr == 0xE010) {
      SHOW("MZ-80A 0xC000 swapped into to monitor space\n");
      for (uint8_t i=0; i<MROMSIZE; i++)
        mzmonitor80a[i]=mzuserram[(addr-0x1000)+i];
      return(0xFF);
    }

    /* Normal video */
    if (addr == 0xE014) {
      SHOW("MZ-80A normal video\n");
      whitepix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,0);
      blackpix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,0);
      /* Return value doesn't seem to be used, but 0x00 stored at 0x1190 */
      /* when normal video is selected */
      return(0x00);
    }

    /* Reverse video */
    if (addr == 0xE015) {
      SHOW("MZ-80A reverse video\n");
      whitepix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,0);
      blackpix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,0);
      /* Return value doesn't seem to be used, but 0xFF stored at 0x1190 */ 
      /* when reverse video is selected */
      return(0xFF);
    }

    /* Scroll screen up / down */
    if ((addr >= 0xE200) && (addr <=0xE2FF)) {
      SHOW("MZ-80A display command read at 0x%04x \n",addr);
      return(addr&0xFF);
    }

  }

  /* MZ-80A user socket ROM - return 0xC7 if not present */
  if ((addr == 0xE800) && (mzmodel == MZ80A)) {
    SHOW("Reading user socket ROM address 0x%04x\n",addr);
    return(0xC7);
  }

  /* All other unused addresses */
  SHOW("Reading unused address 0x%04x\n",addr);
  return(0xC7);
}

/* SIO write to device */
void sio_write(z80* unusedz, uint8_t addr, uint8_t val)
{
  /* SIO not used by MZ-80K/A, so should never get here */
  SHOW("Error: In sio_write at 0x%04x with value 0x%02x\n",addr,val);
  return;
}

/* SIO read from device */
uint8_t sio_read(z80* unusedz, uint8_t addr)
{
  /* SIO not used by MZ-80K/A, so should never get here */
  SHOW("Error: In sio_read at 0x%04x\n",addr);
  return(0);
}

/* Sharp MZ-80K/A emulator main loop */
int main(void) 
{
  uint8_t toggle;          // Used to toggle the pico's led for error
                           // conditions found on startup
#ifdef USBDIAGOUTPUT
  int32_t usbc[USBKBDBUF]; // USB keyboard buffer
  int8_t  ncodes;          // No. codes to process in usb keyboard buffer
#endif

#if defined (USBDIAGOUTPUT) && defined (PICO1)
  // Set system clock to 175MHz if in diag mode on a rp2040
  // See also CMakeLists.txt
  set_sys_clock_pll(1050000000,6,1);
#endif

#if defined (USBDIAGOUTPUT) && defined (PICO2)
  // Set system clock to 125MHz if in diag mode on a rp2350
  // See also CMakeLists.txt
  set_sys_clock_pll(1500000000,6,2);
#endif

#if ! defined (USBDIAGOUTPUT) && defined (PICO2)
  // Set system clock to 100MHz if in normal mode on a rp2350
  // See also CMakeLists.txt
  set_sys_clock_pll(1500000000,5,3);
#endif

  stdio_init_all();

  busy_wait_ms(1250);              // Wait for inits to complete before
                                   // outputting diagnostics etc.

  gpio_init(PICO_DEFAULT_LED_PIN); // Init onboard pico LED (GPIO 25).
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  SHOW("\nHello! My friend\n");
  SHOW("Hello! My computer\n\n");

  // If button A on the Pico's carrier board is pressed, run the emulator 
  // as a MZ-80A rather than as a MZ-80K.

  gpio_init(0);
  gpio_set_dir(0,GPIO_IN);
  gpio_pull_down(0);
  if (gpio_get(0)) {
    mzmodel=MZ80A;
    SHOW("MZ-80A emulation selected\n");
  }
  else {
    mzmodel=MZ80K;
    SHOW("MZ-80K emulation selected\n");
  }

  // Initialise mzuserram
  memset(mzuserram,0x00,URAMSIZE);

  // Initialise mzvram 
  memset(mzvram,0x00,VRAMSIZE);

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

#ifdef USBDIAGOUTPUT
  toggle=1;
  mzpicoled(toggle);
  while (!tud_cdc_connected()) { 
    // Pico onboard led flashes if no keyboard via terminal emulator found
    sleep_ms(200); 
    toggle=!toggle;  
    mzpicoled(toggle);
  }
#else
  tusb_init();
#endif
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

  // Define default pixel colours
  blackpix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,0);
  if (mzmodel == MZ80K)
    whitepix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,255,255);
  else
    whitepix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,0);

  // Start VGA output on the second core
  multicore_launch_core1(vga_main);
  SHOW("VGA output started on second core\n\n");

  // Main emulator loop
  for(;;) {

    z80_step(&mzcpu);		  // Execute next z80 opcode
  #if ! defined (USBDIAGOUTPUT) && defined (PICO2)
    busy_wait_us(1);              // Need to slow down a Pico 2 a little more
  #endif

  #ifdef USBDIAGOUTPUT
    usbc[0]=tud_cdc_read_char();  // Read keyboard, no wait
    if (usbc[0] != -1) {
      SHOW("Key pressed %x\n",usbc[0]);
      sleep_ms(2);                // Delay to allow keyboard to catch up
      ncodes=1;
        while((usbc[ncodes]=tud_cdc_read_char()) != -1) { 
          SHOW("Key pressed %x\n",usbc[ncodes]);
          sleep_ms(2);
          ++ncodes;
        }
      mzcdcmapkey(usbc,ncodes);   // Map usb code(s) to MZ keyboard
      memset(usbc,-1,USBKBDBUF);  // Reset USB keyboard buffer to empty
    }
  #else
    tuh_task();                   // Check for new keyboard events
    mzrptkey();                   // Check for a repeating key event
                                  // or a NUM LOCK event
  #endif
  }

  return(0); // No return from here
}
