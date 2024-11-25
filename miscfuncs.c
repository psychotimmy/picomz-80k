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

  for (i=0; i<strlen(converted); i++) // converted - assume all spaces
    converted[i]=0x00;

  for (i=0; i<strlen(convert); i++) {
    switch(convert[i]) {
      case 0x20: converted[i]=0x00;    // space
                 break;
      case 0x21:                                  // !
      case 0x22:                                  // "
      case 0x23:                                  // #
      case 0x24:                                  // $
      case 0x25:                                  // %
      case 0x26:                                  // &
      case 0x27:                                  // '
      case 0x28:                                  // (
      case 0x29: converted[i]=convert[i]+0x40;    // ) symbols
                 break;
      case 0x2A: converted[i]=0x6B;               // *
                 break;
      case 0x2B: converted[i]=0x6A;               // +
                 break;
      case 0x2C: converted[i]=0x2F;               // ,
                 break;
      case 0x2D: converted[i]=0x2A;               // -
                 break;
      case 0x2E: converted[i]=0x2E;               // .
                 break;
      case 0x2F: converted[i]=0x2D;               // /
                 break;
      case 0x30:
      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36:
      case 0x37:
      case 0x38:
      case 0x39: converted[i]=convert[i]-0x10;    // 0 - 9
                 break;
      case 0x3A: converted[i]=0x4F;               // :
                 break;
      case 0x3B: converted[i]=0x2C;               // ;
                 break;
      case 0x3C: converted[i]=0x51;               // <
                 break;
      case 0x3D: converted[i]=0x2B;               // =
                 break;
      case 0x3E: converted[i]=0x57;               // >
                 break;
      case 0x3F: converted[i]=0x49;               // ?
                 break;
      case 0x40: converted[i]=0x55;               // @
                 break;
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
      case 0x45:
      case 0x46:
      case 0x47:
      case 0x48:
      case 0x49:
      case 0x4A:
      case 0x4B:
      case 0x4C:
      case 0x4D:
      case 0x4E:
      case 0x4F:
      case 0x50:
      case 0x51:
      case 0x52:
      case 0x53:
      case 0x54:
      case 0x55:
      case 0x56:
      case 0x57:
      case 0x58:
      case 0x59:
      case 0x5A: converted[i]=convert[i]-0x40;   // A - Z
                 break;
      case 0x5B: converted[i]=0x52;              // [
                 break;
      case 0x5C: converted[i]=0x59;              // \
                 break;
      case 0x5D: converted[i]=0x54;              // ]
                 break;
      case 0x61:
      case 0x62:
      case 0x63:
      case 0x64:
      case 0x65:
      case 0x66:
      case 0x67:
      case 0x68:
      case 0x69:
      case 0x6A:
      case 0x6B:
      case 0x6C:
      case 0x6D:
      case 0x6E:
      case 0x6F:
      case 0x70:
      case 0x71:
      case 0x72:
      case 0x73:
      case 0x74:
      case 0x75:
      case 0x76:
      case 0x77:
      case 0x78:
      case 0x79:
      case 0x7A: converted[i]=convert[i]+0x20;   // a - z
                 break;
      case 0xA3: converted[i]=0x1B;              // £
                 break;
      case 0xA5: converted[i]=0xBC;              // Yen
                 break;

      default:   converted[i]=0x00;              // Default is space
                 break;
    }
  }

  return;
}

/* Convert a Sharp 'ASCII' tape file name character to an 'ASCII'   */
/* character that will form part of a legal FAT (sd card) file name */
/* Currently incomplete, but good enough for most purposes.         */
uint8_t mzsafefilechar(uint8_t sharpchar)
{
  uint8_t asciichar;

  /* Default anything not in the lists below to a dash */
  asciichar=0x2d;

  /* Sharp upper case letters are all ok */
  if ((sharpchar >= 0x41) && (sharpchar <= 0x5a))
    asciichar=sharpchar;

  /* Sharp numbers are all ok */
  if ((sharpchar >= 0x30) && (sharpchar <= 0x39)) 
    asciichar=sharpchar;

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
  
  return(asciichar);
}

/* Convert a Sharp 'ASCII' character to a display character */
/* Painful and incomplete - but good enough for version 1!  */
uint8_t mzascii2mzdisplay(uint8_t ascii)
{
  uint8_t displaychar;

  switch(ascii) {
    case 0x21:                           //!
    case 0x22:                           //"
    case 0x23:                           //#
    case 0x24:                           //$
    case 0x25:                           //%
    case 0x26:                           //&
    case 0x27:                           //'
    case 0x28:                           //(
    case 0x29: displaychar=ascii+0x40;   //)
               break;
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
    case 0x30:                           //0
    case 0x31:                           //1
    case 0x32:                           //2
    case 0x33:                           //3
    case 0x34:                           //4
    case 0x35:                           //5
    case 0x36:                           //6
    case 0x37:                           //7
    case 0x38:                           //8
    case 0x39: displaychar=ascii-0x10;   //9
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
    case 0x41:                           //A
    case 0x42:                           //B
    case 0x43:                           //C
    case 0x44:                           //D
    case 0x45:                           //E
    case 0x46:                           //F
    case 0x47:                           //G
    case 0x48:                           //H
    case 0x49:                           //I
    case 0x4a:                           //J
    case 0x4b:                           //K
    case 0x4c:                           //L
    case 0x4d:                           //M
    case 0x4e:                           //N
    case 0x4f:                           //O
    case 0x50:                           //P
    case 0x51:                           //Q
    case 0x52:                           //R
    case 0x53:                           //S
    case 0x54:                           //T
    case 0x55:                           //U
    case 0x56:                           //V
    case 0x57:                           //W
    case 0x58:                           //X
    case 0x59:                           //Y
    case 0x5a: displaychar=ascii-0x40;   //Z
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
    default:   displaychar=0x00;   //<space> for anything not defined 
               break;
  }

  return(displaychar);
}
