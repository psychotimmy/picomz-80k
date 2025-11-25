/* Sharp MZ-80K & MZ-80A emulator - keyboard */
/* Tim Holyoake, August 2024 - February 2025 */

#include "picomz.h"

// The Sharp MZ-80K Keyboard Map based on SUC magazine article, July 2001.
// Corrected Row 5,key data bit 0x10 to be ; not : as per original article.

// Key data bit  0   1   2   3   4   5   6   7  
//          hex  01  02  04  08  10  20  40  80
//             The key data bit is 0 if pressed

//               !   #   %   '   )   +
// Row 0         1   3   5   7   9   -   G2  G4

//               "   $   &   (   pi
// Row 1         2   4   6   8   0   G1  G3  G5

//               <   <-  ]   @   :   *
// Row 2         Qq  Ee  Tt  Uu  Oo  =   G7  G9

//               >   [   \   ?   ^
// Row 3         Ww  Rr  Yy  Ii  Pp  G6  G8  G10

//               SPD DIA 
// Row 4         Aa  Dd  Gg  Jj  Ll  £   G12 G14

//               HRT CLB
// Row 5         Ss  Ff  Hh  Kk  ;   G11 G13 G15

//               ->                  SML
// Row 6         Zz  Cc  Bb  Mm  .   CAP G17 G19

//               v
// Row 7         Xx  Vv  Nn  ,   /   G16 G18 G20

//                   INS --- RC
// Row 8         LSH DEL --- LC  CR  RSH G22 G24

//               CLR     UP      ---
// Row 9         HOM SPC DWN BRK --- G21 G23 G25

// SML/CAP key is a toggle. If SML, then the 3rd character on
// the key (either lower case or a graphic) is input.

// Design decision 1 - map usb lower case keys to upper case
// to better mimic the way the MZ-80K keyboard works.

// The Sharp MZ-80A keyboard map is completely different to that of the
// MZ-80K. Diagram below based on section 3.1.2 (page 167) of the MZ-80A manual 
// and a SUC magazine article from July 2001. Row 9 of the keyboard layout
// completed (unshifted keys missing from SUC article).

// Key data bit  0   1   2   3   4   5   6   7  
//          hex  01  02  04  08  10  20  40  80
//             The key data bit is 0 if pressed

//                       --- --- --- --- --- BRK
// Row 0         SHF GPH --- --- --- --- --- CTL

//                   --- INS             !   "
// Row 1         Z   --- DEL A   Q   W   1   2 

//                                       #   $
// Row 2         C   X   S   D   E   R   3   4  

//                                       %   &
// Row 3         B   V   F   G   T   Y   5   6

//                                       '   (
// Row 4         SPC N   H   J   U   I   7   8                           

//                   <                   )   _
// Row 5         M   ,   K   L   O   P   9   0

//               >   <-- +   *   `   {   =   ~
// Row 6         .   /   ;   :   @   [   -   ^

//               up^ --- }   CR  UP  RC  |   CLR
// Row 7         ?   --- ]   ENT DWN LC  \   HOM

//               NP  NP  NP  NP  NP  NP  NP  NP
// Row 8         0   00  1   2   4   5   7   8  

//               NP  --- NP  --- NP  NP  NP  NP
// Row 9         .   --- 3   --- 6   -   9   +  

static uint8_t smlcapled=0;             // SML/CAPS toggle 
static int16_t tfno=0;                  // Current tape file number
static bool tfwd=true;                  // Tape direction - true = forwards
                                        // false = backwards

/* Low level USB keyboard handling. Used if we have an actual keyboard  */
/* rather than receiving keys via minicom etc.                          */
/* Although the USB keyboard could return up to 6 keys at the same      */
/* time, this makes little sense for the emulator. We therefore ignore  */
/* everything except the modifier and first key press in the buffer.    */
/* Simplifies the code and seems to work fine ... so far!               */

#define MZ_KEY_REPEAT_INIT     500   // Key held for 500ms before 1st repeat
#define MZ_KEY_REPEAT_INTERVAL  85   // Subsequent key repeats every 85ms

static uint8_t  rptcode;             // Store (possible) repeating key code
static uint8_t  rptmodifier;         // Store (possible) repeating modfier
static uint32_t rpttime;             // Time to repeat a key code

static uint8_t kaddr=0xFF;           // Keyboard address
static uint8_t kinst;                // Keyboard instance

static uint8_t kleds_now=0x00;       // Keyboard leds status now
static uint8_t kleds_prev=0xFF;      // Previous keyboard leds status

static bool numlock;                 // Numlock key status
static bool numlock_prev_rpt=false;  // Numlock pressed in previous report
static bool numlock_this_rpt=false;  // Numlock pressed in this report

// Used to send a repeating key to the MZ-80K/A
// and set status of NUM LOCK led
void mzrptkey(void)
{
  if (kaddr != 0xFF) {                // Check a keyboard is active

    // Set / unset NUM LOCK led as necessary
    if (kleds_now != kleds_prev) {
      tuh_hid_set_report(kaddr, kinst, 0, HID_REPORT_TYPE_OUTPUT, 
                         &kleds_now, sizeof(kleds_now));
      kleds_prev = kleds_now;
    }

    // Deal with repeating keys
    if (rptcode && (to_ms_since_boot(get_absolute_time()) > rpttime)) {
      // Send the repeating key to the MZ-80K/A and update next repeat time
      if (mzmodel == MZ80K)
        mzhidmapkey80k(rptcode,rptmodifier);
      else
        mzhidmapkey80a(rptcode,rptmodifier);
      rpttime += MZ_KEY_REPEAT_INTERVAL;
    }

  }

  return;
}

// Need to do very little here as the USB keyboard codes are dealt with
// by mzhidmapkey() rather than here
static void process_kbd_report(hid_keyboard_report_t const *report)
{

  // Clear the repeat key code if it's not the same as in current report
  if (report->keycode[0] != rptcode) {
    rptcode=0x00;
    rptmodifier=0x00;
    rpttime=0;
  }

  // Did the status of the Num Lock key change ?
  if (report->keycode[0] == 0x53) 
    numlock_this_rpt=true;
  else
    numlock_this_rpt=false;

  if (numlock_this_rpt && !numlock_prev_rpt) {
    numlock = !numlock;      // Toggle numlock key
    if (numlock)             // Change Num Lock LED
      kleds_now |= KEYBOARD_LED_NUMLOCK;
    else
      kleds_now &= ~KEYBOARD_LED_NUMLOCK;
  }
  numlock_prev_rpt=numlock_this_rpt;  // Save this status
   
  // Ignore anything less than 0x04 - no key, error conditions etc.
  if (report->keycode[0] > 0x03) {
    if (rptcode==0x00) {
      rptcode=report->keycode[0];   // Store new key for possible repeat
      rptmodifier=report->modifier; // Store new modifier for possible repeat
      rpttime=to_ms_since_boot(get_absolute_time())+MZ_KEY_REPEAT_INIT;   
                                    // Repeat key if initially held for 
                                    // MZ_KEY_REPEAT_INIT milliseconds
    }
    // We have a keypress to pass to the MZ-80K / MZ-80A
    if (mzmodel == MZ80K)
      mzhidmapkey80k(report->keycode[0],report->modifier);
    else
      mzhidmapkey80a(report->keycode[0],report->modifier);
  }
  else {
    // We have a key up - clear processkey array
    memset(processkey,0xFF,KBDROWS);
  }

  return;
}

