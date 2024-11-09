/* Sharp MZ-80K 8255 implementation  */
/* Tim Holyoake, August-October 2024 */

#include "picomz.h"

// 8255 address to port mapping for the MZ-80K 
// The ports are all 8 bits wide 
// Note that the MZ-80K only uses the 8255 in mode 0,
// so this simplifies the implementation somewhat.

uint8_t portA;                  /* 0xE000 - port A */
                                /* 0xE001 - port B */
uint8_t portC;                  /* 0xE002 - port C - provides two 4 bit ports */
                                /* 0xE003 - Control port */

uint8_t cmotor=1;               /* Cassette motor off (0) or on (1) */
                                /* Toggled to 0 during MZ-80K startup */
uint8_t csense=1;               /* Cassette sense toggle */
                                /* Toggled to 0 during MZ-80K startup */
                                /* Note - the emulator currently ties the */
                                /* cmotor & csense signals together. A more */
                                /* faithful version would have */
                                /* separate buttons for  a virtual cassette */
                                /* deck, so motor and sense are not always */
                                /* the same. */
uint8_t vblank=0;               /* /VBLANK signal */
uint8_t vgate;                  /* /VGATE signal */

uint8_t scantimes=1;            /* How many times the keyboard matrix is */
                                /* scanned before it is reset. Most programs */
                                /* work well on the default; some m/c games */
                                /* need this set to 2 or 3 for keypresses to */
                                /* be read effectively. A kludge, but the */
                                /* best option at the moment - better than */
                                /* releases 1.0.0 or 1.0.1 as more flexible */
                                /* F9 key rotates between 1 and 3, 1 is the */
                                /* default on startup. */

static uint8_t cblink=0;        /* Cursor blink (0 = off, 1 = on) */
static uint8_t c555=0;          /* Pseudo 555 timer  for cursor blink */

// The control port is implemented as follows:
// 
// Bit 0 - Port C lower 4 bits - 1=input, 0=output
// Bit 1 - Port B              - 1=input, 0=output
// Bit 2 - 8255 mode select    - 1=mode 1, 0=mode 0
// Bit 3 - Port C upper 4 bits - 1=input, 0=output
// Bit 4 - Port A              - 1=input, 0=output
// Bit 5 - Mode select         - 0=mode 0, 0=mode 1, 1=mode 2
// Bit 6 -    -"-              - 0         1         1
// Bit 7 - Mode/port C bit flag- 1=mode set active, 0=bit set active

// The SP-1002 monitor writes the value 0x8A to the control port
// on startup - 1001010 binary. This sets portC lower as output,
// portC upper as input, portB as input and portA as output.

// The control port is also used to change individual bits in portC.
// Bit 7 needs to be 0 for this mode to be used, bits 4,5,6 are not used.
// Bit 0 determines if the command is a bit set (1) or reset (0) operation.
// Bits 3,2,1 determine which bit in portC is affected.
//
// PortC bit              0   1   2   3   4   5   6   7 selected
//                        -   -   -   -   -   -   -   -
// Control port bit 3     0   1   0   1   0   1   0   1
// Control port bit 2     0   0   1   1   0   0   1   1
// Control port bit 1     0   0   0   0   1   1   1   1

// Port A and B notes

// Bits 0-3 of portA are connected to a BCD to decimal decoder,
// with the outputs connected to the strobe lines of the keyboard
// matrix. So setting the bits on portA between 0x00-0x10 to 1 allows
// each column to be examined, with the key pressed being recorded by
// portB. Note that a high value in a bit on portB means the key is NOT
// pressed, 0 means that it has been.
//
// Bits 4-6 of portA are unused. If bit 7 is 1, the cursor flash timer
// is reset.

// Port C notes

// Port C upper 4 bits (4-7) - inputs
// Bit 4 - Cassette motor (0 = off, 1 = on)
// Bit 5 - Data input from the cassette read line
// Bit 6 - Timing pulse for the cursor flash timer
// Bit 7 - Detects /VBLANK (1- vblank period, 0 otherwise)

// Port C lower 4 bits (0-3) - outputs
// Bit 0 - /VGATE (1- blank the screen)
// Bit 1 - Write this bit to the cassette tape 
// Bit 2 - Toggle SML/CAP LED red (1) / green (0)
// Bit 3 - Cassette sense (0 = on, 1 = off)

