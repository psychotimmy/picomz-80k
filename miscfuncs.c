/* Sharp MZ-80K emulator                  */
/* Status and miscellaneous functions     */
/* Tim Holyoake, November - December 2024 */

#include "picomz.h"

/* Turn the LED on the pico on or off */
void mzpicoled(uint8_t state)
{
  // state == 1 is on, 0 is off.
  gpio_put(PICO_DEFAULT_LED_PIN, state);
  return;
}

/* Convert a standard ASCII string to MZ display string. */
/* Deals with A-Z, a-z, 0-9, space plus some symbols.    */
/* Unrecognised ASCII codes are returned as a space.     */
void ascii2mzdisplay(uint8_t* convert, uint8_t* converted)
{
  uint8_t i;

  // Set converted to mz display code spaces - 0x00
  memset(converted,0x00,strlen(convert)); 

  for (i=0; i<strlen(convert); i++) {

    /* Deal with scattered codes first - 0x20 (space) already set to 0x00 */
    switch(convert[i]) {

      case 0x2a: converted[i]=0x6b;               // *
                 break;
      case 0x2b: converted[i]=0x6a;               // +
                 break;
      case 0x2c: converted[i]=0x2f;               // ,
                 break;
      case 0x2d: converted[i]=0x2a;               // -
                 break;
      case 0x2e: converted[i]=0x2e;               // .
                 break;
      case 0x2f: converted[i]=0x2d;               // /
                 break;

      case 0x3a: converted[i]=0x4f;               // :
                 break;
      case 0x3b: converted[i]=0x2c;               // ;
                 break;
      case 0x3c: converted[i]=0x51;               // <
                 break;
      case 0x3d: converted[i]=0x2b;               // =
                 break;
      case 0x3e: converted[i]=0x57;               // >
                 break;
      case 0x3f: converted[i]=0x49;               // ?
                 break;
      case 0x40: converted[i]=0x55;               // @
                 break;

      case 0x5b: converted[i]=0x52;              // [
                 break;
      case 0x5c: converted[i]=0x59;              // \
                 break;
      case 0x5d: converted[i]=0x54;              // ]
                 break;

      case 0xa3: converted[i]=0x1b;              // £
                 break;
      case 0xa5: converted[i]=0xbc;              // Yen
                 break;

    }

    /* Now deal with contiguous sequences of codes */

    if ((convert[i] >= 0x61) && (convert[i] <= 0x7a))
      converted[i]=convert[i]+0x20;               // a - z

    if ((convert[i] >= 0x41) && (convert[i] <= 0x5a))
      converted[i]=convert[i]-0x40;               // A - Z

    if ((convert[i] >= 0x30) && (convert[i] <= 0x39))
      converted[i]=convert[i]-0x10;               // 0 - 9

    if ((convert[i] >= 0x21) && (convert[i] <= 0x29))
      converted[i]=convert[i]+0x40;               // ! " # $ % & ' ( ) 

  }

  return;
}

/* Convert a Sharp 'ASCII' tape file name character to an 'ASCII'   */
/* character that will form part of a legal FAT (sd card) file name */
/* Currently incomplete, but good enough for most purposes.         */
uint8_t mzsafefilechar(uint8_t sharpchar)
{
  uint8_t asciichar=0x2d; /* Default anything not in switch and ifs to dash */

  /* Sharp lower case letters are all ok */
  /* but are not contiguous ... convert  */
  switch(sharpchar) {

    case 0xa1: asciichar=0x61; //a
               break;
    case 0x9a: asciichar=0x62; //b
               break;
    case 0x9f: asciichar=0x63; //c
               break;
    case 0x9c: asciichar=0x64; //d
               break;
    case 0x92: asciichar=0x65; //e
               break;
    case 0xaa: asciichar=0x66; //f
               break;
    case 0x97: asciichar=0x67; //g
               break;
    case 0x98: asciichar=0x68; //h
               break;
    case 0xa6: asciichar=0x69; //i
               break;
    case 0xaf: asciichar=0x6a; //j
               break;
    case 0xa9: asciichar=0x6b; //k
               break;
    case 0xb8: asciichar=0x6c; //l
               break;
    case 0xb3: asciichar=0x6d; //m
               break;
    case 0xb0: asciichar=0x6e; //n
               break;
    case 0xb7: asciichar=0x6f; //o
               break;
    case 0x9e: asciichar=0x70; //p
               break;
    case 0xa0: asciichar=0x71; //q
               break;
    case 0x9d: asciichar=0x72; //r
               break;
    case 0xa4: asciichar=0x73; //s
               break;
    case 0x96: asciichar=0x74; //t
               break;
    case 0xa5: asciichar=0x75; //u
               break;
    case 0xab: asciichar=0x76; //v
               break;
    case 0xa3: asciichar=0x77; //w
               break;
    case 0x9b: asciichar=0x78; //x
               break;
    case 0xbd: asciichar=0x79; //y
               break;
    case 0xa2: asciichar=0x7a; //z
               break;
  }

  /* Sharp upper case letters are all ok */
  if ((sharpchar >= 0x41) && (sharpchar <= 0x5a))
    asciichar=sharpchar;

  /* Sharp numbers are all ok */
  if ((sharpchar >= 0x30) && (sharpchar <= 0x39)) 
    asciichar=sharpchar;
  
  return(asciichar);
}

