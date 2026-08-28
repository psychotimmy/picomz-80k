/* MZ-80K, MZ-80A & MZ-700 emulator - main program */
/* Tim Holyoake, August 2024 - August 2026         */

#include "picomz.h"

uint8_t mzuserram[URAMSIZE];    // RAM from 0x1000 - 0xCFFF
uint8_t mzvram[VRAMSIZE];       // VRAM from 0xD000 - 0xDFFF
uint8_t mzbank4[BANK4SIZE];     // Bank switched RAM 0x0000-0x0FFF (MZ-700 only)
uint8_t mzbank12[BANK12SIZE];   // Bank switched RAM 0xD000-0xFFFF (MZ-700 only)
uint8_t mzemustatus[EMUSSIZE];  // Emulator status area
uint16_t colourpix[8];          // Pixel colour array (MZ-700 only)
uint16_t whitepix;
uint16_t blackpix;
uint8_t picotone1;              // gpio pins for pwm sound
uint8_t picotone2;

z80 mzcpu;                      // Z80 CPU context 
volatile void* unusedv;
volatile z80*  unusedz;

uint8_t mzmodel;                // MZ model type - default is MZ-80K
                                // Button A at boot = MZ-80A
                                // Button B at boot = MZ-700
bool ukrom = true;              // Default is UK CGROM (no JP CGROM for MZ-80A)

                                // Keyboard matrix mapping
                                // All 0xFF means no key to process

uint8_t processkey[KBDROWS] = { 0xFF,0xFF,0xFF,0xFF,0xFF,
                                0xFF,0xFF,0xFF,0XFF,0xFF };

                                // Banked memory status - MZ-700 only
bool bank4k=false;              // 0x0000 - 0x0FFF is ROM at switch on
bool bank12k=false;             // 0xD000 - 0xFFFF is VRAM at switch on
bool bank12klck=false;          // 0xD000 - 0xFFFF not inhibited at switch on

/* Write a byte to RAM or an output device */
void mem_write(void* unusedv, uint16_t addr, uint8_t value)
{
  /* Can't write to monitor space on the MZ-80K */
  if ((addr < 0x1000) && (mzmodel == MZ80K)) {
    return;
  }
  else if ((addr < 0x1000) && (mzmodel == MZ80A)) {
    /* Possible to write to monitor space on MZ-80A */
    mzmonitor80a[addr] = value;
    return;
  }
  else if ((addr < 0x1000) && (mzmodel == MZ700) && (bank4k)) {
    /* Write to 0x0000 - 0x0FFF only if RAM has been switched in */
    mzbank4[addr] = value;
    return;
  }

  /* Monitor workspace and user RAM 0x1000 - 0xCFFF */
  if (addr < 0xD000) {
    mzuserram[addr-0x1000] = value;
    return;
  }

  /* MZ-700: don't write to any address above 0xD000 if it has been inhibited */
  if ((bank12klck) && (mzmodel == MZ700))
    return;

  /* MZ-700: Write to 12K banked RAM if bank12k true */
  if ((bank12k) && (mzmodel == MZ700)) {
    mzbank12[addr-0xD000] = value;
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
    return;
  }
  else if ((addr < 0xE000) && (mzmodel == MZ700)) {
    /* MZ-700 is the simplest of the three! */
    mzvram[addr-0xD000] = value;
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
  /* Ignore as this is currently unimplemented in the emulator */
  if ((addr == 0xE800) && (mzmodel == MZ80A)) {
    return;
  }

  /* Unused addresses. Note that a real MZ-80K doesn't decode all the   */
  /* address lines properly, so writes to these addresses can affect    */
  /* others. Poor practice though - and I haven't found any MZ-80K code */
  /* in the 'wild' yet that relies on this side effect. Not an issue on */
  /* the MZ-700 - addresses above 0xE008 are handled differently.       */
  /* Currently this trap includes FD and QD ROM space - unimplemented.  */

  return;
}

/* Read a byte from memory or input device */
uint8_t mem_read(void* unusedv, uint16_t addr)
{
  /* Monitor address space (ROM on 80K, RAM on 80A, either on MZ-700) */
  if ((addr < 0x1000) && (mzmodel == MZ80K)) 
    return(mzmonitor80k[addr]);
  else if ((addr < 0x1000) && (mzmodel == MZ80A))
    return(mzmonitor80a[addr]);
  else if ((addr < 0x1000) && (mzmodel == MZ700)) {
    if (bank4k)
      return(mzbank4[addr]);
    else
      return(mzmonitor700[addr]);
  }

  /* Monitor and user RAM 0x1000 - 0xCFFF */
  if (addr < 0xD000) 
    return(mzuserram[addr-0x1000]);

  /* MZ-700: don't read any address above 0xD000 if it has been inhibited */
  if ((bank12klck) && (mzmodel==MZ700))
    return(0xC7);

  /* MZ-700: read from 12K banked RAM if bank12k true */
  if ((bank12k) && (mzmodel==MZ700))
    return(mzbank12[addr-0xD000]);

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
  else if ((addr < 0xE000) && (mzmodel == MZ700))
    /* MZ-700 - bank12k is false if we get here */
    return(mzvram[addr-0xD000]);

  /* Intel 8255 */
  if (addr < 0xE004) 
    return(rd8255(addr));

  /* Intel 8253 */
  if (addr < 0xE008) 
    return(rd8253(addr));

  /* Sound */
  if (addr == 0xE008) 
    return(rdE008());

  /* MZ-80A specific addresses follow */
  if (mzmodel == MZ80A) {

    /* Memory swap - Monitor code goes to 0xC000 */
    if (addr == 0xE00C) {
      for (uint8_t i=0; i<MROMSIZE; i++)
        mzuserram[(addr-0x1000)+i]=mzmonitor80a[i];
      return(0xFF);
    }

    /* Memory swap - 0xC000+4K goes to 0x0000 */
    if (addr == 0xE010) {
      for (uint8_t i=0; i<MROMSIZE; i++)
        mzmonitor80a[i]=mzuserram[(addr-0x1000)+i];
      return(0xFF);
    }

    /* Normal video */
    if (addr == 0xE014) {
      whitepix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,0);
      blackpix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,0);
      /* Return value doesn't seem to be used, but 0x00 stored at 0x1190 */
      /* when normal video is selected */
      return(0x00);
    }

    /* Reverse video */
    if (addr == 0xE015) {
      whitepix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,0);
      blackpix=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,0);
      /* Return value doesn't seem to be used, but 0xFF stored at 0x1190 */ 
      /* when reverse video is selected */
      return(0xFF);
    }

    /* Scroll screen up / down */
    if ((addr >= 0xE200) && (addr <=0xE2FF)) {
      return(addr&0xFF);
    }

  }

  /* MZ-80A user socket ROM - return 0xC7 if not present */
  if ((addr == 0xE800) && (mzmodel == MZ80A)) {
    return(0xC7);
  }

  /* All other unused addresses */
  return(0xC7);
}

