/* A vastly simplified 8253 Programmable Interval Timer implementation */
/* Sharp MZ-80K emulator, Tim Holyoake, September - October 2024. */

#include "picomz.h"

// MZ-80K Implementation notes

// Address E007 - PIT control word. (Write only - can't read this address.)
// This implementation ignores it entirely. The MZ-80K usage of the 8253 is
// very limited and never changes, so we can safely ignore control words.

// Counter 2 (address E006) MZ-80K clock, 1 second resolution.
// Counts down from 43,200 seconds unless reset. At zero, triggers /INT
// to Z80 to toggle AM/PM flag in the monitor workarea (Mode 0).
// This is NOT initialised by the SP-1002 monitor at startup, but is used
// (for example) by BASIC SP-5025 to implement TI$

// Counter 1 (address E005) in the real hardware is used as rate generator
// (Mode 2) to drive the clock for counter 2 with 1 second pulses. Not
// used in this emulator as we can simply drive counter 2 from the pico's
// real time clock functions.

// Counter 0 (address E004) - a square wave generator (Mode 3) 
// used to output sound at the correct frequency to the MZ-80K's
// loudspeaker. The monitor turns sound off by default on startup 
// by writing 0x00 to E008. If 0x01 is written to E008, sound is turned on.
// Sound generation in this implementation relies on the pwm and alarm
// functions delivered by the pico SDK.

/* Pico pwm gpio pin definitions */
#define PICOTONE1 27      /* The VGA board uses pins 27 & 28 for sound (pwm) */
#define PICOTONE2 28

static uint16_t counter0; /* Two byte counter for sound frequency */
static uint16_t counter2; /* Two byte counter for time */
static bool out2;         /* Start / stop counter 2 output */

static uint16_t c2start;  /* Records value set in counter 2 when initialised */

static uint8_t msb2;      /* Used to keep track of which byte of counter 2 */
                          /* we're writing to or reading from (E006) */

static uint8_t msb0;      /* Used to keep track of which byte of counter 0 */
                          /* we're writing to or reading from (E004) */

static uint8_t e008call;  /* Incremented whenever E008 is read */

typedef struct toneg {    /* Tone generator structure for pwm sound  */
  uint8_t slice1;         /* Initialised by pico_tone_init() function */
  uint8_t slice2;
  uint8_t channel1;
  uint8_t channel2;
  uint32_t picoclock;     /* Pico clock speed */
  float freq;             /* Requested frequency in Hz */
} toneg;

static toneg picotone;         /* Tone generator global static */

static alarm_id_t tone_alarm;  /* We use alarms to start/stop tones */

/*************************************************************/
/*                                                           */
/* Internal 8253 functions to support the Sharp MZ-80K clock */
/*                                                           */
/*************************************************************/
void mzpico_rtc_init(void)
{
  // Start on Monday 1st January 2024 00:00:00
  // This is arbitrary, as the MZ-80K clock only
  // counts in seconds for half a day. It has no
  // concept of years, months etc.
  datetime_t t = {
    .year  = 2024,
    .month = 01,
    .day   = 01,
    .dotw  = 1,    // Monday
    .hour  = 00,
    .min   = 00,
    .sec   = 00
  };
 
  // Start the pico RTC
  rtc_init();
  rtc_set_datetime(&t);

  return;
}

/* Return the number of seconds minus 1 since the Pico RTC was initialised */
uint16_t mzpicosecs(void)
{
  uint16_t elapsed;
  datetime_t t;

  rtc_get_datetime(&t);
  elapsed=t.hour*3600+t.min*60+t.sec-1;
  return(elapsed);
}

/*************************************************************/
/*                                                           */
/* Internal 8253 functions to support sound generation       */
/*                                                           */
/*************************************************************/
void pico_tone_init()
{
  // Pico VGA board uses pins 27 and 28 for the pwm stereo output 
  // defined as PICOTONE1 and PICOTONE2 in sharpmz.h
  // The original MZ-80K was mono, of course!

  /* Set the gpio pins for pwm sound */
  gpio_set_function(PICOTONE1, GPIO_FUNC_PWM);
  gpio_set_function(PICOTONE2, GPIO_FUNC_PWM);

  /* Initialise the picotone static structure - stores slices and channels */
  picotone.slice1=pwm_gpio_to_slice_num(PICOTONE1);
  picotone.slice2=pwm_gpio_to_slice_num(PICOTONE2);
  picotone.channel1=pwm_gpio_to_channel(PICOTONE1);
  picotone.channel2=pwm_gpio_to_channel(PICOTONE2);

  /* Set the channel levels */
  pwm_set_chan_level(picotone.slice1, picotone.channel1, 2048);
  pwm_set_chan_level(picotone.slice2, picotone.channel2, 2048);

  /* Determine the pico clock speed */
  picotone.picoclock=clock_get_hz(clk_sys);

  /* Set frequency to 0.1Hz (zero-ish) */
  picotone.freq=0.1;

  return;
}

