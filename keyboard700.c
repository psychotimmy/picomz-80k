/* Sharp MZ-700 emulator - keyboard */
/*    Tim Holyoake, October 2025    */

#include "picomz.h"

uint8_t processkey[KBDROWS];            // Return keyboard characters
                                        // All 0xFF means no key to process

static int16_t tfno=0;                  // Current tape file number
static bool tfwd=true;                  // Tape direction - true = forwards
                                        // false = backwards

#define MZ_KEY_REPEAT_INIT     500   // Key held for 500ms before 1st repeat
#define MZ_KEY_REPEAT_INTERVAL  85   // Subsequent key repeats every 85ms
                                     // Only applies to USB keyboard repeats -
                                     // how the MZ-700 handles this depends on
                                     // if the ROM monitor (no repeating keys)
                                     // or S-BASIC RAM monitor (repeating keys)
                                     // is active.

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

static bool alphashift;              // ALPHA key shifted status
static bool alpha_prev_rpt=false;    // Shift=lower case as per MZ-700 keyboard
static bool alpha_this_rpt=false;

static bool graphmode=false;         // Keep track of GRAPH mode activation
static bool resetalpha=false;        // true when moving from GRAPH to ALPHA

// Used to send a repeating key to the MZ-700
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
      // Send the repeating key to the MZ-700 and update next repeat time
      mzhidmapkey700(rptcode,rptmodifier);
      rpttime += MZ_KEY_REPEAT_INTERVAL;
    }

  }

  return;
}