/* SIO write to device */
void sio_write(z80* unusedz, uint8_t addr, uint8_t val)
{
  /* Used by MZ-700 to control memory bank switching */
  if (mzmodel == MZ700) {

    // Swap out ROM for RAM
    if (addr == 0xE0)
      bank4k=true;

    // Swap out VRAM etc. for banked RAM
    if (addr == 0xE1)
      bank12k=true;

    // Swap out RAM for ROM
    if (addr == 0xE2)
      bank4k=false;

    // Swap out banked RAM for VRAM etc.
    if (addr == 0xE3)
      bank12k=false;

    // The equivalent of a power off / power on - locked bank unlocked as well
    if (addr == 0xE4) {
      bank4k=false;
      bank12k=false;
      bank12klck=false;
    }

    // Lock the 12K banked RAM or VRAM etc. Writes inhibited, reads undefined
    if (addr == 0xE5)
      bank12klck=true;

    // Unlock the 12K banked RAM or VRAM etc. Writes enabled, reads defined by
    // whether the banked RAM is active or the VRAM etc. is active.
    if (addr == 0xE6)
      bank12klck=false;
  }

  /* SIO not used by MZ-80K/A */
  return;
}

/* SIO read from device */
uint8_t sio_read(z80* unusedz, uint8_t addr)
{
  /* SIO not read by MZ-80K/A/700, so should never get here */
  return(0);
}

