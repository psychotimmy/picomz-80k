/* Sharp MZ-80K emulator - main program */
/* Tim Holyoake, August-October 2024    */

#include "picomz.h"

uint8_t mzuserram[URAMSIZE];		// MZ-80K monitor and user RAM
uint8_t mzvram[VRAMSIZE];		// MZ-80K video RAM
uint8_t mzemustatus[EMUSSIZE];          // Emulator status area

z80 mzcpu;			        // Z80 CPU context 
volatile void* unusedv;
volatile z80*  unusedz;

/* Write a byte to RAM or an output device */
void mem_write(void* unusedv, uint16_t addr, uint8_t value)
{
  /* Can't write to monitor ROM or into FD ROM space */
  if ((addr < 0x1000) || (addr > 0xEFFF )) return;

  /* Monitor and user RAM */
  if (addr < 0xD000) {
    mzuserram[addr-0x1000] = value;
    return;
  }

  /* Video RAM */
  /* Now deals with writing outside the real range, rather than returning */
  /* an error as previously */
  if (addr < 0xE000) {
    mzvram[addr&0x03ff] = value;
    return;
  }

  /* Write to the Intel 8255 */
  if (addr<0xE004) {
    wr8255(addr,value);
    return;
  }

  /* Write to the Intel 8253 */
  if (addr<0xE008) {
    wr8253(addr,value);
    return;
  }

  /* Write to the speaker (and other peripherals not implemented) */
  if (addr<0xE009) {
    wrE008(value);
    return;
  }

  /* Unused addresses. Note that a real MZ-80K doesn't decode all the   */
  /* address lines properly, so writes to these addresses can affect    */
  /* others. Poor practice though - and I haven't found any MZ-80K code */
  /* in the 'wild' yet that relies on this side effect. */
  SHOW("** Writing 0x%02x to unused address 0x%04x **\n",value,addr);
  return;
}

/* Read a byte from memory or input device */
uint8_t mem_read(void* unusedv, uint16_t addr)
{
  /* Monitor ROM */
  if (addr < 0x1000) return(mzmonitor[addr]);

  /* Monitor and user RAM */
  if (addr < 0xD000) return(mzuserram[addr-0x1000]);

  /* Video RAM */
  /* Now reads unused addresses between D400 and E000 as per */
  /* the real hardware */
  if (addr < 0xE000) return(mzvram[addr&0x03ff]);

  /* Intel 8255 */
  if (addr < 0xE004) return(rd8255(addr));

  /* Intel 8253 */
  if (addr < 0xE007) return(rd8253(addr));

  /* Unused address */
  if (addr < 0xE008) {
    SHOW("Reading unused address 0x%04x\n",addr);
    return(0xC7);
  }

  /* Sound */
  if (addr < 0xE009) return(rdE008());

  /* Unused addresses */
  SHOW("Reading unused address 0x%04x\n",addr);
  return(0xC7);
}

/* SIO write to device */
void sio_write(z80* unusedz, uint8_t addr, uint8_t val)
{
  /* SIO not used by MZ-80K, so should never get here */
  SHOW("Error: In sio_write at 0x%04x with value 0x%02x\n",addr,val);
  return;
}

/* SIO read from device */
uint8_t sio_read(z80* unusedz, uint8_t addr)
{
  /* SIO not used by MZ-80K, so should never get here */
  SHOW("Error: In sio_read at 0x%04x\n",addr);
  return(0);
}

/* Turn the LED on the pico on or off */
void mzpicoled(uint8_t state)
{
  // state == 1 is on, 0 is off.
  gpio_put(PICO_DEFAULT_LED_PIN, state);
  return;
}

/* Sharp MZ-80K emulator main loop */
int main(void) 
{
  uint8_t toggle;          // Used to toggle the pico's led for error
                           // conditions found on startup
#ifdef USBDIAGOUTPUT
  int32_t usbc[USBKBDBUF]; // USB keyboard buffer
  int8_t  ncodes;          // No. codes to process in usb keyboard buffer
#endif

#ifdef USBDIAGOUTPUT
  // Set system clock to 175MHz if in diag mode - see also CMakeLists.txt
  set_sys_clock_pll(1050000000,6,1);
#endif
  stdio_init_all();

  gpio_init(PICO_DEFAULT_LED_PIN); // Init onboard pico LED (GPIO 25).
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  sleep_ms(500);

  SHOW("\n Hello! My friend\n");
  SHOW("Hello! My computer\n\n");

  // Initialise mzuserram
  memset(mzuserram,0x00,URAMSIZE);

  // Initialise mzemustatus area (bottom 40 scanlines)
  memset(mzemustatus,0x00,EMUSSIZE);

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

  // Start VGA output on the second core
  multicore_launch_core1(vga_main);
  SHOW("VGA output started on second core\n\n");

  // Main emulator loop
  for(;;) {

    z80_step(&mzcpu);		  // Execute next z80 opcode

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
      mzcdcmapkey(usbc,ncodes);   // Map usb code(s) to MZ-80K keyboard
      memset(usbc,-1,USBKBDBUF);  // Reset USB keyboard buffer to empty
    }
  #else
    tuh_task();                   // Check for new keyboard events
    mzrptkey();                   // Check for a repeating key event
  #endif
  }

  return(0); // No return from here
}