/* Convert a Sharp 'ASCII' character to a display character */
/* Incomplete, but good enough for version 1!               */
uint8_t mzascii2mzdisplay(uint8_t ascii)
{
  uint8_t displaychar = 0x00;            // space returned for anything not
                                         // in the switch and ifs below

  switch(ascii) {

    case 0x2a: displaychar=0x6b;   //*
               break;
    case 0x2b: displaychar=0x6a;   //+
               break;
    case 0x2c: displaychar=0x2f;   //,
               break;
    case 0x2d: displaychar=0x2a;   //-
               break;
    case 0x2e: displaychar=0x2e;   //.
               break;
    case 0x2f: displaychar=0x2d;   ///
               break;

    case 0x3a: displaychar=0x4f;   //:
               break;
    case 0x3b: displaychar=0x2c;   //;
               break;
    case 0x3c: displaychar=0x51;   //<
               break;
    case 0x3d: displaychar=0x2b;   //=
               break;
    case 0x3e: displaychar=0x57;   //>
               break;
    case 0x3f: displaychar=0x49;   //?
               break;
    case 0x40: displaychar=0x55;   //@
               break;

    case 0x5b: displaychar=0x52;   //[
               break;
    case 0x5c: displaychar=0x59;   //\
               break;
    case 0x5d: displaychar=0x54;   //]
               break;
    case 0x6c: displaychar=0x5a;   // right arrow
               break;
    case 0x92: displaychar=0x85;   //e
               break;
    case 0x96: displaychar=0x94;   //t
               break;
    case 0x97: displaychar=0x87;   //g
               break;
    case 0x98: displaychar=0x88;   //h
               break;
    case 0x9a: displaychar=0x82;   //b
               break;
    case 0x9b: displaychar=0x98;   //x
               break;
    case 0x9c: displaychar=0x84;   //d
               break;
    case 0x9d: displaychar=0x92;   //r
               break;
    case 0x9e: displaychar=0x90;   //p
               break;
    case 0x9f: displaychar=0x83;   //c
               break;
    case 0xa0: displaychar=0x91;   //q
               break;
    case 0xa1: displaychar=0x81;   //a
               break;
    case 0xa2: displaychar=0x9a;   //z
               break;
    case 0xa3: displaychar=0x97;   //w
               break;
    case 0xa4: displaychar=0x93;   //s
               break;
    case 0xa5: displaychar=0x95;   //u
               break;
    case 0xa6: displaychar=0x89;   //i
               break;
    case 0xa9: displaychar=0x8b;   //k
               break;
    case 0xaa: displaychar=0x86;   //f
               break;
    case 0xab: displaychar=0x96;   //v
               break;
    case 0xaf: displaychar=0x8a;   //j
               break;
    case 0xb0: displaychar=0x8e;   //n
               break;
    case 0xb3: displaychar=0x8d;   //m
               break;
    case 0xb7: displaychar=0x8f;   //o
               break;
    case 0xb8: displaychar=0x8c;   //l
               break;
    case 0xbd: displaychar=0x99;   //y
               break;
    case 0xe1: displaychar=0x41;   //spade
               break;
    case 0xf3: displaychar=0x53;   //heart
               break;
    case 0xf8: displaychar=0x46;   //club
               break;
    case 0xfa: displaychar=0x44;   //diamond
               break;
    case 0xff: displaychar=0x60;   //pi
               break;
  }

  if ((ascii >= 0x41) && (ascii <= 0x5a))
    displaychar=ascii-0x40;        // A - Z

  if ((ascii >= 0x30) && (ascii <= 0x39))
    displaychar=ascii-0x10;        // 0 - 9

  if ((ascii >= 0x21) && (ascii <= 0x29))
    displaychar=ascii+0x40;        // ! " # $ % & ' ( ) 

  return(displaychar);
}