/* Sharp MZ-80K/A emulator main loop */
int main(void) 
{
  uint8_t toggle=0;          // Used to toggle the pico's led for error
                             // conditions found on startup
  bool clocksetok=true;      // Assume clock set ok - as over/underclocking is
                             // not always used

  stdio_init_all();

  busy_wait_ms(250);               // Wait for inits to complete

  gpio_init(PICO_DEFAULT_LED_PIN); // Init onboard pico LED (GPIO 25)
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  // If button A on the Pico's carrier board is pressed, run the emulator 
  // as a MZ-80A. Button B = MZ-700, no button = MZ-80K.

  gpio_init(0);  // Button A
  gpio_init(6);  // Button B
  gpio_set_dir(0,GPIO_IN);
  gpio_set_dir(6,GPIO_IN);
  gpio_pull_down(0);
  gpio_pull_down(6);
  if (gpio_get(6))
    mzmodel=MZ700;
  else if (gpio_get(0))
    mzmodel=MZ80A;
  else
    mzmodel=MZ80K;

  // Over/underclocking depends on pico type, board type and MZ model
  #ifdef PICO1
  if (mzmodel==MZ700) {
    // 250MHz clock
    set_sys_clock_pll(1500000000,6,1);
    set_sys_clock_hz(250000000,clocksetok);
  }
  else {
    // 150 MHz clock
    set_sys_clock_pll(1500000000,5,2);
    set_sys_clock_hz(150000000,clocksetok);
  }
  #endif
  #ifdef PICO2
  if ((mzmodel==MZ80K)||(mzmodel==MZ80A)) {
    // 125MHz clock
    set_sys_clock_pll(1500000000,6,2);
    set_sys_clock_hz(125000000,clocksetok);
  }
  #endif

  // Check the clock has been set correctly - signal error if not
  while(!clocksetok) {
    busy_wait_ms(100);
    toggle=!toggle;
    mzpicoled(toggle);
  }

  // Initialise mzuserram and mzvram
  memset(mzuserram,0x00,URAMSIZE);
  memset(mzvram,0x00,VRAMSIZE);

  // Initialise MZ-700 banked RAM (4K and 12K) - inactive at switch on */
  // Not used for the MZ-80K and A emulations
  memset(mzbank4,0x00,BANK4SIZE);
  memset(mzbank12,0x00,BANK12SIZE);

  // Initialise mzemustatus area (bottom 40 scanlines)
  memset(mzemustatus,0x00,EMUSSIZE);

  // Define default pixel colours (MZ-80K and A are monochrome)
  colourpix[0]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,0);        //black
  colourpix[1]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,0,255);      //blue
  colourpix[2]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,0,0);      //red
  colourpix[3]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,0,255);    //magenta
  colourpix[4]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,0);      //green
  colourpix[5]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(0,255,255);    //cyan
  colourpix[6]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,255,0);    //yellow
  colourpix[7]=PICO_SCANVIDEO_PIXEL_FROM_RGB8(255,255,255);  //white

  blackpix=colourpix[0];
  if ((mzmodel == MZ80K) || (mzmodel == MZ700))
    whitepix=colourpix[7];
  else
    /* MZ-80A has green characters */
    whitepix=colourpix[4];

  // Start VGA output on the second core
  multicore_launch_core1(vga_main);

#ifdef RC2014VGA
  // Check for I2C capability on RC2014 RP2040 VGA board
  init_i2c_bus();
  if (has_pca9536(i2c_bus)) {
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

  // Initialise the Z80 processor
  z80_init(&mzcpu);
  mzcpu.read_byte = mem_read;
  mzcpu.write_byte = mem_write;
  mzcpu.port_in = sio_read;
  mzcpu.port_out = sio_write;
  mzcpu.pc = 0x0000;

  // Initialise USB keyboard
  tusb_init();

  mzpicoled(0);

  // Mount the sd card to act as a tape source
  FRESULT tapestatus;
  tapestatus=tapeinit(); 
  if (tapestatus != FR_OK) {
    // We've been unable to mount the sd card, so signal this with
    // 1s long pulses on the pico led. Emulator will need restarting
    // as without the sd card it's not much use!
    toggle=1;
    mzpicoled(toggle);
    while (true) {
      busy_wait_ms(1000);
      toggle=!toggle;
      mzpicoled(toggle);
    }
  }

  // Main emulator loop
  uint8_t delay=0;
  uint64_t nowtime,exectime;
  int64_t adjust=0;
  for(;;) {

    exectime=get_absolute_time();
    z80_step(&mzcpu);		  // Execute next z80 opcode

    // Timing adjustments 
    // Depends on Pico model, MZ emulator required and the carrier board
    #ifdef PICO1
    if (mzmodel==MZ700) {
      nowtime=get_absolute_time();
      // <<2 equivalent to x4
      adjust += mzcpu.cyc-((nowtime-exectime)<<2);
      mzcpu.cyc=0;
      if (adjust > 1856) {
        busy_wait_us(64);
        adjust=0;
      }
    } 
    else if ((mzmodel==MZ80K)||(mzmodel==MZ80A)) {
      nowtime=get_absolute_time();
      // <<1 equivalent to x2
      adjust += mzcpu.cyc-((nowtime-exectime)<<1);
      mzcpu.cyc=0;
      if (adjust > 1024) {
        busy_wait_us(64);
        adjust=0;
      }
    } 
    #endif
    #ifdef PICO2
    if ((mzmodel==MZ700) && (++delay == 26)) {
      busy_wait_us(1);           
      delay=0;
    } 
    else if ((mzmodel==MZ80K) && (++delay == 2)) {
      busy_wait_us(3);           
      delay=0;
    }
    else if ((mzmodel==MZ80A) && (++delay == 3)) {
      busy_wait_us(4);           
      delay=0;
    }
    #endif

    tuh_task();                   // Check for new keyboard events
    mzrptkey();                   // Check for a repeating key event
                                  // or a NUM LOCK event
  }

  return(0); // No return from here
}