// Need to do very little here as the USB keyboard codes are dealt with
// by mzhidmapkey700() rather than here
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

  // Did the status of the Caps Lock key change ?
  if (report->keycode[0] == 0x39)
    alpha_this_rpt=true;
  else
    alpha_this_rpt=false;

  if (alpha_this_rpt && !alpha_prev_rpt) {
    if (!graphmode) {
      alphashift = !alphashift;      // Toggle CAPS Lock key
                                     // alphashift == true == lower case (!)
      if (alphashift)                // Change CAPS Lock LED
        kleds_now &= ~KEYBOARD_LED_CAPSLOCK;
      else
        kleds_now |= KEYBOARD_LED_CAPSLOCK;
    }
    else {
      graphmode = false;
      resetalpha = true;             // Signal move from GRAPH to ALPHA
      alphashift = false;            // CAPS LOCK key reset to upper case
                                     // alphashift == true == lower case (!)
                                     // Change CAPS Lock LED
      kleds_now |= KEYBOARD_LED_CAPSLOCK;
    }
  }
   
  // Ignore anything less than 0x04 - no key, error conditions etc.
  if (report->keycode[0] > 0x03) {
    if (rptcode==0x00) {
      rptcode=report->keycode[0];   // Store new key for possible repeat
      rptmodifier=report->modifier; // Store new modifier for possible repeat
      rpttime=to_ms_since_boot(get_absolute_time())+MZ_KEY_REPEAT_INIT;   
                                    // Repeat key if initially held for 
                                    // MZ_KEY_REPEAT_INIT milliseconds
    }
    // We have a keypress to pass to the MZ-700
    mzhidmapkey700(report->keycode[0],report->modifier);
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

  // Set NUM LOCK and CAPS LOCK ON to start with on the MZ-700
  kleds_now=KEYBOARD_LED_NUMLOCK|KEYBOARD_LED_CAPSLOCK;
  tuh_hid_set_report(kaddr, kinst, 0, HID_REPORT_TYPE_OUTPUT, 
                     &kleds_now, sizeof(kleds_now));
  kleds_prev=kleds_now;
  numlock=true;
  alphashift=false;
  resetalpha=false;

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

/* Convert USB HID key press to the MZ-700 keyboard map,             */
/* then store in the processkey[] global (read on portB by the 8255) */
void mzhidmapkey700(uint8_t usbk0, uint8_t modifier) 
{
  int16_t tftemp;                         // Temporary tape file variable

  /* Unshifted USB keys */
  if (modifier == 0x00) {
    switch (usbk0) {

      case 0x04: processkey[4]=0x7F; //A  
                 break;
      case 0x05: processkey[4]=0xBF; //B  
                 break;
      case 0x06: processkey[4]=0xDF; //C  
                 break;
      case 0x07: processkey[4]=0xEF; //D
                 break;
      case 0x08: processkey[4]=0xF7; //E
                 break;
      case 0x09: processkey[4]=0xFB; //F
                 break;
      case 0x0a: processkey[4]=0xFD; //G
                 break;
      case 0x0b: processkey[4]=0xFE; //H
                 break;
      case 0x0c: processkey[3]=0x7F; //I
                 break;
      case 0x0d: processkey[3]=0xBF; //J
                 break;
      case 0x0e: processkey[3]=0xDF; //K
                 break;
      case 0x0f: processkey[3]=0xEF; //L
                 break;
      case 0x10: processkey[3]=0xF7; //M
                 break;
      case 0x11: processkey[3]=0xFB; //N
                 break;
      case 0x12: processkey[3]=0xFD; //O
                 break;
      case 0x13: processkey[3]=0xFE; //P
                 break;
      case 0x14: processkey[2]=0x7F; //Q
                 break;
      case 0x15: processkey[2]=0xBF; //R
                 break;
      case 0x16: processkey[2]=0xDF; //S
                 break;
      case 0x17: processkey[2]=0xEF; //T
                 break;
      case 0x18: processkey[2]=0xF7; //U
                 break;
      case 0x19: processkey[2]=0xFB; //V
                 break;
      case 0x1a: processkey[2]=0xFD; //W
                 break;
      case 0x1b: processkey[2]=0xFE; //X
                 break;
      case 0x1c: processkey[1]=0x7F; //Y
                 break;
      case 0x1d: processkey[1]=0xBF; //Z
                 break;

      case 0x1e: processkey[5]=0x7F; //1  
                 break;
      case 0x1f: processkey[5]=0xBF; //2  
                 break;
      case 0x20: processkey[5]=0xDF; //3  
                 break;
      case 0x21: processkey[5]=0xEF; //4  
                 break;
      case 0x22: processkey[5]=0xF7; //5  
                 break;
      case 0x23: processkey[5]=0xFB; //6  
                 break;
      case 0x24: processkey[5]=0xFD; //7  
                 break;
      case 0x25: processkey[5]=0xFE; //8  
                 break;
      case 0x26: processkey[6]=0xFB; //9  
                 break;
      case 0x27: processkey[6]=0xF7; //0  
                 break;

      case 0x28: processkey[0]=0xFE; //<CR>    (USB return key)
                 break;
      case 0x2a: processkey[7]=0xBF; //<DEL>   (USB backspace)
                 break;
      case 0x2b: processkey[0]=0xBF; //<GRAPH> (USB tab key)
                 graphmode=true;     //If true, CAPS LOCK always sets ALPHA
                 break;
      case 0x2c: processkey[6]=0xEF; //<SPACE>
                 break;
      case 0x2d: processkey[6]=0xDF; //-
                 break;
      case 0x2e: processkey[8]=0xFE; //=
                 processkey[6]=0xDF;
                 break;
      case 0x2f: processkey[1]=0xEF; //[
                 break;
      case 0x30: processkey[1]=0xF7; //]
                 break;
      case 0x32: processkey[8]=0xFE; //#
                 processkey[5]=0xDF;
                 break;
      case 0x33: processkey[0]=0xFB; //;
                 break;
      case 0x34: processkey[8]=0xFE; //'
                 processkey[5]=0xFD;
                 break;
      case 0x35: processkey[8]=0xFE; //`
                 processkey[1]=0xDF;
                 break;
      case 0x36: processkey[6]=0xFD; //,
                 break;
      case 0x37: processkey[6]=0xFE; //.
                 break;
      case 0x38: processkey[7]=0xFE; ///
                 break;
      case 0x39: // CAPS LOCK = TOGGLE ALPHA / SHIFT ALPHA unless exiting GRAPH
                 if ((!graphmode) && (!resetalpha))
                   processkey[8]=0xFE;
                 resetalpha=false;   // Always set false when CAPS LOCK pressed
                 processkey[0]=0xEF;
                 break;

      case 0x3a: //F1 - Not mapped to a MZ-700 key
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
      case 0x3b: //F2 - Not mapped to a MZ-700 key
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
      case 0x3c: //F3 - Not mapped to a MZ-700 key
                 mzspinny(0);             // Reset tape counter
                 break;
      case 0x3d: //F4 - Not mapped to a MZ-700 key
                 memset(mzemustatus,0x00,EMUSSIZE); // Clear status area
                 break;

      // USB F5 - F9 are mapped as MZ-700 F1 to F5, 
      // USB shifted F5 - F9 are mapped as MZ-700 shifted F1 - F5
      case 0x3e: //USB F5 = MZ-700 F1
                 processkey[9]=0x7F;
                 break;
      case 0x3f: //USB F6 = MZ-700 F2
                 processkey[9]=0xBF;
                 break;
      case 0x40: //USB F7 = MZ-700 F3
                 processkey[9]=0xDF;
                 break;
      case 0x41: //USB F8 = MZ-700 F4
                 processkey[9]=0xEF;
                 break;
      case 0x42: //USB F9 = MZ-700 F5
                 processkey[9]=0xF7;
                 break;

      case 0x43: mzcpu.pc=0x0000;         //F10 - MZ-700 reset button
                 // Reset only resets the program counter to 0x0000
                 // Ctrl reset is required to reset banked memory as well
                 // ... the equivalent of a power off / power on.
                 reset_tape(); 
                 break;
      case 0x44: mzreaddump();            //F11 - read memory dump
                 break;
      case 0x45: mzsavedump();            //F12 - save memory dump
                 break;

      case 0x49: processkey[7]=0x7F; //<INS>  (USB Insert)
                 break;
      case 0x4a: processkey[8]=0xFE; //<HOME> (USB Home)
                 processkey[7]=0xBF;
                 break;
      case 0x4b: processkey[8]=0x7E; //Shift BREAK (USB PgUp)
                 // Shift break always resets the cassette deck states 
                 reset_tape(); 
                 break;
      case 0x4c: processkey[7]=0xBF; //<DEL> (USB Delete forward)
                 break;
      case 0x4d: processkey[8]=0xFE; //<CLR> (USB End)
                 processkey[7]=0x7F;
                 break;
      case 0x4e: processkey[8]=0x7F; //BREAK (USB PgDn)
                 break;
      case 0x4f: processkey[7]=0xF7; //cursor right
                 break;
      case 0x50: processkey[7]=0xFB; //cursor left
                 break;
      case 0x51: processkey[7]=0xEF; //cursor down
                 break;
      case 0x52: processkey[7]=0xDF; //cursor up
                 break;

      // 0x53 - NUM LOCK -  is dealt with before this function is called

      case 0x54: processkey[6]=0xBF; //up arrow (USB keypad /)
                 break;
      case 0x55: processkey[8]=0xFE; //*
                 processkey[0]=0xFD;
                 break;
      case 0x56: processkey[6]=0xDF; //-
                 break;
      case 0x57: processkey[8]=0xFE; //+
                 processkey[0]=0xFB;
                 break;
      case 0x58: processkey[0]=0xFE; //<CR>  (USB keypad Enter)
                 break;

      case 0x59: if (numlock) 
                   processkey[5]=0x7F; //1  
                 else {
                   processkey[8]=0xFE; //<CLR> (USB End)
                   processkey[7]=0x7F;
                 }
                 break;
      case 0x5a: if (numlock) 
                   processkey[5]=0xBF; //2  
                 else 
                   processkey[7]=0xEF; //cursor down
                 break;
      case 0x5b: if (numlock) 
                   processkey[5]=0xDF; //3  
                 else
                   processkey[8]=0x7F; //BREAK (USB PgDn)
                 break;
      case 0x5c: if (numlock) 
                   processkey[5]=0xEF; //4  
                 else 
                   processkey[7]=0xFB; //cursor left
                 break;
      case 0x5d: if (numlock) 
                   processkey[5]=0xF7; //5  
                 break;
      case 0x5e: if (numlock) 
                   processkey[5]=0xFB; //6  
                 else 
                   processkey[7]=0xF7; //cursor right
                 break;
      case 0x5f: if (numlock) 
                   processkey[5]=0xFD; //7  
                 else {
                   processkey[8]=0xFE; //<HOME> (USB Home)
                   processkey[7]=0xBF;
                 }
                 break;
      case 0x60: if (numlock) 
                   processkey[5]=0xFE; //8  
                 else
                   processkey[7]=0xDF; //cursor up
                 break;
      case 0x61: if (numlock) 
                   processkey[6]=0xFB; //9  
                 else {
                   processkey[8]=0x7E; //Shift BREAK (USB PgUp)
                   // Shift break always resets the cassette deck states
                   reset_tape();
                 }
                 break;
      case 0x62: if (numlock) 
                   processkey[6]=0xF7; //0  
                 else
                   processkey[7]=0x7F; //insert (INS)
                 break;
      case 0x63: if (numlock) 
                   processkey[6]=0xFE; //.
                 else
                   processkey[7]=0xBF; //delete (DEL)
                 break;

      case 0x64: processkey[6]=0x7F; //backslash (non US USB key 102)
                 break;

      default:   break;                   // Ignore unmapped keys

    }
  }

  /* Shifted USB keys */
  if ((modifier == 0x02) || (modifier == 0x20)) {
    switch (usbk0) {

      case 0x04: processkey[8]=0xFE; //a
                 processkey[4]=0x7F;
                 break;
      case 0x05: processkey[8]=0xFE; //b
                 processkey[4]=0xBF;
                 break;
      case 0x06: processkey[8]=0xFE; //c
                 processkey[4]=0xDF;
                 break;
      case 0x07: processkey[8]=0xFE; //d
                 processkey[4]=0xEF;
                 break;
      case 0x08: processkey[8]=0xFE; //e
                 processkey[4]=0xF7;
                 break;
      case 0x09: processkey[8]=0xFE; //f
                 processkey[4]=0xFB;
                 break;
      case 0x0a: processkey[8]=0xFE; //g
                 processkey[4]=0xFD;
                 break;
      case 0x0b: processkey[8]=0xFE; //h
                 processkey[4]=0xFE;
                 break;
      case 0x0c: processkey[8]=0xFE; //i
                 processkey[3]=0x7F;
                 break;
      case 0x0d: processkey[8]=0xFE; //j
                 processkey[3]=0xBF;
                 break;
      case 0x0e: processkey[8]=0xFE; //k
                 processkey[3]=0xDF;
                 break;
      case 0x0f: processkey[8]=0xFE; //l
                 processkey[3]=0xEF;
                 break;
      case 0x10: processkey[8]=0xFE; //m
                 processkey[3]=0xF7;
                 break;
      case 0x11: processkey[8]=0xFE; //n
                 processkey[3]=0xFB;
                 break;
      case 0x12: processkey[8]=0xFE; //o
                 processkey[3]=0xFD;
                 break;
      case 0x13: processkey[8]=0xFE; //p
                 processkey[3]=0xFE;
                 break;
      case 0x14: processkey[8]=0xFE; //q
                 processkey[2]=0x7F;
                 break;
      case 0x15: processkey[8]=0xFE; //r
                 processkey[2]=0xBF;
                 break;
      case 0x16: processkey[8]=0xFE; //s
                 processkey[2]=0xDF;
                 break;
      case 0x17: processkey[8]=0xFE; //t
                 processkey[2]=0xEF;
                 break;
      case 0x18: processkey[8]=0xFE; //u
                 processkey[2]=0xF7;
                 break;
      case 0x19: processkey[8]=0xFE; //v
                 processkey[2]=0xFB;
                 break;
      case 0x1a: processkey[8]=0xFE; //w
                 processkey[2]=0xFD;
                 break;
      case 0x1b: processkey[8]=0xFE; //x
                 processkey[2]=0xFE;
                 break;
      case 0x1c: processkey[8]=0xFE; //y
                 processkey[1]=0x7F;
                 break;
      case 0x1d: processkey[8]=0xFE; //z
                 processkey[1]=0xBF;
                 break;
      case 0x1e: processkey[8]=0xFE; //!
                 processkey[5]=0x7F;
                 break;
      case 0x1f: processkey[8]=0xFE; //"
                 processkey[5]=0xBF;
                 break;
      case 0x20: processkey[8]=0xFE; //£ 
                 processkey[0]=0xDF;
                 break;
      case 0x21: processkey[8]=0xFE; //$
                 processkey[5]=0xEF;
                 break;
      case 0x22: processkey[8]=0xFE; //%
                 processkey[5]=0xF7;
                 break;
      case 0x23: processkey[6]=0xBF; //up arrow (shifted 6 - ^ on USB kbd)
                 break;
      case 0x24: processkey[8]=0xFE; //&
                 processkey[5]=0xFD;
                 break;
      case 0x25: processkey[8]=0xFE; //*
                 processkey[0]=0xFD;
                 break;
      case 0x26: processkey[8]=0xFE; //(
                 processkey[5]=0xFE;
                 break;
      case 0x27: processkey[8]=0xFE; //)
                 processkey[6]=0xFB;
                 break;
      case 0x2d: processkey[8]=0xFE; //(USB underscore _ = MZ700 pi)
                 processkey[6]=0xF7;
                 break;
      case 0x2e: processkey[8]=0xFE; //+
                 processkey[0]=0xFB;
                 break;
      case 0x2f: processkey[8]=0xFE; //{
                 processkey[1]=0xEF;
                 break;
      case 0x30: processkey[8]=0xFE; //}
                 processkey[1]=0xF7;
                 break;
      case 0x32: processkey[8]=0xFE;  //~
                 processkey[6]=0xBF;  
                 break;
      case 0x33: processkey[0]=0xFD; //:
                 break;
      case 0x34: processkey[1]=0xDF; //@
                 break;
      case 0x35: processkey[0]=0xDF; //down arrow
                 break;
      case 0x36: processkey[8]=0xFE; //< 
                 processkey[6]=0xFD;
                 break;
      case 0x37: processkey[8]=0xFE; //>
                 processkey[6]=0xFE;
                 break;
      case 0x38: processkey[7]=0xFD; //?
                 break;

      case 0x3e: //USB shift F5 = MZ-700 shift F1
                 processkey[8]=0xFE;
                 processkey[9]=0x7F;
                 break;
      case 0x3f: //USB shift F6 = MZ-700 shift F2
                 processkey[8]=0xFE;
                 processkey[9]=0xBF;
                 break;
      case 0x40: //USB shift F7 = MZ-700 shift F3
                 processkey[8]=0xFE;
                 processkey[9]=0xDF;
                 break;
      case 0x41: //USB shift F8 = MZ-700 shift F4
                 processkey[8]=0xFE;
                 processkey[9]=0xEF;
                 break;
      case 0x42: //USB shift F9 = MZ-700 shift F5
                 processkey[8]=0xFE;
                 processkey[9]=0xF7;
                 break;

      case 0x51: processkey[8]=0xFE; //shifted cursor down
                 processkey[7]=0xEF; 
                 break;
      case 0x52: processkey[8]=0xFE; //shifted cursor up
                 processkey[7]=0xDF;
                 break;

      case 0x54: processkey[8]=0xFE; // <- (USB keypad shift /)
                 processkey[7]=0xFE;
                 break;
      case 0x55: processkey[8]=0xFE; // -> (USB keypad shift *)
                 processkey[7]=0xFD;

      case 0x64: processkey[8]=0xFE; // | (USB pipe symbol |, shift \)
                 processkey[6]=0x7F;
                 break;
      default:   break;
    }
  }

  /* Ctrl keys  - CTRL key on MZ-700 keyboard mapped to USB Ctrls */
  if ((modifier == 0x01) || (modifier == 0x10)) {
    switch (usbk0) {

      case 0x04: processkey[8]=0xBF; //CTRL A
                 processkey[4]=0xF7; 
                 break;
      case 0x05: processkey[8]=0xBF; //CTRL B
                 processkey[4]=0xBF; 
                 break;
      case 0x06: processkey[8]=0xBF; //CTRL C
                 processkey[4]=0xDF; 
                 break;
      case 0x07: processkey[8]=0xBF; //CTRL D
                 processkey[4]=0xEF; 
                 break;
      case 0x08: processkey[8]=0xBF; //CTRL E - keyboard to lower case
                 processkey[4]=0xF7;
                 break;
      case 0x09: processkey[8]=0xBF; //CTRL F - keybaord to upper case
                 processkey[4]=0xFB;
                 break;
      case 0x0a: processkey[8]=0xBF; //CTRL G
                 processkey[4]=0xFD;
                 break;
      case 0x0b: processkey[8]=0xBF; //CTRL H
                 processkey[4]=0xFE;
                 break;
      case 0x0c: processkey[8]=0xBF; //CTRL I
                 processkey[3]=0x7F;
                 break;
      case 0x0d: processkey[8]=0xBF; //CTRL J
                 processkey[3]=0xBF;
                 break;
      case 0x0e: processkey[8]=0xBF; //CTRL K
                 processkey[3]=0xDF;
                 break;
      case 0x0f: processkey[8]=0xBF; //CTRL L
                 processkey[3]=0xEF;
                 break;
      case 0x10: processkey[8]=0xBF; //CTRL M - <CR>
                 processkey[3]=0xF7;
                 break;
      case 0x11: processkey[8]=0xBF; //CTRL N
                 processkey[3]=0xFB;
                 break;
      case 0x12: processkey[8]=0xBF; //CTRL O
                 processkey[3]=0xFD;
                 break;
      case 0x13: processkey[8]=0xBF; //CTRL P - <DEL>
                 processkey[3]=0xFE;
                 break;
      case 0x14: processkey[8]=0xBF; //CTRL Q - cursor down
                 processkey[2]=0x7F; 
                 break;
      case 0x15: processkey[8]=0xBF; //CTRL R - cursor up
                 processkey[2]=0xBF;
                 break;
      case 0x16: processkey[8]=0xBF; //CTRL S - cursor left
                 processkey[2]=0xDF;
                 break;
      case 0x17: processkey[8]=0xBF; //CTRL T - cursor right
                 processkey[2]=0xEF;
                 break;
      case 0x18: processkey[8]=0xBF; //CTRL U - cursor home
                 processkey[2]=0xF7;
                 break;
      case 0x19: processkey[8]=0xBF; //CTRL V - <CLR>
                 processkey[2]=0xFB;
                 break;
      case 0x1a: processkey[8]=0xBF; //CTRL W - <GRAPH>
                 processkey[2]=0xFD;
                 graphmode=true;     //If true, CAPS LOCK always sets ALPHA
                 break;
      case 0x1b: processkey[8]=0xBF; //CTRL X - <INST>
                 processkey[2]=0xFE;
                 break;
      case 0x1c: processkey[8]=0xBF; //CTRL Y - <ALPHA>
                 processkey[1]=0x7F;
                 break;
      case 0x1d: processkey[8]=0xBF; //CTRL Z
                 processkey[1]=0xBF;
                 break;
      case 0x43: mzcpu.pc=0x0000;         //F10 - MZ-700 reset button
                 // Reset only resets the program counter to 0x0000
                 // Ctrl reset is required to reset banked memory as well
                 // ... the equivalent of a power off / power on.
                 z80* unusedz;
                 sio_write(unusedz,0xE4,0);
                 reset_tape(); 
                 break;

      default:   break;
    }
  }

  return;
}