static int64_t mzpico_tone_off(alarm_id_t id, void *userdata)
{
  // Turn the sound generator off
  pwm_set_enabled(picotone.slice1, false);
  pwm_set_enabled(picotone.slice2, false);
  return(0); 
}

void mzpico_tone_on(void)
{
  uint32_t *unused; /* Dummy variable for alarm callback */

  // Avoid possible divide by 0 by insisting frequency > 0.1Hz
  if (picotone.freq > 0.1) {
    float divider=(float)picotone.picoclock/(picotone.freq*10000.0);
    pwm_set_clkdiv(picotone.slice1, divider);
    pwm_set_clkdiv(picotone.slice2, divider);
    pwm_set_wrap(picotone.slice1, 10000);
    pwm_set_wrap(picotone.slice2, 10000);
    pwm_set_gpio_level(PICOTONE1, 5000);
    pwm_set_gpio_level(PICOTONE2, 5000);
    pwm_set_enabled(picotone.slice1, true);
    pwm_set_enabled(picotone.slice2, true);

    if (tone_alarm) cancel_alarm(tone_alarm);
    // The delay of 20s below is arbitrary as the longest possible
    // duration of a note on the MZ-80K is 7 seconds (7000ms).
    // The alarm will always be cancelled before this value is reached.
    tone_alarm=add_alarm_in_ms(20000,mzpico_tone_off,unused,true);
  }
}

/* Initialise the 8253 Programmable Interval Timer (PIT) */
void p8253_init(void) 
{
  // Sound generation
  counter0 = 0x0000;
  msb0 = 0;
  pico_tone_init();
  e008call = 0x00;   // Used as a return value when E008 is read

  // MZ-80K time
  counter2 = 0x0000;
  msb2 = 0;
  c2start = 0x0000;
}

// Note - latching is currently ignored - unlikely to be crucial to the MZ-80K emulator's operation
uint8_t rd8253(uint16_t addr) 
{
  if (addr == 0xE006) {

    /* E006 - read the countdown value from counter 2 */

    if ((counter2 == 1) && (out2)) {  // Counter2 has reached 1 (0 seconds),
      out2=false;                     // so trigger an interupt if
      z80_gen_int(&mzcpu,0x01);       // this has not already happened
    }

    if (counter2 <= 1) {                   // Special handling if the counter
      msb2=!msb2;                          // is zero (1 or less)
      return(0x00);
    }

    if (!msb2) {
      counter2=c2start-mzpicosecs();  
      msb2=1;
      return(counter2&0xFF);
    }
    else {
      msb2=0;
      return((counter2>>8)&0xFF);
    }
  }

  /* Anything other than E006 is unexpected */
  SHOW("rd8253 passed unexpected address 0x%04x\n",addr);
  return(0x00);
}

void wr8253(uint16_t addr, uint8_t val) 
{

  // E004 is used for generating tones
  if (addr == 0xE004) {
    // The 8253 on the MZ-80K is fed with a 1MHz pulse
    // A 16 bit value is sent LSB, MSB to this address to divide
    // the base frequency to get the desired frequency.
    if (!msb0) {
      counter0=val;
      msb0=1;
    }
    else {
      counter0|=((val<<8)&0xFF00);
      msb0=0;
      if (counter0 == 0) counter0=1;  // Avoids possible divide by 0
      picotone.freq=1000000.0/(float)counter0;
      //SHOW("Frequency requested is %f Hz\n",picotone.freq);
    }
  }

  // E005 is ignored by this emulator, as it is not required to do anything

  // E006 is used for the clock (TI$ in BASIC)
  if (addr == 0xE006) {
    /* E006 - write the countdown value to counter 2 */
    /* This is a 16bit value, sent LSB, MSB */
    if (!msb2) {
      mzpico_rtc_init(); // (re)initialise the time to 00:00:00
      out2=true;         // Set output pin high to allow counter to decrement
      counter2=val;
      msb2=1;
    }
    else {
      counter2|=((val<<8)&0xFF00);
      msb2=0;
      c2start=counter2; /* Keep the start value so we can calculate elapsed */
                        /* seconds since initialisation of counter2 */
    }
  }

  return;
}

uint8_t rdE008(void) 
{
  // Implements TEMPO & note durations - this needs to sleep for 16ms per call
  // Each time this routine is called, the return value is incremented by 1
  sleep_ms(16);
  return(++e008call);
}

void wrE008(uint8_t data) 
{
  uint32_t *unused;

  if (data == 0) {
    // Disable sound generation if an alarm has been set
    if (tone_alarm) 
      mzpico_tone_off(tone_alarm, unused);
    return;
  }

  if (data == 1) {
    // Enable sound generation
    mzpico_tone_on();
    return;
  }
    
  SHOW("Error: wrE008 sound generator passed %d (0 or 1 expected)\n",data);
  return;
}
