/* Sharp MZ-80K emulator                             */
/* Status LED and miscellaneous conversion functions */
/* Tim Holyoake, November 2024 - January 2025        */

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
  // Set converted to mz display code spaces - 0x00
  memset(converted,0x00,strlen(convert)); 

  for (uint8_t i=0; i<strlen(convert); i++) {

    /* Deal with contiguous sequences of codes first */

    if ((convert[i] >= 0x21) && (convert[i] <= 0x29)) {
      converted[i]=convert[i]+0x40;               // ! " # $ % & ' ( ) 
    }
    else if ((convert[i] >= 0x30) && (convert[i] <= 0x39)) {
      converted[i]=convert[i]-0x10;               // 0 - 9
    }
    else if ((convert[i] >= 0x41) && (convert[i] <= 0x5a)) {
      converted[i]=convert[i]-0x40;               // A - Z
    }
    else if ((convert[i] >= 0x61) && (convert[i] <= 0x7a)) {
      converted[i]=convert[i]+0x20;               // a - z
    }
    else {

      /* Deal with scattered codes - 0x20 (space) already set to 0x00 */
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
    }
  }

  return;
}

/* Convert a Sharp 'ASCII' tape file name character to an 'ASCII'   */
/* character that will form part of a legal FAT (sd card) file name */
/* Incomplete coverage, but good enough for most purposes.          */
uint8_t mzsafefilechar(uint8_t sharpchar)
{
  uint8_t asciichar=0x2d; /* Default anything not in if or switch to a dash */

  /* # $ % & ' ( ) 0-9 and A-Z are ok, contiguous & have a true ASCII value */
  if (((sharpchar >= 0x23) && (sharpchar <= 0x29)) ||
      ((sharpchar >= 0x30) && (sharpchar <= 0x39)) ||
      ((sharpchar >= 0x41) && (sharpchar <= 0x5a))) 
    asciichar=sharpchar;
  else
    switch(sharpchar) {

      /* Sharp lower case letters are all ok */
      /* but are not contiguous ... convert  */

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

      /* ! and @ are also ok */

      case 0x21: asciichar=0x21; // !
                 break;
      case 0x40: asciichar=0x40; // @
                 break;

      /* German characters are ok */
  
      case 0xa8: asciichar=0x99; // O+umlaut
                 break;
      case 0xad: asciichar=0x81; // u+umlaut
                 break;
      case 0xae: asciichar=0xe1; // eszett
                 break;
      case 0xb2: asciichar=0x9a; // U+umlaut
                 break;
      case 0xb9: asciichar=0x8e; // A+umlaut
                 break;
      case 0xba: asciichar=0x94; // o+umlaut
                 break;
      case 0xbb: asciichar=0x84; // a+umlaut
                 break;
    }

  return(asciichar);
}