void wr8255(uint16_t addr, uint8_t data)
{
  switch (addr&0x0003) {             // addr is between 0xE000 and 0xE003
    case 0:// Write to portA
           if ((data&0x80) && (++c555 > 50)) {
             c555=0;                 // A simple 555 timer emulation 
             ++cblink;               // Bit 7 controls cursor blink
           } 
                                     // Bits 0-3 are used by keyboard
           portA=data;               // Keeps state in portA global
           break;
    case 1:// Write to portB - this should never happen on an MZ-80K,
           // so nothing should change if a program tries to do it.
           break;
    case 2:// Overwrite the lower 4 bits of port C.
           // This is allowed, but normally the control port is used
           // to affect 1 bit at a time.
           SHOW("!! addressing portC directly with 0x%02x !!\n",data);
           portC = (portC&0xF0)|(data&0x0F);
           break;
    case 3:// Control port code
           // If mode set is chosen, do nothing as the MZ-80K must never
           // change the way the 8255 ports are configured after the 
           // monitor issues it with 8A on start up.
           uint8_t portCbit, setbit;
           if (!(data&0x80)) {  // Bit 7 is 0, so the 8255 is performing bit ops
           setbit = data&0x01;    // Bit 0 indicates set(1)/reset(0)
           portCbit = (data>>1)&0x07; // Mask out everything except bits 1-3
                                      // Shift right 1 bit to make switch
                                      // statement simpler
           if (portCbit != 1) SHOW("Setbit %d portCbit %d\n",setbit,portCbit);
           switch (portCbit) {
             case 0: // /VGATE
                     if (setbit) {
                       portC|=0x01;
                       vgate=0;      // Signal to unblank screen
                     }
                     else {
                       portC&=0xFE;
                       vgate=1;      // Signal to blank screen
                     }
                     break;
             case 1: // Bit to write to cassette tape when
                     // motor and sense are on
                     if (setbit) portC|=0x02;
                     else portC&=0xFD;
                     if (csense && cmotor) cwrite(setbit);
                     break;
             case 2: // SML/CAP toggle
                     if (setbit) portC|=0x04;
                     else portC&=0xFB;
                     break;
             case 3: // Cassette sense
                     if (setbit) { 
                       portC|=0x08;
                       csense=!csense;   /* Toggle sense when bit set */
                       cmotor=!cmotor;   /* Toggle motor when bit set */
                       SHOW("motor %d sense %d\n",cmotor,csense);
                     }
                     else { 
                       portC&=0xF7;      /* sense & motor remain as before */
                     }
                     break;
                  /* Should never get to cases 4-7, so break */
             default:SHOW("Unexpected portC bit set attempt (%d)\n",portCbit);
                     break;
           }
      } // As noted, mustn't do anything if bit 7 is 1 (mode set)
    break;

    default:// Error!
            SHOW("Error: illegal address passed to wr8255 0x%04x\n",addr);
            break;
  }
  return;
}

uint8_t rd8255(uint16_t addr)
{
  static uint8_t newkey[KBDROWS] = { 0xFF,0xFF,0xFF,0xFF,0xFF,
                                     0xFF,0xFF,0xFF,0xFF,0xFF };
  static uint8_t idxloop=0;        /* Allows some m/c code games to work */
                                   /* by keeping the press from the key  */
                                   /* matrix active for scantimes cycles */
  uint8_t idx,retval;

  switch (addr&0x0003) {      // addr is between 0xE000 and 0xE002
    case 0:// Read from portA global
           retval=portA;
           break;
    case 1:// Port B is keyboard input
           idx=portA&0x0F;
           // 10 lines (KBDROWS) to strobe, so idx must be between 0 and 9
           if (idx < KBDROWS) {

             /* If we're at the top of the scan refresh newkey from  */
             /* the processkey global */
             if (idx == 9)
               memcpy(newkey,processkey,KBDROWS);

             /* Return the current row of the keyboard matrix */
             retval=newkey[idx];

             /* Increment the number of scans if on last row of matrix */
             if (idx == 0) 
               ++idxloop;

             /* Enough scans done, clear processkey for next key event */
             if ((idx == 9) && (idxloop >= scantimes)) {
               memset(processkey,0xFF,KBDROWS);
               idxloop=0;
             }

           }
           else {
             retval=0xFF;                // 0xFF always returned if idx > 9
           }
           break;
    case 2:// Read upper 4 bits from portC 
           retval=portC&0x0F;          // Lower 4 bits returned unchanged
           retval|=(cmotor?0x10:0x00); // Cassette motor (off=0x00,on=0x10)
           retval|=(cread()?0x20:0x00);// Next bit read from tape  (1=0x20)
                                       //                          (0=0x00)
           retval|=((cblink>0x7F)?0x40:0x00); // Blink cursor
           retval|=(vblank?0x80:0x00);        // /V-BLANK status
           break;
    default:// Error!
           retval=0xC7;
           SHOW("Error: illegal address passed to rd8255 0x%04x\n",addr);
           break;
  }
  return(retval);
}