// Executed when a new USB device (keyboard) is attached to the pico
void tuh_hid_mount_cb(uint8_t addr, uint8_t inst, 
                      uint8_t const *desc_report, uint16_t desc_len)
{
  uint8_t const itfp = tuh_hid_interface_protocol(addr,inst);
  uint8_t toggle=0;
  if (itfp!=HID_ITF_PROTOCOL_KEYBOARD) {
    // Do nothing if the HID isn't a keyboard
    return;
  }

  // Store keyboard address and instance - used to set NUM LOCK led
  kaddr=addr;
  kinst=inst;

  // Request to receive reports
  // tuh_hid_report_received_cb() callback invoked when a report is available
  while (!tuh_hid_receive_report(addr,inst)) {
    mzpicoled(toggle);
    sleep_ms(200); 
    toggle=!toggle;
  } 

  // Set NUM LOCK ON to start with on the MZ-80A (off on MZ-80K to conform
  // with version 1.x behaviour of the emulator)
  if (mzmodel == MZ80A) {
    kleds_now=KEYBOARD_LED_NUMLOCK;
    tuh_hid_set_report(kaddr, kinst, 0, HID_REPORT_TYPE_OUTPUT, 
                       &kleds_now, sizeof(kleds_now));
    kleds_prev=kleds_now;
    numlock=true;
  }

  return;
}

// tuh_hid_report_received_cb executed whenever data received from keyboard
void tuh_hid_report_received_cb(uint8_t addr, uint8_t inst, 
                                uint8_t const* report, uint16_t len)
{
  uint8_t toggle=0;
  process_kbd_report((hid_keyboard_report_t const*) report);
  // request to receive next report
  // tuh_hid_report_received_cb() invoked when next report is available
  while (!tuh_hid_receive_report(addr,inst)) {
    mzpicoled(toggle);
    sleep_ms(200); 
    toggle=!toggle;
  } 

  return;
}

// tuh_hid_umount_cb is executed when a device is unmounted.
// dummy function on the pico.
void tuh_hid_umount_cb(uint8_t addr, uint8_t inst)
{
  return;
}