/* Convert a Sharp 'ASCII' character to a display character */
/* Incomplete, but good enough for version 1!               */
uint8_t mzascii2mzdisplay(uint8_t ascii)
{
  uint8_t displaychar = 0x00;            // space returned for anything not
                                         // in the switch and ifs below

  /* Deal with contiguous blocks of characters first */
  if ((ascii >= 0x21) && (ascii <= 0x29)) {
    displaychar=ascii+0x40;        // ! " # $ % & ' ( ) 
  }
  else if ((ascii >= 0x30) && (ascii <= 0x39)) {
    displaychar=ascii-0x10;        // 0 - 9
  }
  else if ((ascii >= 0x41) && (ascii <= 0x5a)) {
    displaychar=ascii-0x40;        // A - Z
  }
  else if ((ascii >= 0x60) && (ascii <= 0x68)) {
    displaychar=ascii+0x67;        // ufo, cars, people, faces
  }
  else if ((ascii >= 0x6a) && (ascii <= 0x6d)) {
    displaychar=ascii+0x7d;        // diodes and transistors
  }
  else if ((ascii >= 0x71) && (ascii <= 0x7e)) {
    displaychar=ascii+0x60;        // grey shapes, degree symbol
  }
  else if ((ascii >= 0x93) && (ascii <= 0x95)) {
    displaychar=ascii+0x11;        // hatching
  }
  else if ((ascii >= 0xb4) && (ascii <= 0xb6)) {
    displaychar=ascii-0x0d;
  }
  else {
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
      case 0x5e: displaychar=0x50;   //up arrow
                 break;
      case 0x5f: displaychar=0x45;   //left arrow
                 break;

      case 0x69: displaychar=0xdf;   //worm
                 break;
      case 0x6e: displaychar=0xec;   //capacitor horiz
                 break;
      case 0x6f: displaychar=0xed;   //capacitor vert
                 break;

      case 0x70: displaychar=0xef;   //chequered square
                 break;
      case 0x7f: displaychar=0x00;   //space ??
                 break;

      case 0x80: displaychar=0x00;   //space
                 break;
      case 0x81: displaychar=0xbd;   //st george cross
                 break;
      case 0x82: displaychar=0x9d;   //curve top up
                 break;
      case 0x83: displaychar=0xb1;   //qtr circle bottom left - top right
                 break;
      case 0x84: displaychar=0xb5;   //qtr circle top left - bottom right
                 break;
      case 0x85: displaychar=0xb9;   //curve bottom
                 break;
      case 0x86: displaychar=0xb4;   //curve left
                 break;
      case 0x87: displaychar=0x9e;   //curve top down
                 break;
      case 0x88: displaychar=0xb2;   //qtr circle top left - bottom right
                 break;
      case 0x89: displaychar=0xb6;   //qtr circle bottom left - top right
                 break;
      case 0x8a: displaychar=0xba;   //curve bottom
                 break;
      case 0x8b: displaychar=0xbe;   //nose
                 break;
      case 0x8c: displaychar=0x9f;
                 break;
      case 0x8d: displaychar=0xb3;
                 break;
      case 0x8e: displaychar=0xb7;
                 break;
      case 0x8f: displaychar=0xbb;
                 break;

      case 0x90: displaychar=0xbf;   //eye
                 break;
      case 0x91: displaychar=0xa3;   //vertical hatching
                 break;
      case 0x92: displaychar=0x85;   //e
                 break;
      case 0x96: displaychar=0x94;   //t
                 break;
      case 0x97: displaychar=0x87;   //g
                 break;
      case 0x98: displaychar=0x88;   //h
                 break;
      case 0x99: displaychar=0x9c;
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
      case 0xa7: displaychar=0xa1;   //double horiz bar
                 break;
      case 0xa8: displaychar=0xaf;   //O+umlaut
                 break;
      case 0xa9: displaychar=0x8b;   //k
                 break;
      case 0xaa: displaychar=0x86;   //f
                 break;
      case 0xab: displaychar=0x96;   //v
                 break;
      case 0xac: displaychar=0xa2;   //double vert bar
                 break;
      case 0xad: displaychar=0xab;   //u+umlaut
                 break;
      case 0xae: displaychar=0xaa;   //eszett
                 break;
      case 0xaf: displaychar=0x8a;   //j
                 break;

      case 0xb0: displaychar=0x8e;   //n
                 break;
      case 0xb1: displaychar=0xb0;
                 break;
      case 0xb2: displaychar=0xad;   //U+umlaut
                 break;
      case 0xb3: displaychar=0x8d;   //m
                 break;
      case 0xb7: displaychar=0x8f;   //o
                 break;
      case 0xb8: displaychar=0x8c;   //l
                 break;
      case 0xb9: displaychar=0xae;   //A+umlaut
                 break;
      case 0xba: displaychar=0xac;   //o+umlaut
                 break;
      case 0xbb: displaychar=0x9b;   //a+umlaut
                 break;
      case 0xbc: displaychar=0xc0;
                 break;
      case 0xbd: displaychar=0x99;   //y
                 break;
      case 0xbe: displaychar=0xbc;   //yen
                 break;
      case 0xbf: displaychar=0xb8;
                 break;

      case 0xc0: displaychar=0x00;   //space
                 break;
      case 0xc1: displaychar=0x3b;   //filled half rectangle right
                 break;
      case 0xc2: displaychar=0x3a;   //filled half rectangle bottom
                 break;
      case 0xc3: displaychar=0x70;   //line top
                 break;
      case 0xc4: displaychar=0x3c;   //line bottom
                 break;
      case 0xc5: displaychar=0x71;   //line left
                 break;
      case 0xc6: displaychar=0x5a;   //right arrow
                 break;
      case 0xc7: displaychar=0x3d;   //line right
                 break;
      case 0xc8: displaychar=0x43;   //unfilled square
                 break;
      case 0xc9: displaychar=0x56;   //half triangle fill bottom
                 break;
      case 0xca: displaychar=0x3f;   //line left thick
                 break;
      case 0xcb: displaychar=0x1e;   //lines |-
                 break;
      case 0xcc: displaychar=0xce;   //white circular blob
                 break;
      case 0xcd: displaychar=0x1c;   //lines top right quarter
                 break;
      case 0xce: displaychar=0x5d;   //lines bottom left quarter
                 break;
      case 0xcf: displaychar=0x3e;   //line bottom thick
                 break;

      case 0xd0: displaychar=0x5c;   //lines bottom right quarter
                 break;
      case 0xd1: displaychar=0x1f;   //lines _|_
                 break;
      case 0xd2: displaychar=0x5f;   //lines T
                 break;
      case 0xd3: displaychar=0x5e;   //lines -|
                 break;
      case 0xd4: displaychar=0x37;   //line right thick
                 break;
      case 0xd5: displaychar=0x7b;   //filled half rectangle left
                 break;
      case 0xd6: displaychar=0x7f;   //line right 3 thick
                 break;
      case 0xd7: displaychar=0x36;   //line top thick
                 break;
      case 0xd8: displaychar=0x7a;   //filled half rectangle top
                 break;
      case 0xd9: displaychar=0x7e;   //line bottom 3 thick
                 break;
      case 0xda: displaychar=0x33;   //lines bottom, right
                 break;
      case 0xdb: displaychar=0x4b;   //quarter circle bottom right
                 break;
      case 0xdc: displaychar=0x4c;   //quarter circle bottom left
                 break;
      case 0xdd: displaychar=0x1d;   //lines top left quarter
                 break;
      case 0xde: displaychar=0x6c;   //square blobs up 
                 break;
      case 0xdf: displaychar=0x5b;   //square blobs down
                 break;

      case 0xe0: displaychar=0x78;   //line middle bottom
                 break;
      case 0xe1: displaychar=0x41;   //spade
                 break;
      case 0xe2: displaychar=0x35;   //line middle left
                 break;
      case 0xe3: displaychar=0x33;   //line middle top
                 break;
      case 0xe4: displaychar=0x74;   //line middle+ top
                 break;
      case 0xe5: displaychar=0x30;   //line top -1
                 break;
      case 0xe6: displaychar=0x38;   //line middle- bottom
                 break;
      case 0xe7: displaychar=0x75;   //line middle- left
                 break;
      case 0xe8: displaychar=0x39;   //line middle+ right
                 break;
      case 0xe9: displaychar=0x4d;   //filled half triangle top right
                 break;
      case 0xea: displaychar=0x6f;   //quarter circle top right
                 break;
      case 0xeb: displaychar=0x6e;   //quarter circle top left
                 break;
      case 0xec: displaychar=0x32;   //lines bottom, left
                 break;
      case 0xed: displaychar=0x77;   //diagonal line top left - bottom right
                 break;
      case 0xee: displaychar=0x76;   //diagonal line bottom left - top right
                 break;
      case 0xef: displaychar=0x72;   //lines top, left
                 break;

      case 0xf0: displaychar=0x73;   //lines top, right
                 break;
      case 0xf1: displaychar=0x47;   //black blob on white background
                 break;
      case 0xf2: displaychar=0x7c;   //line off bottom
                 break;
      case 0xf3: displaychar=0x53;   //heart
                 break;
      case 0xf4: displaychar=0x31;   //line off left
                 break;
      case 0xf5: displaychar=0x4e;   //filled triangle top left
                 break;
      case 0xf6: displaychar=0x7c;   //st andrews cross
                 break;
      case 0xf7: displaychar=0x48;   //blob outline
                 break;
      case 0xf8: displaychar=0x46;   //club
                 break;
      case 0xf9: displaychar=0x7d;   //line off right
                 break;
      case 0xfa: displaychar=0x44;   //diamond
                 break;
      case 0xfb: displaychar=0x1b;   //£
                 break;
      case 0xfc: displaychar=0x58;   //down arrow
                 break;
      case 0xfd: displaychar=0x79;   //vertical line middle right
                 break;
      case 0xfe: displaychar=0x42;   //filled triangle bottom left
                 break;
      case 0xff: displaychar=0x60;   //pi
                 break;
    }
  }

  return(displaychar);
}