/* Convert USB HID key press to the MZ-80K keyboard map,             */
/* then store in the processkey[] global (read on portB by the 8255) */
void mzhidmapkey80k(uint8_t usbk0, uint8_t modifier) 
{
  int16_t tftemp;                         // Temporary tape file variable

  /* Unshifted USB keys */
  if (modifier == 0x00) {
    switch (usbk0) {

      case 0x04: processkey[4]=0x01^0xFF; //A  
                 break;
      case 0x05: processkey[6]=0x04^0xFF; //B  
                 break;
      case 0x06: processkey[6]=0x02^0xFF; //C  
                 break;
      case 0x07: processkey[4]=0x02^0xFF; //D
                 break;
      case 0x08: processkey[2]=0x02^0xFF; //E
                 break;
      case 0x09: processkey[5]=0x02^0xFF; //F
                 break;
      case 0x0a: processkey[4]=0x04^0xFF; //G
                 break;
      case 0x0b: processkey[5]=0x04^0xFF; //H
                 break;
      case 0x0c: processkey[3]=0x08^0xFF; //I
                 break;
      case 0x0d: processkey[4]=0x08^0xFF; //J
                 break;
      case 0x0e: processkey[5]=0x08^0xFF; //K
                 break;
      case 0x0f: processkey[4]=0x10^0xFF; //L
                 break;
      case 0x10: processkey[6]=0x08^0xFF; //M
                 break;
      case 0x11: processkey[7]=0x04^0xFF; //N
                 break;
      case 0x12: processkey[2]=0x10^0xFF; //O
                 break;
      case 0x13: processkey[3]=0x10^0xFF; //P
                 break;
      case 0x14: processkey[2]=0x01^0xFF; //Q
                 break;
      case 0x15: processkey[3]=0x02^0xFF; //R
                 break;
      case 0x16: processkey[5]=0x01^0xFF; //S
                 break;
      case 0x17: processkey[2]=0x04^0xFF; //T
                 break;
      case 0x18: processkey[2]=0x08^0xFF; //U
                 break;
      case 0x19: processkey[7]=0x02^0xFF; //V
                 break;
      case 0x1a: processkey[3]=0x01^0xFF; //W
                 break;
      case 0x1b: processkey[7]=0x01^0xFF; //X
                 break;
      case 0x1c: processkey[3]=0x04^0xFF; //Y
                 break;
      case 0x1d: processkey[6]=0x01^0xFF; //Z
                 break;

      case 0x1e: processkey[0]=0x01^0xFF; //1  
                 break;
      case 0x1f: processkey[1]=0x01^0xFF; //2  
                 break;
      case 0x20: processkey[0]=0x02^0xFF; //3  
                 break;
      case 0x21: processkey[1]=0x02^0xFF; //4  
                 break;
      case 0x22: processkey[0]=0x04^0xFF; //5  
                 break;
      case 0x23: processkey[1]=0x04^0xFF; //6  
                 break;
      case 0x24: processkey[0]=0x08^0xFF; //7  
                 break;
      case 0x25: processkey[1]=0x08^0xFF; //8  
                 break;
      case 0x26: processkey[0]=0x10^0xFF; //9  
                 break;
      case 0x27: processkey[1]=0x10^0xFF; //0  
                 break;

      case 0x28: processkey[8]=0x10^0xFF; //<CR>    (USB return key)
                 break;
      case 0x2a: processkey[8]=0x02^0xFF; //<DEL>   (USB backspace)
                 break;
      case 0x2c: processkey[9]=0x02^0xFF; //<SPACE>
                 break;
      case 0x2d: processkey[0]=0x20^0xFF; //-
                 break;
      case 0x2e: processkey[2]=0x20^0xFF; //=
                 break;
      case 0x2f: processkey[8]=0x01^0xFF; //[
                 processkey[3]=0x02^0xFF;
                 break;
      case 0x30: processkey[8]=0x01^0xFF; //]
                 processkey[2]=0x04^0xFF;
                 break;
      case 0x32: processkey[8]=0x01^0xFF; //#
                 processkey[0]=0x02^0xFF;
                 break;
      case 0x33: processkey[5]=0x10^0xFF; //;
                 break;
      case 0x34: processkey[8]=0x01^0xFF; //'
                 processkey[0]=0x08^0xFF;
                 break;
      case 0x36: processkey[7]=0x08^0xFF; //,
                 break;
      case 0x37: processkey[6]=0x10^0xFF; //.
                 break;
      case 0x38: processkey[7]=0x10^0xFF; ///
                 break;

      case 0x3a: //F1 - Not mapped to a MZ-80K key
                 if (!tfwd) {             // Reverse if tape not going forward
                   tfwd=true;
                   tfno++;
                 }
                 tftemp=tapeloader(tfno);      
                 if (tftemp >= 0) {       // If not at end of tape, increment
                   ++tfno;                // the tape file number
                 }
                 else {                   // Otherwise step back 1 file
                   --tfno;                // and preload it to memory again
                   tftemp=tapeloader(tfno);
                 }
                 break;
      case 0x3b: //F2 - Not mapped to a MZ-80K key
                 if (tfwd) {              // Reverse if tape not going back
                   tfwd=false;
                   tfno--;
                 }
                 if (tfno > 0) {          // Step back one file if not at
                   --tfno;                // first file on tape.
                 }
                 tftemp=tapeloader(tfno); // Preload the file
                 if (tfno < 0) {          // Oh - we're off the other end!!
                   tfno=0;                // Shouldn't happen ... but ...
                   tfno=tapeloader(tfno);
                 }
                 break;
      case 0x3c: //F3 - Not mapped to a MZ-80K key
                 mzspinny(0);             // Reset tape counter
                 break;
      case 0x3d: //F4 - Not mapped to a MZ-80K key
                 memset(mzemustatus,0x00,EMUSSIZE); // Clear status area
                 break;
                 
      case 0x3e: //F5 - Not mapped to a MZ-80K key
                 uint16_t temp;
                 temp=whitepix;           // Reverse video
                 whitepix=blackpix;
                 blackpix=temp;
                 break;

      case 0x3f: //F6 - Not mapped to a MZ-80K key
                 ukrom=!ukrom;            // Toggle between UK and JP CGROM
                 memset(mzemustatus,0x00,EMUSSIZE); // Clear status area
                 break;

      case 0x44: mzreaddump();            //F11 - read memory dump
                 break;
      case 0x45: mzsavedump();            //F12 - save memory dump
                 break;

      case 0x49: processkey[8]=0x03^0xFF; //<INS>  (USB Insert)
                 break;
      case 0x4a: processkey[9]=0x01^0xFF; //<HOME> (USB Home)
                 break;
      case 0x4b: processkey[8]=0x01^0xFF; //Shift BREAK (USB PgUp)
                 processkey[9]=0x08^0xFF;
                 // Shift break always resets the cassette deck states 
                 reset_tape(); 
                 break;
      case 0x4c: processkey[8]=0x02^0xFF; //<DEL> (USB Delete forward)
                 break;
      case 0x4d: processkey[8]=0x01^0xFF; //<CLR> (USB End)
                 processkey[9]=0x01^0xFF;
                 break;
      case 0x4e: processkey[9]=0x08^0xFF; //BREAK (unshifted) (USB PgDn)
                 // Break always resets the cassette deck states 
                 reset_tape(); 
                 break;

      case 0x4f: processkey[8]=0x08^0xFF; //cursor left
                 break;
      case 0x50: processkey[8]=0x09^0xFF; //cursor right
                 break;
      case 0x51: processkey[9]=0x04^0xFF; //cursor down
                 break;
      case 0x52: processkey[8]=0x01^0xFF; //cursor up
                 processkey[9]=0x04^0xFF;
                 break;

      // 0x53 - NUM LOCK -  is dealt with before this function is called

      case 0x54: processkey[7]=0x10^0xFF; ///
                 break;
      case 0x55: processkey[8]=0x01^0xFF; //*
                 processkey[2]=0x20^0xFF;
                 break;
      case 0x56: processkey[0]=0x20^0xFF; //-
                 break;
      case 0x57: processkey[8]=0x01^0xFF; //+
                 processkey[0]=0x20^0xFF;
                 break;
      case 0x58: processkey[8]=0x10^0xFF; //<CR>  (USB keypad Enter)
                 break;

      case 0x59: if (numlock) 
                   processkey[0]=0x01^0xFF; //1  
                 else {
                   processkey[8]=0x01^0xFF; //<CLR> (USB End)
                   processkey[9]=0x01^0xFF;
                 }
                 break;
      case 0x5a: if (numlock) 
                   processkey[1]=0x01^0xFF; //2  
                 else
                   processkey[9]=0x04^0xFF; //cursor down
                 break;
      case 0x5b: if (numlock) 
                   processkey[0]=0x02^0xFF; //3  
                 else
                   processkey[9]=0x08^0xFF; //BREAK (unshifted) (USB PgDn)
                 break;
      case 0x5c: if (numlock) 
                   processkey[1]=0x02^0xFF; //4  
                 else
                   processkey[8]=0x09^0xFF; //cursor left
                 break;
      case 0x5d: if (numlock) 
                   processkey[0]=0x04^0xFF; //5  
                 break;
      case 0x5e: if (numlock) 
                   processkey[1]=0x04^0xFF; //6  
                 else
                   processkey[8]=0x08^0xFF; //cursor right
                 break;
      case 0x5f: if (numlock) 
                   processkey[0]=0x08^0xFF; //7  
                 else
                   processkey[9]=0x01^0xFF; //home (HOME)
                 break;
      case 0x60: if (numlock) 
                   processkey[1]=0x08^0xFF; //8  
                 else {
                   processkey[8]=0x01^0xFF; //cursor up
                   processkey[9]=0x04^0xFF;
                 }
                 break;
      case 0x61: if (numlock) 
                   processkey[0]=0x10^0xFF; //9  
                 else {
                   processkey[8]=0x01^0xFF; //Shift BREAK (USB PgUp)
                   processkey[9]=0x08^0xFF;
                 }
                 break;
      case 0x62: if (numlock) 
                   processkey[1]=0x10^0xFF; //0  
                 else
                   processkey[8]=0x03^0xFF; //insert (INS)
                 break;
      case 0x63: if (numlock) 
                   processkey[6]=0x10^0xFF; //.
                 else
                   processkey[8]=0x02^0xFF; //delete (DEL)
                 break;

      case 0x64: processkey[8]=0x01^0xFF; //backslash (non US USB key 102)
                 processkey[3]=0x04^0xFF;
                 break;

      default:   break;                   // Ignore unmapped keys

    }
  }

  /* Shifted USB keys */
  if ((modifier == 0x02) || (modifier == 0x20)) {
    switch (usbk0) {

      case 0x04: processkey[8]=0x01^0xFF; // (MZ-80K shift A)
                 processkey[4]=0x01^0xFF;
                 break;
      case 0x05: processkey[8]=0x01^0xFF; // (MZ-80K shift B)
                 processkey[6]=0x04^0xFF;
                 break;
      case 0x06: processkey[8]=0x01^0xFF; // (MZ-80K shift C)
                 processkey[6]=0x02^0xFF;
                 break;
      case 0x07: processkey[8]=0x01^0xFF; // (MZ-80K shift D)
                 processkey[4]=0x02^0xFF;
                 break;
      case 0x08: processkey[8]=0x01^0xFF; // (MZ-80K shift E)
                 processkey[2]=0x02^0xFF;
                 break;
      case 0x09: processkey[8]=0x01^0xFF; // (MZ-80K shift F)
                 processkey[5]=0x02^0xFF;
                 break;
      case 0x0a: processkey[8]=0x01^0xFF; // (MZ-80K shift G)
                 processkey[4]=0x04^0xFF;
                 break;
      case 0x0b: processkey[8]=0x01^0xFF; // (MZ-80K shift H)
                 processkey[5]=0x04^0xFF;
                 break;
      case 0x0c: processkey[8]=0x01^0xFF; // (MZ-80K shift I)
                 processkey[3]=0x08^0xFF;
                 break;
      case 0x0d: processkey[8]=0x01^0xFF; // (MZ-80K shift J)
                 processkey[4]=0x08^0xFF;
                 break;
      case 0x0e: processkey[8]=0x01^0xFF; // (MZ-80K shift K)
                 processkey[5]=0x08^0xFF;
                 break;
      case 0x0f: processkey[8]=0x01^0xFF; // (MZ-80K shift L)
                 processkey[4]=0x10^0xFF;
                 break;
      case 0x10: processkey[8]=0x01^0xFF; // (MZ-80K shift M)
                 processkey[6]=0x08^0xFF;
                 break;
      case 0x11: processkey[8]=0x01^0xFF; // (MZ-80K shift N)
                 processkey[7]=0x04^0xFF;
                 break;
      case 0x12: processkey[8]=0x01^0xFF; // (MZ-80K shift O)
                 processkey[2]=0x10^0xFF;
                 break;
      case 0x13: processkey[8]=0x01^0xFF; // (MZ-80K shift P)
                 processkey[3]=0x10^0xFF;
                 break;
      case 0x14: processkey[8]=0x01^0xFF; // (MZ-80K shift Q)
                 processkey[2]=0x01^0xFF;
                 break;
      case 0x15: processkey[8]=0x01^0xFF; // (MZ-80K shift R)
                 processkey[3]=0x02^0xFF;
                 break;
      case 0x16: processkey[8]=0x01^0xFF; // (MZ-80K shift S)
                 processkey[5]=0x01^0xFF;
                 break;
      case 0x17: processkey[8]=0x01^0xFF; // (MZ-80K shift T)
                 processkey[2]=0x04^0xFF;
                 break;
      case 0x18: processkey[8]=0x01^0xFF; // (MZ-80K shift U)
                 processkey[2]=0x08^0xFF;
                 break;
      case 0x19: processkey[8]=0x01^0xFF; // (MZ-80K shift V)
                 processkey[7]=0x02^0xFF;
                 break;
      case 0x1a: processkey[8]=0x01^0xFF; // (MZ-80K shift W)
                 processkey[3]=0x01^0xFF;
                 break;
      case 0x1b: processkey[8]=0x01^0xFF; // (MZ-80K shift X)
                 processkey[7]=0x01^0xFF;
                 break;
      case 0x1c: processkey[8]=0x01^0xFF; // (MZ-80K shift Y)
                 processkey[3]=0x04^0xFF;
                 break;
      case 0x1d: processkey[8]=0x01^0xFF; // (MZ-80K shift Z)
                 processkey[6]=0x01^0xFF;
                 break;
      case 0x1e: processkey[8]=0x01^0xFF; //!
                 processkey[0]=0x01^0xFF;
                 break;
      case 0x1f: processkey[8]=0x01^0xFF; //"
                 processkey[1]=0x01^0xFF;
                 break;
      case 0x20: processkey[4]=0x20^0xFF; //£
                 break;
      case 0x21: processkey[8]=0x01^0xFF; //$
                 processkey[1]=0x02^0xFF;
                 break;
      case 0x22: processkey[8]=0x01^0xFF; //%
                 processkey[0]=0x04^0xFF;
                 break;
      case 0x23: processkey[8]=0x01^0xFF; //pi (shifted 6 - ^ on USB kbd)
                 processkey[1]=0x10^0xFF;
                 break;
      case 0x24: processkey[8]=0x01^0xFF; //&
                 processkey[1]=0x04^0xFF;
                 break;
      case 0x25: processkey[8]=0x01^0xFF; //*
                 processkey[2]=0x20^0xFF;
                 break;
      case 0x26: processkey[8]=0x01^0xFF; //(
                 processkey[1]=0x08^0xFF;
                 break;
      case 0x27: processkey[8]=0x01^0xFF; //)
                 processkey[0]=0x10^0xFF;
                 break;
      case 0x2e: processkey[8]=0x01^0xFF; //+
                 processkey[0]=0x20^0xFF;
                 break;

      // SML/CAPS toggle. portC bit2 is 1 at boot (green led).
      // When latched, this sets portC bit 2 to 0 (red led) and affects
      // the characters displayed - e.g. A becomes a. Even though the
      // SML/CAPS key is latched on the MZ-80K keyboard, it's treated as
      // a shifted character, hence the need to set processkey[8].
      // mzpicoled() is used to turn the inbuilt pico led on (SML) or off.

      case 0x32: if ((portC>>2)&0x01)      //SML/CAPS toggle (~ on UK kbd)
                   processkey[8]=0x01^0xFF;
                 processkey[6]=0x20^0xFF;  
                 smlcapled=!smlcapled;
                 mzpicoled(smlcapled);
                 break;
      case 0x33: processkey[8]=0x01^0xFF; //:
                 processkey[2]=0x10^0xFF;
                 break;
      case 0x34: processkey[8]=0x01^0xFF; //@
                 processkey[2]=0x08^0xFF;
                 break;
      case 0x36: processkey[8]=0x01^0xFF; //< 
                 processkey[2]=0x01^0xFF;
                 break;
      case 0x37: processkey[8]=0x01^0xFF; //>
                 processkey[3]=0x01^0xFF;
                 break;
      case 0x38: processkey[8]=0x01^0xFF; //?
                 processkey[3]=0x08^0xFF;
                 break;
      default:   break;
    }
  }

  /* Alt USB keys */
  if ((modifier == 0x04) || (modifier == 0x40)) {
    switch (usbk0) {

      case 0x14: processkey[1]=0x20^0xFF; //Q - Graphics 1 (top left blue key)
                 break;
      case 0x1a: processkey[0]=0x40^0xFF; //W - Graphics 2
                 break;
      case 0x08: processkey[1]=0x40^0xFF; //E - Graphics 3 
                 break;
      case 0x15: processkey[0]=0x80^0xFF; //R - Graphics 4 
                 break;
      case 0x17: processkey[1]=0x80^0xFF; //T - Graphics 5 
                 break;

      case 0x1c: processkey[3]=0x20^0xFF; //Y - Graphics 6 
                 break;
      case 0x18: processkey[2]=0x40^0xFF; //U - Graphics 7
                 break;
      case 0x0c: processkey[3]=0x40^0xFF; //I - Graphics 8 
                 break;
      case 0x12: processkey[2]=0x80^0xFF; //O - Graphics 9 
                 break;
      case 0x13: processkey[3]=0x80^0xFF; //P - Graphics 10
                 break;

      case 0x04: processkey[5]=0x20^0xFF; //A - Graphics 11
                 break;
      case 0x16: processkey[4]=0x40^0xFF; //S - Graphics 12
                 break;
      case 0x07: processkey[5]=0x40^0xFF; //D - Graphics 13
                 break;
      case 0x09: processkey[4]=0x80^0xFF; //F - Graphics 14
                 break;
      case 0x0a: processkey[5]=0x80^0xFF; //G - Graphics 15
                 break;

      case 0x0b: processkey[7]=0x20^0xFF; //H - Graphics 16
                 break;
      case 0x0d: processkey[6]=0x40^0xFF; //J - Graphics 17
                 break;
      case 0x0e: processkey[7]=0x40^0xFF; //K - Graphics 18
                 break;
      case 0x0f: processkey[6]=0x80^0xFF; //L - Graphics 19
                 break;
      case 0x10: processkey[7]=0x80^0xFF; //M - Graphics 20
                 break;

      case 0x1d: processkey[9]=0x20^0xFF; //Z - Graphics 21
                 break;
      case 0x1b: processkey[8]=0x40^0xFF; //X - Graphics 22
                 break;
      case 0x06: processkey[9]=0x40^0xFF; //C - Graphics 23
                 break;
      case 0x19: processkey[8]=0x80^0xFF; //V - Graphics 24
                 break;
      case 0x05: processkey[9]=0x80^0xFF; //B - Graphics 25
                 break;
  
      default:   break;
    }
  }

  /* Shift Alt USB keys */
  if ((modifier == 0x06) || (modifier == 0x60) || 
      (modifier == 0x24) || (modifier == 0x42) ) {
    switch (usbk0) {

      case 0x14: processkey[1]=0x20^0xFF; //Q - Graphics 1 (top left blue key)
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x1a: processkey[0]=0x40^0xFF; //W - Graphics 2
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x08: processkey[1]=0x40^0xFF; //E - Graphics 3 
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x15: processkey[0]=0x80^0xFF; //R - Graphics 4 
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x17: processkey[1]=0x80^0xFF; //T - Graphics 5 
                 processkey[8]=0x01^0xFF; 
                 break;

      case 0x1c: processkey[3]=0x20^0xFF; //Y - Graphics 6 
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x18: processkey[2]=0x40^0xFF; //U - Graphics 7
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x0c: processkey[3]=0x40^0xFF; //I - Graphics 8 
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x12: processkey[2]=0x80^0xFF; //O - Graphics 9 
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x13: processkey[3]=0x80^0xFF; //P - Graphics 10
                 processkey[8]=0x01^0xFF; 
                 break;

      case 0x04: processkey[5]=0x20^0xFF; //A - Graphics 11
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x16: processkey[4]=0x40^0xFF; //S - Graphics 12
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x07: processkey[5]=0x40^0xFF; //D - Graphics 13
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x09: processkey[4]=0x80^0xFF; //F - Graphics 14
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x0a: processkey[5]=0x80^0xFF; //G - Graphics 15
                 processkey[8]=0x01^0xFF; 
                 break;

      case 0x0b: processkey[7]=0x20^0xFF; //H - Graphics 16
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x0d: processkey[6]=0x40^0xFF; //J - Graphics 17
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x0e: processkey[7]=0x40^0xFF; //K - Graphics 18
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x0f: processkey[6]=0x80^0xFF; //L - Graphics 19
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x10: processkey[7]=0x80^0xFF; //M - Graphics 20
                 processkey[8]=0x01^0xFF; 
                 break;

      case 0x1d: processkey[9]=0x20^0xFF; //Z - Graphics 21
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x1b: processkey[8]=0x41^0xFF; //X - Graphics 22
                 break;
      case 0x06: processkey[9]=0x40^0xFF; //C - Graphics 23
                 processkey[8]=0x01^0xFF; 
                 break;
      case 0x19: processkey[8]=0x81^0xFF; //V - Graphics 24
                 break;
      case 0x05: processkey[9]=0x80^0xFF; //B - Graphics 25
                 processkey[8]=0x01^0xFF; 
                 break;
  
      default:   break;
    }
  }

  /* Misc ctrl USB keys */
  if ((modifier == 0x01) || (modifier == 0x10)) {
    switch (usbk0) {

      case 0x0b: processkey[8]=0x02^0xFF; //<DEL>   (ctrl H)
                 break;
      case 0x0f: processkey[8]=0x01^0xFF; //left <SHIFT>  (ctrl L)
                 break;
      case 0x10: processkey[8]=0x10^0xFF; //<CR>    (ctrl M)
                 break;
      case 0x15: processkey[8]=0x20^0xFF; //right <SHIFT> (ctrl R)
                 break;
      default:   break;
    }
  }

  return;
}

/* Convert USB HID key press to the MZ-80A keyboard map,             */
/* then store in the processkey[] global (read on portB by the 8255) */
void mzhidmapkey80a(uint8_t usbk0, uint8_t modifier) 
{
  int16_t tftemp;                         // Temporary tape file variable

  /* Unshifted USB keys */
  if (modifier == 0x00) {
    switch (usbk0) {

      case 0x04: processkey[1]=0x08^0xFF; //A  
                 break;
      case 0x05: processkey[3]=0x01^0xFF; //B  
                 break;
      case 0x06: processkey[2]=0x01^0xFF; //C  
                 break;
      case 0x07: processkey[2]=0x08^0xFF; //D
                 break;
      case 0x08: processkey[2]=0x10^0xFF; //E
                 break;
      case 0x09: processkey[3]=0x04^0xFF; //F
                 break;
      case 0x0a: processkey[3]=0x08^0xFF; //G
                 break;
      case 0x0b: processkey[4]=0x04^0xFF; //H
                 break;
      case 0x0c: processkey[4]=0x20^0xFF; //I
                 break;
      case 0x0d: processkey[4]=0x08^0xFF; //J
                 break;
      case 0x0e: processkey[5]=0x04^0xFF; //K
                 break;
      case 0x0f: processkey[5]=0x08^0xFF; //L
                 break;
      case 0x10: processkey[5]=0x01^0xFF; //M
                 break;
      case 0x11: processkey[4]=0x02^0xFF; //N
                 break;
      case 0x12: processkey[5]=0x10^0xFF; //O
                 break;
      case 0x13: processkey[5]=0x20^0xFF; //P
                 break;
      case 0x14: processkey[1]=0x10^0xFF; //Q
                 break;
      case 0x15: processkey[2]=0x20^0xFF; //R
                 break;
      case 0x16: processkey[2]=0x04^0xFF; //S
                 break;
      case 0x17: processkey[3]=0x10^0xFF; //T
                 break;
      case 0x18: processkey[4]=0x10^0xFF; //U
                 break;
      case 0x19: processkey[3]=0x02^0xFF; //V
                 break;
      case 0x1a: processkey[1]=0x20^0xFF; //W
                 break;
      case 0x1b: processkey[2]=0x02^0xFF; //X
                 break;
      case 0x1c: processkey[3]=0x20^0xFF; //Y
                 break;
      case 0x1d: processkey[1]=0x01^0xFF; //Z
                 break;

      case 0x1e: processkey[1]=0x40^0xFF; //1  
                 break;
      case 0x1f: processkey[1]=0x80^0xFF; //2  
                 break;
      case 0x20: processkey[2]=0x40^0xFF; //3  
                 break;
      case 0x21: processkey[2]=0x80^0xFF; //4  
                 break;
      case 0x22: processkey[3]=0x40^0xFF; //5  
                 break;
      case 0x23: processkey[3]=0x80^0xFF; //6  
                 break;
      case 0x24: processkey[4]=0x40^0xFF; //7  
                 break;
      case 0x25: processkey[4]=0x80^0xFF; //8  
                 break;
      case 0x26: processkey[5]=0x40^0xFF; //9  
                 break;
      case 0x27: processkey[5]=0x80^0xFF; //0  
                 break;

      case 0x28: processkey[7]=0x08^0xFF; //<CR>    (USB return key)
                 break;
      case 0x29: processkey[0]=0x02^0xFF; //<GRAPH> toggle (USB ESC key)
                 break;                   
      case 0x2a: processkey[1]=0x04^0xFF; //<DEL>   (USB backspace)
                 break;
      case 0x2c: processkey[4]=0x01^0xFF; //<SPACE>
                 break;
      case 0x2d: processkey[6]=0x40^0xFF; //-
                 break;
      case 0x2e: processkey[0]=0x01^0xFF; //=
                 processkey[6]=0x40^0xFF;
                 break;
      case 0x2f: processkey[6]=0x20^0xFF; //[
                 break;
      case 0x30: processkey[7]=0x04^0xFF; //]
                 break;
      case 0x32: processkey[0]=0x01^0xFF; //#
                 processkey[2]=0x40^0xFF;
                 break;
      case 0x33: processkey[6]=0x04^0xFF; //;
                 break;
      case 0x34: processkey[0]=0x01^0xFF; //'
                 processkey[4]=0x40^0xFF;
                 break;
      case 0x35: processkey[0]=0x01^0xFF; //`
                 processkey[6]=0x10^0xFF;
                 break;
      case 0x36: processkey[5]=0x02^0xFF; //,
                 break;
      case 0x37: processkey[6]=0x01^0xFF; //.
                 break;
      case 0x38: processkey[6]=0x02^0xFF; ///
                 break;

      case 0x3a: //F1 - Not mapped to a MZ-80A key
                 if (!tfwd) {             // Reverse if tape not going forward
                   tfwd=true;
                   tfno++;
                 }
                 tftemp=tapeloader(tfno);      
                 if (tftemp >= 0) {       // If not at end of tape, increment
                   ++tfno;                // the tape file number
                 }
                 else {                   // Otherwise step back 1 file
                   --tfno;                // and preload it to memory again
                   tftemp=tapeloader(tfno);
                 }
                 break;
      case 0x3b: //F2 - Not mapped to a MZ-80A key
                 if (tfwd) {              // Reverse if tape not going back
                   tfwd=false;
                   tfno--;
                 }
                 if (tfno > 0) {          // Step back one file if not at
                   --tfno;                // first file on tape.
                 }
                 tftemp=tapeloader(tfno); // Preload the file
                 if (tfno < 0) {          // Oh - we're off the other end!!
                   tfno=0;                // Shouldn't happen ... but ...
                   tfno=tapeloader(tfno);
                 }
                 break;
      case 0x3c: //F3 - Not mapped to a MZ-80A key
                 mzspinny(0);             // Reset tape counter
                 break;
      case 0x3d: //F4 - Not mapped to a MZ-80A key
                 memset(mzemustatus,0x00,EMUSSIZE); // Clear status area
                 break;
      case 0x43: mzcpu.pc=0x0000;         //F10 - MZ-80A reset button
                 reset_tape(); 
                 break;
      case 0x44: mzreaddump();            //F11 - read memory dump
                 break;
      case 0x45: mzsavedump();            //F12 - save memory dump
                 break;

      case 0x49: processkey[0]=0x01^0xFF; //<INS>  (USB Insert)
                 processkey[1]=0x04^0xFF;
                 break;
      case 0x4a: processkey[7]=0x80^0xFF; //<HOME> (USB Home)
                 break;
      case 0x4b: processkey[0]=0x81^0xFF; //Shift BREAK (USB PgUp)
                 // Shift break always resets the cassette deck states 
                 reset_tape(); 
                 break;
      case 0x4c: processkey[1]=0x04^0xFF; //<DEL> (USB Delete forward)
                 break;
      case 0x4d: processkey[0]=0x01^0xFF; //<CLR> (USB End)
                 processkey[7]=0x80^0xFF;
                 break;
      case 0x4e: processkey[0]=0x81^0xFF; //Shift BREAK (USB PgDn)
                 // Shift break always resets the cassette deck states 
                 reset_tape(); 
                 break;
      case 0x4f: processkey[7]=0x20^0xFF; //cursor left
                 break;
      case 0x50: processkey[0]=0x01^0xFF; //cursor right
                 processkey[7]=0x20^0xFF;
                 break;
      case 0x51: processkey[0]=0x01^0xFF; //cursor up
                 processkey[7]=0x10^0xFF;
                 break;
      case 0x52: processkey[7]=0x10^0xFF; //cursor down
                 break;

      // 0x53 - NUM LOCK -  is dealt with before this function is called

      case 0x54: processkey[0]=0x01^0xFF; // up arrow (USB keypad /)
                 processkey[7]=0x01^0xFF;
                 break;
      case 0x55: processkey[8]=0x02^0xFF; // *, but assigned to 00
                 break;
      case 0x56: processkey[9]=0x20^0xFF; //-
                 break;
      case 0x57: processkey[9]=0x80^0xFF; //+
                 break;
      case 0x58: processkey[7]=0x08^0xFF; //<CR>  (USB keypad Enter)
                 break;

      case 0x59: if (numlock) 
                   processkey[8]=0x04^0xFF; //1  
                 else {
                   processkey[0]=0x01^0xFF; //<CLR> (USB End)
                   processkey[7]=0x80^0xFF;
                 }
                 break;
      case 0x5a: if (numlock) 
                   processkey[8]=0x08^0xFF; //2  
                 else {
                   processkey[0]=0x01^0xFF; //cursor up
                   processkey[7]=0x10^0xFF;
                 }
                 break;
      case 0x5b: if (numlock) 
                   processkey[9]=0x04^0xFF; //3  
                 else {
                   processkey[0]=0x01^0xFF; //shifted ? = up arrow (USB PgDn)
                   processkey[7]=0x01^0xFF; 
                 }
                 break;
      case 0x5c: if (numlock) 
                   processkey[8]=0x10^0xFF; //4  
                 else {
                   processkey[0]=0x01^0xFF; //cursor left
                   processkey[7]=0x20^0xFF;
                 }
                 break;
      case 0x5d: if (numlock) 
                   processkey[8]=0x20^0xFF; //5  
                 break;
      case 0x5e: if (numlock) 
                   processkey[9]=0x10^0xFF; //6  
                 else 
                   processkey[7]=0x20^0xFF; //cursor right
                 break;
      case 0x5f: if (numlock) 
                   processkey[8]=0x40^0xFF; //7  
                 else
                   processkey[7]=0x80^0xFF; //home (HOME)
                 break;
      case 0x60: if (numlock) 
                   processkey[8]=0x80^0xFF; //8  
                 else
                   processkey[7]=0x10^0xFF; //cursor down
                 break;
      case 0x61: if (numlock) 
                   processkey[9]=0x40^0xFF; //9  
                 else
                   processkey[0]=0x81^0xFF; //Shift BREAK (USB PgUp)
                 break;
      case 0x62: if (numlock) 
                   processkey[8]=0x01^0xFF; //0  
                 else {
                   processkey[8]=0x01^0xFF; //insert (INS)
                   processkey[1]=0x04^0xFF;
                 }
                 break;
      case 0x63: if (numlock) 
                   processkey[9]=0x01^0xFF; //.
                 else
                   processkey[1]=0x04^0xFF; //delete (DEL)
                 break;

      case 0x64: processkey[7]=0x40^0xFF; //backslash (non US USB key 102)
                 break;

      default:   break;                   // Ignore unmapped keys

    }
  }

  /* Shifted USB keys */
  if ((modifier == 0x02) || (modifier == 0x20)) {
    switch (usbk0) {

      case 0x04: processkey[0]=0x01^0xFF; //a
                 processkey[1]=0x08^0xFF;
                 break;
      case 0x05: processkey[0]=0x01^0xFF; //b
                 processkey[3]=0x01^0xFF;
                 break;
      case 0x06: processkey[0]=0x01^0xFF; //c
                 processkey[2]=0x01^0xFF;
                 break;
      case 0x07: processkey[0]=0x01^0xFF; //d
                 processkey[2]=0x08^0xFF;
                 break;
      case 0x08: processkey[0]=0x01^0xFF; //e
                 processkey[2]=0x10^0xFF;
                 break;
      case 0x09: processkey[0]=0x01^0xFF; //f
                 processkey[3]=0x04^0xFF;
                 break;
      case 0x0a: processkey[0]=0x01^0xFF; //g
                 processkey[3]=0x08^0xFF;
                 break;
      case 0x0b: processkey[0]=0x01^0xFF; //h
                 processkey[4]=0x04^0xFF;
                 break;
      case 0x0c: processkey[0]=0x01^0xFF; //i
                 processkey[4]=0x20^0xFF;
                 break;
      case 0x0d: processkey[0]=0x01^0xFF; //j
                 processkey[4]=0x08^0xFF;
                 break;
      case 0x0e: processkey[0]=0x01^0xFF; //k
                 processkey[5]=0x04^0xFF;
                 break;
      case 0x0f: processkey[0]=0x01^0xFF; //l
                 processkey[5]=0x08^0xFF;
                 break;
      case 0x10: processkey[0]=0x01^0xFF; //m
                 processkey[5]=0x01^0xFF;
                 break;
      case 0x11: processkey[0]=0x01^0xFF; //n
                 processkey[4]=0x02^0xFF;
                 break;
      case 0x12: processkey[0]=0x01^0xFF; //o
                 processkey[5]=0x10^0xFF;
                 break;
      case 0x13: processkey[0]=0x01^0xFF; //p
                 processkey[5]=0x20^0xFF;
                 break;
      case 0x14: processkey[0]=0x01^0xFF; //q
                 processkey[1]=0x10^0xFF;
                 break;
      case 0x15: processkey[0]=0x01^0xFF; //r
                 processkey[2]=0x20^0xFF;
                 break;
      case 0x16: processkey[0]=0x01^0xFF; //s
                 processkey[2]=0x04^0xFF;
                 break;
      case 0x17: processkey[0]=0x01^0xFF; //t
                 processkey[3]=0x10^0xFF;
                 break;
      case 0x18: processkey[0]=0x01^0xFF; //u
                 processkey[4]=0x10^0xFF;
                 break;
      case 0x19: processkey[0]=0x01^0xFF; //v
                 processkey[3]=0x02^0xFF;
                 break;
      case 0x1a: processkey[0]=0x01^0xFF; //w
                 processkey[1]=0x20^0xFF;
                 break;
      case 0x1b: processkey[0]=0x01^0xFF; //x
                 processkey[2]=0x02^0xFF;
                 break;
      case 0x1c: processkey[0]=0x01^0xFF; //y
                 processkey[3]=0x20^0xFF;
                 break;
      case 0x1d: processkey[0]=0x01^0xFF; //z
                 processkey[1]=0x01^0xFF;
                 break;
      case 0x1e: processkey[0]=0x01^0xFF; //!
                 processkey[1]=0x40^0xFF;
                 break;
      case 0x1f: processkey[0]=0x01^0xFF; //"
                 processkey[1]=0x80^0xFF;
                 break;
      case 0x20: processkey[0]=0x01^0xFF; //£ (Generates # on MZ-80A)
                 processkey[2]=0x40^0xFF;
                 break;
      case 0x21: processkey[0]=0x01^0xFF; //$
                 processkey[2]=0x80^0xFF;
                 break;
      case 0x22: processkey[0]=0x01^0xFF; //%
                 processkey[3]=0x40^0xFF;
                 break;
      case 0x23: processkey[6]=0x80^0xFF; //^ (shifted 6 - ^ on USB kbd)
                 break;
      case 0x24: processkey[0]=0x01^0xFF; //&
                 processkey[3]=0x80^0xFF;
                 break;
      case 0x25: processkey[0]=0x01^0xFF; //*
                 processkey[6]=0x08^0xFF;
                 break;
      case 0x26: processkey[0]=0x01^0xFF; //(
                 processkey[4]=0x80^0xFF;
                 break;
      case 0x27: processkey[0]=0x01^0xFF; //)
                 processkey[5]=0x40^0xFF;
                 break;
      case 0x2d: processkey[0]=0x01^0xFF; //_
                 processkey[5]=0x80^0xFF;
                 break;
      case 0x2e: processkey[0]=0x01^0xFF; //+
                 processkey[6]=0x04^0xFF;
                 break;
      case 0x2f: processkey[0]=0x01^0xFF; //{
                 processkey[6]=0x20^0xFF;
                 break;
      case 0x30: processkey[0]=0x01^0xFF; //}
                 processkey[7]=0x04^0xFF;
                 break;
      case 0x32: processkey[0]=0x01^0xFF;  //~
                 processkey[6]=0x80^0xFF;  
                 break;
      case 0x33: processkey[6]=0x08^0xFF; //:
                 break;
      case 0x34: processkey[6]=0x10^0xFF; //@
                 break;
      case 0x36: processkey[0]=0x01^0xFF; //< 
                 processkey[5]=0x02^0xFF;
                 break;
      case 0x37: processkey[0]=0x01^0xFF; //>
                 processkey[6]=0x01^0xFF;
                 break;
      case 0x38: processkey[7]=0x01^0xFF; //?
                 break;

      /* Non-standard MZ-80K keys need to be somewhere ! */

      case 0x54: processkey[0]=0x01^0xFF; // <- (USB keypad shift /)
                 processkey[6]=0x02^0xFF;
                 break;

      case 0x64: processkey[0]=0x01^0xFF; //|
                 processkey[7]=0x40^0xFF;
                 break;
      default:   break;
    }
  }

  /* Ctrl keys  - CTRL key on MZ-80A keyboard mapped to USB Ctrls */
  if ((modifier == 0x01) || (modifier == 0x10)) {
    switch (usbk0) {

      case 0x04: processkey[0]=0x80^0xFF; //CTRL A - shift lock toggle
                 processkey[1]=0x08^0xFF; 
                 break;
      case 0x05: processkey[0]=0x80^0xFF; //CTRL B
                 processkey[3]=0x01^0xFF; 
                 break;
      case 0x06: processkey[0]=0x80^0xFF; //CTRL C
                 processkey[2]=0x01^0xFF; 
                 break;
      case 0x07: processkey[0]=0x80^0xFF; //CTRL D - display roll up
                 processkey[2]=0x08^0xFF; 
                 break;
      case 0x08: processkey[0]=0x80^0xFF; //CTRL E - display roll down
                 processkey[2]=0x10^0xFF;
                 break;
      case 0x09: processkey[0]=0x80^0xFF; //CTRL F
                 processkey[3]=0x04^0xFF;
                 break;
      case 0x0a: processkey[0]=0x80^0xFF; //CTRL G
                 processkey[3]=0x08^0xFF;
                 break;
      case 0x0b: processkey[0]=0x80^0xFF; //CTRL H
                 processkey[4]=0x04^0xFF;
                 break;
      case 0x0c: processkey[0]=0x80^0xFF; //CTRL I
                 processkey[4]=0x20^0xFF;
                 break;
      case 0x0d: processkey[0]=0x80^0xFF; //CTRL J
                 processkey[2]=0x08^0xFF;
                 break;
      case 0x0e: processkey[0]=0x80^0xFF; //CTRL K
                 processkey[5]=0x04^0xFF;
                 break;
      case 0x0f: processkey[0]=0x80^0xFF; //CTRL L
                 processkey[5]=0x08^0xFF;
                 break;
      case 0x10: processkey[0]=0x80^0xFF; //CTRL M
                 processkey[5]=0x01^0xFF;
                 break;
      case 0x11: processkey[0]=0x80^0xFF; //CTRL N
                 processkey[4]=0x02^0xFF;
                 break;
      case 0x12: processkey[0]=0x80^0xFF; //CTRL O
                 processkey[5]=0x10^0xFF;
                 break;
      case 0x13: processkey[0]=0x80^0xFF; //CTRL P
                 processkey[5]=0x20^0xFF;
                 break;
      case 0x14: processkey[0]=0x80^0xFF; //CTRL Q - cursor down
                 processkey[1]=0x10^0xFF; 
                 break;
      case 0x15: processkey[0]=0x80^0xFF; //CTRL R - cursor up
                 processkey[2]=0x20^0xFF;
                 break;
      case 0x16: processkey[0]=0x80^0xFF; //CTRL S - cursor right
                 processkey[2]=0x04^0xFF;
                 break;
      case 0x17: processkey[0]=0x80^0xFF; //CTRL T - cursor left
                 processkey[3]=0x10^0xFF;
                 break;
      case 0x18: processkey[0]=0x80^0xFF; //CTRL U - cursor home
                 processkey[4]=0x10^0xFF;
                 break;
      case 0x19: processkey[0]=0x80^0xFF; //CTRL V - cursor home & clear screen
                 processkey[3]=0x02^0xFF;
                 break;
      case 0x1a: processkey[0]=0x80^0xFF; //CTRL W 
                 processkey[1]=0x20^0xFF;
                 break;
      case 0x1b: processkey[0]=0x80^0xFF; //CTRL X 
                 processkey[2]=0x02^0xFF;
                 break;
      case 0x1c: processkey[0]=0x80^0xFF; //CTRL Y 
                 processkey[3]=0x20^0xFF;
                 break;
      case 0x1d: processkey[0]=0x80^0xFF; //CTRL Z - -> character
                 processkey[1]=0x01^0xFF;
                 break;
      case 0x2f: processkey[0]=0x80^0xFF; //CTRL [ - VRAM = 80K mode
                 processkey[6]=0x20^0xFF;
                 break;
      case 0x30: processkey[0]=0x80^0xFF; //CTRL ] - VRAM = 80A mode
                 processkey[7]=0x04^0xFF;
                 break;
      case 0x34: processkey[0]=0x80^0xFF; //CTRL @ (USB ctrl ')- reverse video
                 processkey[6]=0x10^0xFF;
                 break;

      default:   break;
    }
  }

  return;
}
