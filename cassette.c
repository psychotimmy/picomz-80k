/* Sharp MZ-80K tape handling        */
/* Tim Holyoake, August-October 2024 */

#include "picomz.h"

#define LONGPULSE  1     /* cread() returns high for a long pulse */
#define SHORTPULSE 0     /*                 low for a short pulse */

#define READPT   400     /* Elapsed time in microseconds that on a tape  */
                         /* write a pulse is treated as a 1 rather than  */
                         /* than a 0. The real MZ-80K read point is      */
                         /* after 368us, but 400 is safe in the emulator */

#define RBGAP_L 120      /* Big tape gap length in bits - read  */
#define WBGAP_L 22000    /* Big tape gap length in bits - write */
#define RSGAP_L 120      /* Small tape gap length - read        */
#define WSGAP_L 11000    /* Small tape gap length - write       */
#define BTM_L  80        /* Big tape mark length                */
#define STM_L  40        /* Small tape mark length              */
#define L_L    1         /* One long pulse length               */
#define S256_L 256       /* 256 short pulses length             */
#define HDR_L  1024      /* Header 128 bytes, 1024 bits         */
#define CHK_L  16        /* Checksum 2 bytes, 16 bits           */
#define SKIP_L 24938     /* Used in cwrite() to skip the header */
                         /* copy and body preamble bits. The    */
                         /* calculation is in the comment below */
//(L_L+S256_L+HDR_L+(HDR_L/8)+CHK_L+(CHK_L/8)+L_L+WSGAP_L+STM_L+L_L)*2 

/* Start points in mzemustatus array of emulator status area lines */
#define EMULINE0      0
#define EMULINE1     40
#define EMULINE2     80
#define EMULINE3    120
#define EMULINE4    160

/* Used in mzspinny() */
#define TCOUNTERMAX 999  /* Maximum value of tapecounter */
#define TCOUNTERINC 200  /* Incr. tapecounter by 1 every TCOUNTERINC calls */

uint8_t crstate=0;       // Holds tape state for cread()
uint8_t cwstate=0;       // Holds tape state for cwrite()

uint8_t header[TAPEHEADERSIZE];// Tape headers are always 128 bytes
uint8_t body[TAPEBODYMAXSIZE]; // Maximum storage is 47.5K - 48640 bytes

static FATFS fs;         // File system pointer for sd card

/* MZ-80K tapes always have a 128 byte header, followed by a body */

// Tape format is as follows: 

// ** Header preamble **
// bgap - big gap - 22,000 short pulses (only need to send >100 when reading)
// btm -  big tape mark - 40 long, 40 short
// l - 1 long pulse

// ** Tape header **
// hdr - tape header - 128 bytes (1024 bits).
// chkh - header checksum - 2 bytes.
// l - 1 long pulse
// 256s - 256 short pulses
// hdrc - a copy of the tape header
// chkh - a copy of the header checksum
// l - 1 long pulse

// ** Body preamble **
// sgap - small gap - 11,000 short pulses (only need to send >100 when reading)
// stm - small tape mark - 20 long, 20 short
// l - 1 long pulse

// ** Tape body **
// file - variable length, set in header.
// chkf - file checksum - 2 bytes.
// l - 1 long pulse
// 256s - 256 short pulses
// filec - a copy of file
// chkf - a copy of the file checksum
// l - 1 long pulse

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
uint8_t mzascii2display(uint8_t ascii)
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

/* Update the tape counter in the emulator status area, line 3 */
void mzspinny(uint8_t state)
{
  uint8_t spos=0;
  static uint16_t spinny=0;         // Used to reset the tape counter
  static uint8_t ignore=0;          // Don't update the tape counter
                                    // with every call to this function

  // Reset the tape counter if state == 0
  if (!state) { 
    spinny=0;
    ignore=0;
  }

  // Increment ignore. If this is >= TCOUNTERINC then increment spinny.
  // If spinny > TCOUNTERMAX then spinny is reset to zero.

  if ((++ignore) >= TCOUNTERINC) {
    ignore=0;
    if ((++spinny) > TCOUNTERMAX) {
      spinny=0;
    }
  }

  // Write out value of spinny to the emulator status area
  spos=EMULINE3;
  mzemustatus[spos++]=0x14; //T
  mzemustatus[spos++]=0x81; //a
  mzemustatus[spos++]=0x90; //p
  mzemustatus[spos++]=0x85; //e
  mzemustatus[spos++]=0x00; //<space>
  mzemustatus[spos++]=0x83; //c
  mzemustatus[spos++]=0x8f; //o
  mzemustatus[spos++]=0x95; //u
  mzemustatus[spos++]=0x8e; //n
  mzemustatus[spos++]=0x94; //t
  mzemustatus[spos++]=0x85; //e
  mzemustatus[spos++]=0x92; //r
  mzemustatus[spos++]=0x4f; //:
  mzemustatus[spos++]=0x00; //<space>
  
  // Output the tape counter number
  uint16_t spinnyt=spinny;
  mzemustatus[spos++]=0x20+(uint8_t)(spinnyt/100);
  spinnyt=spinnyt%100;
  mzemustatus[spos++]=0x20+(uint8_t)(spinnyt/10);
  spinnyt=spinnyt%10;
  mzemustatus[spos]=0x20+(uint8_t) spinnyt;

  return;
}

/* Attempt to mount an sd card */
FRESULT tapeinit(void)
{
  FRESULT res;

  // Attempt to mount the sd card filesystem
  // Calling routine must deal with status
  sleep_ms(500);
  res=f_mount(&fs, "", 1);

  return(res);
}

/* Save MZ-80K user RAM to a file */
FRESULT mzsavedump(void)
{
  FIL fp;                           // File pointer
  FRESULT res;                      // FatFS function result
  uint bw;                          // Number of bytes written to file
  uint8_t uramheader[TAPEHEADERSIZE];  // A 'tape' header for the memory dump
  uint8_t dumpfile[11] =            // Memory dump filename
  { 'M','Z','D','U','M','P','.','M','Z','F','\0' };

  memset(uramheader,0,TAPEHEADERSIZE); // Clear the 'tape' header
  uramheader[0] = 0x20;             // Use 0x20 as the header identifier
                                    // (0x20 not used by real MZ-80K tapes)
  uramheader[1] = 0x4d;             // M
  uramheader[2] = 0x92;             // e
  uramheader[3] = 0xb3;             // m
  uramheader[4] = 0xb7;             // o
  uramheader[5] = 0x9d;             // r
  uramheader[6] = 0xbd;             // y
  uramheader[7] = 0x20;             // <space>
  uramheader[8] = 0x9c;             // d
  uramheader[9] = 0xa5;             // u
  uramheader[10]= 0xb3;             // m
  uramheader[11]= 0x9e;             // p
  uramheader[12]= 0x0d;             // <end of name>

  // Open a file on the sd card
  res=f_open(&fp,dumpfile,FA_CREATE_ALWAYS|FA_WRITE);
  if (res) {
    SHOW("Error on file open for MZDUMP.MZF, status is %d\n",res);
    return(res);
  }

  // Write the 'tape' header
  f_write(&fp, uramheader, TAPEHEADERSIZE, &bw);
  SHOW("Memory dump header: %d bytes written to MZDUMP.MZF\n",bw);

  // Write 'tape' contents - everything in mzuserram 
  f_write(&fp, mzuserram, URAMSIZE, &bw); 
  SHOW("Memory dump user RAM: %d bytes written to MZDUMP.MZF\n",bw);

  // Write 'tape' contents - everything in mzvram
  f_write(&fp, mzvram, VRAMSIZE, &bw); 
  SHOW("Memory dump video RAM: %d bytes written to MZDUMP.MZF\n",bw);

  // Write 'tape' contents - z80 state
  f_write(&fp, &mzcpu, sizeof(mzcpu), &bw);
  SHOW("Memory dump z80 cpu state: %d bytes written to MZDUMP.MZF\n",bw);

  // Close the file and return
  f_close(&fp);

  return(FR_OK);
}

/* Read MZ-80K memory dump        */
FRESULT mzreaddump(void)
{
  FIL fp;                           // File pointer
  FRESULT res;                      // FatFS function result
  uint br;                          // Number of bytes read from file
  uint8_t uramheader[TAPEHEADERSIZE];  // A 'tape' header for the memory dump
  uint8_t dumpfile[11] =            // Memory dump filename
  { 'M','Z','D','U','M','P','.','M','Z','F','\0' };

  // Open a file on the sd card
  res=f_open(&fp,dumpfile,FA_READ|FA_WRITE);
  if (res) {
    SHOW("Error on file open for MZDUMP.MZF, status is %d\n",res);
    return(res);
  }

  // Read the header - check it's what we're expecting
  f_read(&fp,uramheader,TAPEHEADERSIZE,&br);
  if (uramheader[0] != 0x20) {
    SHOW("Error on header read - expecting type 0x20\n");
    f_close(&fp);
    return(FR_INT_ERR);      // assertion failed
  }

  // Read mzuserram
  f_read(&fp, mzuserram, URAMSIZE, &br);
  if (br != URAMSIZE) {
    SHOW("Error on userram read - expecting %d bytes, got %d\n",URAMSIZE,br);
    f_close(&fp);
    return(FR_INT_ERR);      // assertion failed
  }

  // Read mzvram
  f_read(&fp, mzvram, VRAMSIZE, &br);
  if (br != VRAMSIZE) {
    SHOW("Error on video ram read - expecting %d bytes, got %d\n",VRAMSIZE,br);
    f_close(&fp);
    return(FR_INT_ERR);      // assertion failed
  }

  // Read z80 state
  f_read(&fp, &mzcpu, sizeof(mzcpu), &br);
  if (br != sizeof(mzcpu)) {
    SHOW("Error on z80 read - expecting %d bytes, got %d\n",sizeof(mzcpu),br);
    f_close(&fp);
    return(FR_INT_ERR);      // assertion failed
  }

  // Success - close file
  f_close(&fp);

  return(FR_OK);
}

/* Preload a tape file into the header/body memory ready for LOAD */
int16_t tapeloader(int16_t n)
{
  FIL fp;
  DIR dp;
  FILINFO fno;
  FRESULT res;
  uint bytesread,bodybytes,dc;

  res=f_opendir(&dp,"/");	/* Open the root directory on the sd card */
  if (res) {
    SHOW("Error on directory open for /, status is %d\n",res);
    return(-1);
  }

  // If we're passed a number less than 0, use 0 (first file).
  if (n < 0) n=0;

  dc=0;                         
  while (dc <= n) {             /* Find the nth file in the directory */
    res=f_readdir(&dp, &fno);   /* (i.e. the nth file on the 'tape')  */
    if (res != FR_OK || fno.fname[0] == 0) {
      /* We're at the end of the tape or an error has happened */
      /* Return with no change to the preloaded file */
      SHOW("End of tape, status is %d\n",res);
      f_closedir(&dp);
      return(-1);
    }
    if (fno.fattrib & AM_DIR)    /* All subdirectories ignored at present */
      SHOW("Ignoring directory %s\n",fno.fname);
    else 
      ++dc;                      /* Increment the file count */
  }
  f_closedir(&dp);               /* Close the directory pointer */
  
  // We now have the next file on the tape - preload it
  res=f_open(&fp,fno.fname,FA_READ|FA_OPEN_EXISTING);
  if (res) {
    SHOW("Error on file open for %s, status is %d\n",fno.fname,res);
    return(-1);
  }
  
  // MZ-80K tape headers are always 128 bytes
  f_read(&fp,header,TAPEHEADERSIZE,&bytesread);
  if (bytesread != TAPEHEADERSIZE) {
    SHOW("Header error - only read %d of 128 bytes\n",bytesread);
    f_close(&fp);
    return(-1);
  }

  // Work out how many bytes to read from the header - stored in
  // locations header[19] and header[18] (msb, lsb)
  bodybytes=((header[19]<<8)&0xFF00)|header[18];
  SHOW("Tape body length for tape %d is %d\n",n,bodybytes);
  f_read(&fp,body,bodybytes,&bytesread);
  if (bytesread != bodybytes) {
    SHOW("Body error - only read %d of %d bytes\n",bytesread,bodybytes);
    f_close(&fp);
    return(-1);
  }

  // Update the preloaded tape name in the emulator status area. Note
  // this is the name stored in the header, NOT the actual file name on
  // the SD card.
  uint8_t spos;
  // spos: EMULINE0 = start of status area line 0, EMULINE1 = line 1 etc.
  for (spos=EMULINE1;spos<EMULINE2;spos++) 
    mzemustatus[spos]=0x00;
  spos=EMULINE1;
  mzemustatus[spos++]=0x0e;  //N
  mzemustatus[spos++]=0x85;  //e
  mzemustatus[spos++]=0x98;  //x
  mzemustatus[spos++]=0x94;  //t
  mzemustatus[spos++]=0x00;  //<space>
  mzemustatus[spos++]=0x86;  //f
  mzemustatus[spos++]=0x89;  //i
  mzemustatus[spos++]=0x8c;  //l
  mzemustatus[spos++]=0x85;  //e
  mzemustatus[spos++]=0x00;  //<space
  mzemustatus[spos++]=0x89;  //i
  mzemustatus[spos++]=0x93;  //s
  mzemustatus[spos++]=0x4f;  //:
  mzemustatus[spos++]=0x00;  //<space>
  // Tape name terminates with 0x0d or is 17 characters long
  // Stored in header[1] to header[17] - update status area with this
  // Note - needs converting from MZ 'ASCCI' to MZ display codes
  uint8_t hpos=1;
  while ((header[hpos] != 0x0d) && (hpos <= 17))
    mzemustatus[spos++]=mzascii2display(header[hpos++]);

  // Update the preloaded tape type in the emulator status area.
  for (spos=EMULINE2;spos<EMULINE3;spos++) 
    mzemustatus[spos]=0x00;
  spos=EMULINE2;
  mzemustatus[spos++]=0x06;  //F
  mzemustatus[spos++]=0x89;  //i
  mzemustatus[spos++]=0x8c;  //l
  mzemustatus[spos++]=0x85;  //e
  mzemustatus[spos++]=0x00;  //<space>
  mzemustatus[spos++]=0x94;  //t
  mzemustatus[spos++]=0x99;  //y
  mzemustatus[spos++]=0x90;  //p
  mzemustatus[spos++]=0x85;  //e
  mzemustatus[spos++]=0x00;  //<space>
  mzemustatus[spos++]=0x89;  //i
  mzemustatus[spos++]=0x93;  //s
  mzemustatus[spos++]=0x4f;  //:
  mzemustatus[spos++]=0x00;  //<space>
  // Type of tape is stored in the header
  // 0x01 = machine code, 0x02 = language (BASIC,Pascal etc.), 0x03 = data
  // 0x04 = zen source, 0x20 = memory dump (Pico MZ-80K specific)
  switch (header[0]) {
    case 0x01: mzemustatus[spos++]=0x8d; //m
               mzemustatus[spos++]=0x2d; ///
               mzemustatus[spos++]=0x83; //c
               mzemustatus[spos++]=0x00; //<space>
               mzemustatus[spos++]=0x83; //c
               mzemustatus[spos++]=0x8f; //o
               mzemustatus[spos++]=0x84; //d
               mzemustatus[spos]=0x85;   //e
               break;
    case 0x02: mzemustatus[spos++]=0x02; //B
               mzemustatus[spos++]=0x01; //A
               mzemustatus[spos++]=0x13; //S
               mzemustatus[spos++]=0x09; //I
               mzemustatus[spos++]=0x03; //C
               mzemustatus[spos++]=0x00; //<space>
               mzemustatus[spos++]=0x85; //e
               mzemustatus[spos++]=0x94; //t
               mzemustatus[spos++]=0x83; //c
               mzemustatus[spos]=0x2e;   //.
               break;
    case 0x03: mzemustatus[spos++]=0x84; //d
               mzemustatus[spos++]=0x81; //a
               mzemustatus[spos++]=0x94; //t
               mzemustatus[spos]=0x81;   //a
               break;
    case 0x04: mzemustatus[spos++]=0x9a; //z
               mzemustatus[spos++]=0x85; //e
               mzemustatus[spos]=0x8e;   //n
               break;
    case 0x20: mzemustatus[spos++]=0x8d; //m
               mzemustatus[spos++]=0x85; //e
               mzemustatus[spos++]=0x8d; //m
               mzemustatus[spos++]=0x8f; //o
               mzemustatus[spos++]=0x92; //r
               mzemustatus[spos++]=0x99; //y
               mzemustatus[spos++]=0x00; //<space>
               mzemustatus[spos++]=0x84; //d
               mzemustatus[spos++]=0x95; //u
               mzemustatus[spos++]=0x8d; //m
               mzemustatus[spos]=0x90;   //p
               break;
    default:   mzemustatus[spos++]=0x95; //u
               mzemustatus[spos++]=0x8e; //n
               mzemustatus[spos++]=0x8b; //k
               mzemustatus[spos++]=0x8e; //n
               mzemustatus[spos++]=0x8f; //o
               mzemustatus[spos++]=0x97; //w
               mzemustatus[spos]=0x8e;   //n
               break;
  }

  // We've read the tape successfully if we get here
  SHOW("Successful preload of %s\n",fno.fname);
  f_close(&fp);

  return(n);     /* Return the file number loaded - matches requested */
}

/* Write a new file to sd card 'tape'                             */
void tapewriter(void)
{
  uint8_t sharpfilelen=0;
  uint8_t sdfilename[22];  // sdfilename needs 1 more char than
                           // the Sharp tape file name due to null
                           // termination requirements
                           // plus 4 characters for the .mzf extension
  uint bw;                 // Number of bytes written to file
  uint16_t i;              // Counts file body bytes written to sd card
  FRESULT res;
  FIL fp;

  SHOW("In tapewriter()\n");
  SHOW("Convert Sharp tape file name to sensible ASCII\n");

  // Sharp tape file name is up to 17 characters stored in header[1]
  // to header[17]. If less than 17 characters, name ends with 0x0D

  // Find each character of the Sharp tape file name and convert
  // it into something safe for the sd card file name
  while ((sharpfilelen < 17) && (header[sharpfilelen+1] != 0x0D)) {
    sdfilename[sharpfilelen]=mzsafefilechar(header[sharpfilelen+1]);
    ++sharpfilelen;
  }

  // Add .MZF and null terminate
  sdfilename[sharpfilelen++]='.';
  sdfilename[sharpfilelen++]='M';
  sdfilename[sharpfilelen++]='Z';
  sdfilename[sharpfilelen++]='F';
  sdfilename[sharpfilelen]='\0';

  // Open a file on the sd card for writing. If it exists already
  // we simply overwrite it ... just as would happen on a tape.
  res=f_open(&fp,sdfilename,FA_CREATE_ALWAYS|FA_WRITE);
  if (res) {
    SHOW("Error on file open for %s, status is %d\n",sdfilename,res);
    return;
  }

  // Write the 128 byte header to the file
  f_write(&fp, header, TAPEHEADERSIZE, &bw);
  SHOW("%d header bytes written to %s\n",bw,sdfilename);

  // Write the tape body to the file
  uint16_t bodybytes=((header[19]<<8)&0xFF00)|header[18];
  SHOW("bodybytes is\n",bodybytes);
  for (i=0;i<bodybytes;i++)
    f_write(&fp, &body[i], 1, &bw);
  SHOW("%d file body bytes written to %s\n",i,sdfilename);

  // Close the file and return
  f_close(&fp);

  return;
}

/* Resets the tape state machines. Called at the */
/* end of a successful read or write, or if the  */
/* BREAK key is pressed to abort.                */
void reset_tape(void)
{
  crstate=0;
  cwstate=0;
  /* Also reset the motor and sense flags - not sure if this
     is really needed, but should be harmless ... */
  cmotor=0;
  csense=0;

  return;
}

/* Read an MZ-80K format tape one bit at a time */
/* Pseudo finite state machine implementation   */
/* If the header and body are read successfully */
/* at the first attempt, the read process ends  */
/* and the second copy is not read. This impl.  */
/* assumes that the first read is ALWAYS good,  */
/* as we're using .mzf files rather than a real */
/* cassette tape.                               */
uint8_t cread(void)
{
                             // Used to calculate the bit to output from tape
  uint8_t bitshift;          // to the MZ-80K when reading the header or body
  static uint16_t chkbits;   // Tracks number of long pulses sent in the header
                             // or body to enable the checksum to be calculated
                             // MUST be a 16 bit unsigned value
  static uint16_t bodybytes; // Length of tape body as declared in the header
  static bool longsent;      // Tracks if a long pulse has been sent before 
                             // each new byte of the header, checksums and body
  static uint8_t checksum[2];// Stores the calculated checksum
  static uint8_t hilo=0;     // Used for the 1 -> tape bit read -> 0 logic
  static uint32_t secbits;   // Tracks where we are in the current tape section

  if (cmotor==0) {
    if (crstate==0) {
      return(LONGPULSE); // Motor is off and we're not reading a tape
    }
    else {               // Motor is off and we've part read a tape
      hilo=0;            // Reset hilo counter for next time motor is on
      return(LONGPULSE);
    }
  }

  // If we reach here, the motor is running and sense has been triggered
  // but check that we're not writing a tape before doing anything ...
  if (cwstate > 0) return(LONGPULSE);

  // To mimic a tape being read, we need to surround each bit sent with a
  // high bit and a low bit. The lines below do this in the simplest way
  // possible ... by using modulo 3 arithmetic

  hilo=(hilo+1)%3;           // Sequence is 1, followed by the tape bit (0/1),
  if (hilo<2) return(hilo);  // followed by 0 until end of tape is reached

  // Initialise local statics if state is 0, transition to state 1
  if (crstate==0) {
    secbits=0;
    chkbits=0;
    longsent=false;
    crstate=1;
    // We don't return here - always fall through to state 1 immediately
  }
 
  /* Header preamble - bgap, btm, l - state 1 */
  // Note - 22,000 pulses in a real bgap, but anything > 100 will work
  // when a tape is being read (writing is different!) 
  if (crstate==1) {
    if (secbits<RBGAP_L) {
      ++secbits;
      return(SHORTPULSE);
    }
    if (secbits<RBGAP_L+(BTM_L/2)) {  /* First half of btm is long pulses */
      ++secbits;
      return(LONGPULSE);
    }
    if (secbits<RBGAP_L+BTM_L) {
      ++secbits;
      return(SHORTPULSE);
    }
    secbits=0;
    crstate=2;
    return(LONGPULSE);
  }

  /* First copy of the header */
  if (crstate==2) {
    if (secbits<HDR_L) {
      /* One LONGPULSE is sent before every byte of the header */
      if (((secbits%8)==0) && (longsent==false)) {
        /* Note - we don't increment secbits here */
        longsent=true;
        return(LONGPULSE);
      }
      longsent=false;
      /* Bytes are sent starting with bit 7 (msb) */
      bitshift=secbits%8;
      if (((header[secbits++/8]<<bitshift)&0x80) == 0x80) {
        ++chkbits; // Increment the long pulse count for calculating chkh
        return(LONGPULSE);
      }
      return(SHORTPULSE);
    }
    /* At the end of the header, move onto checksum state (3) */
    secbits=0;
    crstate=3;
  }

  /* Header checksum - state 3 */
  if (crstate==3) {
    if (secbits<CHK_L) {
      if ((secbits==0)&&(chkbits>0)) {
        // Need to calculate the header checksum
        // Note as chkbits is a uint16_t, we don't need to do modulo 2^16
        // as this will automatically be taken care of for us
        checksum[0]=(chkbits>>8)&0xFF; /* MSB of the checksum */
        checksum[1]=chkbits&0xFF;      /* LSB of the checksum */
        SHOW("Header checksum is 0x%04x 0x%02x 0x%02x\n",chkbits,checksum[0],checksum[1]);
        // Reset chkbits for the next time a checksum is calculated
        chkbits=0;
      }
      if (((secbits % 8) == 0) && (longsent == false)) {
        /* Note - we don't increment secbits here */
        longsent = true;
        return(LONGPULSE);
      }
      /* Reset the longsent flag */
      longsent = false;
      /* Bytes are sent starting with the MSB */
      bitshift=secbits%8;
      if (((checksum[secbits++/8]<<bitshift)&0x80) == 0x80) {
        return(LONGPULSE);
      }
      return(SHORTPULSE);
    }
    /* At the end of the checksum */
    /* Note - current assumption is that this is correct */
    /* Reasonable - as this isn't a real cassette tape */
    /* Saves a little time and complexity */
    secbits=0;
    crstate=7; 
  }

  /* States 4,5 and 6 are only required if the header checksum failed */
  /* The emulator uses .mzf files and assumes the copy of the header */
  /* is not required, but these states are reserved in case the ability */
  /* to use .wav tape copies is ever implemented */
  /* State 4 - one long, 256 short pulses */
  /* State 5 - header copy */
  /* State 6 - header checksum copy */

  /* We now have a long pulse, short gap, short tape mark and long pulse */
  /* before we finally get to the tape body */

  if (crstate==7) {
    if (secbits<L_L) {
      ++secbits;
      return(LONGPULSE);
    }
    if (secbits<L_L+RSGAP_L) {
      ++secbits;
      return(SHORTPULSE);
    }
    if (secbits<L_L+RSGAP_L+(STM_L/2)) {
      ++secbits;
      return(LONGPULSE);
    }
    if (secbits<L_L+RSGAP_L+STM_L) {
      ++secbits;
      return(SHORTPULSE);
    }
    if (secbits<L_L+RSGAP_L+STM_L+L_L) {
      ++secbits;
      return(LONGPULSE);
    }
    secbits=0;
    /* Tape body - length is calculated from the values stored by the header */
    /* in memory locations 0x1103 and 0x1102 from the 20th & 19th values     */
    /* found in the header - i.e. header[19] (msb) and header[18] (lsb).     */
    bodybytes=((header[19]<<8)&0xFF00)|header[18];
    SHOW("Body length is 0x%04x (%d) bytes\n",bodybytes,bodybytes);
    SHOW("Transition to state 8 - program data\n");
    crstate=8;
  }

  /* Process the tape body - state 8 */
  if (crstate==8) {
    if (secbits<(bodybytes*8)) {     // 1 byte = 8 bits to transmit
      /* One LONGPULSE is sent before every byte of the header */
      if (((secbits%8)==0) && (longsent==false)) {
        /* Note - we don't increment secbits here */
        longsent=true;
        mzspinny(1); //Increment tape counter
        return(LONGPULSE);
      }
      longsent=false;
      /* Bytes are sent starting with bit 7 (msb) */
      bitshift=secbits%8;
      if (((body[secbits++/8]<<bitshift)&0x80) == 0x80) {
        ++chkbits; // Increment the long pulse count for calculating chkb
        return(LONGPULSE);
      }
      return(SHORTPULSE);
    }
    /* At the end of the body, move onto checksum state (9) */
    SHOW("Transition to state 9 - program checksum\n");
    SHOW("%d bits processed\n",secbits);
    SHOW("%d bytes processed\n",secbits/8);
    secbits=0;
    crstate=9;
  }

  /* Body checksum - state 9 */
  if (crstate==9) {
    if (secbits<CHK_L) {
      if ((secbits==0)&&(chkbits>0)) {
        // Need to calculate the body checksum
        // Note as chkbits is a uint16_t, we don't need to do modulo 2^16
        // as this will automatically be taken care of for us
        checksum[0]=(chkbits>>8)&0xFF; /* MSB of the checksum */
        checksum[1]=chkbits&0xFF;      /* LSB of the checksum */
        SHOW("Body checksum is 0x%02x%02x\n",checksum[0],checksum[1]);
        // Reset chkbits for the next time a checksum is calculated
        chkbits=0;
      }
      if (((secbits % 8) == 0) && (longsent == false)) {
        /* Note - we don't increment secbits here */
        longsent = true;
        return(LONGPULSE);
      }
      /* Reset the longsent flag */
      longsent = false;
      /* Bytes are sent starting with the MSB */
      bitshift=secbits%8;
      if (((checksum[secbits++/8]<<bitshift)&0x80) == 0x80) {
        return(LONGPULSE);
      }
      return(SHORTPULSE);
    }
    /* At the end of the checksum stop */
    /* Assumes copy of program data is not needed */
    SHOW("Transition to state 13 - stop\n");
    secbits=0;
    crstate=13; 
  }

  /* States 10,11 and 12 are only needed if the program body has failed */
  /* to checksum correctly, so are not required for .mzf files */
  /* State 10 - a long pulse followed by 256 short */
  /* State 11 - a copy of the body */
  /* State 12 - a copy of the body checksum */

  if (crstate==13) {
  /* At end of body checksum, reset tape state, send final stop bit */
      hilo=0;
      reset_tape();
      SHOW("Final stop bit sent\n");
      return(LONGPULSE);
  }

  /* Catch any errors - shouldn't happen, but ...        */
  SHOW("Error in cread() - unknown state %d\n",crstate);
  /* Reset hilo to 0, reset state */
  hilo=0;
  reset_tape();

  return(LONGPULSE);
}

/* Write an MZ-80K format tape one bit at a time */
/* Pseudo finite state machine implementation    */
void cwrite(uint8_t nextbit)
{
  /* Note: cwrite() can only ever be called if the motor and sense are on */

  static uint16_t bodybytes; // Length of tape body as declared in the header
  static bool longread;      // Tracks if a long pulse has been read before 
                             // each new byte of the header, checksums and body
  static uint32_t secbits;   // Tracks where we are in the current tape section
  static int32_t low,high;   // Count of low and high bits received
  static absolute_time_t hightime;  // Timestamp of last high bit received
  static absolute_time_t lowtime;   // Timestamp of last low bit received
  static uint16_t chkbits;   // Tracks number of long pulses recvd in the header
                             // or body to enable the checksum to be calculated
                             // MUST be a 16 bit unsigned value
  static uint8_t checksum[2];// Stores the calculated checksum
  uint8_t pulse;             // Current header or body pulse: 0=low, 1=high
  
  if (cwstate==0) {
    /* The first high bit has been received */
    crstate=0;               // Reset the cread() state to 0. Something odd
                             // happens on a SAVE, as a real MZ-80K tries to
                             // read the tape first. On a real machine it can't
                             // do this as 'rec' is also pressed. On this 
                             // emulator, it starts  to read any preloaded 
                             // tape and then stops, before starting to write
                             // to it. Hence the need for this statement!
    secbits=0;               // Section (state) bit count
    low=0;                   // low pulse counter
    high=0;                  // high pulse counter
    hightime=get_absolute_time(); // Timestamp of first high bit received
    cwstate=1;                    // Process the preamble bits in state 1.
    return;                  
  }

  /* State 1 - tape header preamble */
  if (cwstate==1) {
    if (nextbit==0) {
      lowtime=get_absolute_time();
      if (absolute_time_diff_us(hightime,lowtime) < READPT)
        ++low;                   // We have a low (short) pulse
      else
        ++high;                  // We have a high (long) pulse
      ++secbits;                 // Increment pulses counted
    }
    else {
      hightime=get_absolute_time();
    }
    /* Check that we have received 22,040 low pulses and 41 high pulses */
    /* when the total received is 22,081 - ie, after WBGAP_L+BTM_L+L_L  */
    if (secbits==WBGAP_L+BTM_L+L_L) {
      if (low==WBGAP_L+BTM_L/2) {
        SHOW("Tape header preamble written ok\n");
        // All ok - move onto the next state
        cwstate=2;
        secbits=0;
        low=0;
        high=0;
        chkbits=0;
      }
      else {
        SHOW("Error writing tape header preamble %d %d %d\n",secbits,low,high);
        cwstate=0;
      }
    }
    return;
  }

  /* State 2 - header */
  if (cwstate==2) {
    if (nextbit==0) {
      lowtime=get_absolute_time();
      if (absolute_time_diff_us(hightime,lowtime) < READPT)
        pulse=0;                 // We have a low (short) pulse
      else 
        pulse=1;                 // We have a high (long) pulse
      if (((secbits%8)==0) && (longread==false)) {
        // This is the long pulse that preceeds every byte of the header,
        // so we ignore it and blank the next byte of the header ready
        // for the next 8 bits
        header[secbits/8]=0x00;
        longread=true;
      }
      else {
        longread=false;    // Reset for next byte
        header[secbits/8]=(header[secbits/8]<<1)|pulse; // order is msb first 
        ++secbits;         // Increment data pulses counted
        chkbits += pulse;  // If pulse was long, increment chkbits count
      }
    }
    else {
      hightime=get_absolute_time();
    }
    /* Check to see if we're at the end of the header */
    if (secbits==HDR_L) {
      cwstate=3;
      secbits=0;
      longread=false;
    }
    return;
  }

  /* State 3 - header checksum */
  if (cwstate==3) {
    if (nextbit==0) {
      lowtime=get_absolute_time();
      if (absolute_time_diff_us(hightime,lowtime) < READPT)
        pulse=0;                 // We have a low (short) pulse
      else 
        pulse=1;                 // We have a high (long) pulse
      if (((secbits%8)==0) && (longread==false)) {
        // This is the long pulse that preceeds every byte of the checksum,
        // so we ignore it and blank the next byte of the checksum ready
        // for the next 8 bits
        checksum[secbits/8]=0x00;
        longread=true;
      }
      else {
        longread=false;     // Reset for next byte
        checksum[secbits/8]=(checksum[secbits/8]<<1)|pulse; // msb first 
        ++secbits;         // Increment data pulses counted
      }
    }
    else {
      hightime=get_absolute_time();
    }
    /* Check to see if we're at the end of the checksum */
    if (secbits==CHK_L) {
      if (chkbits==(((checksum[0]<<8)&0xFF00)|checksum[1]))
        SHOW("Header checksum is ok\n");
      else
        SHOW("Header checksum is bad ... carrying on anyway\n");
      bodybytes=((header[19]<<8)&0xFF00)|header[18]; // Needed for state 8
      SHOW("Body length is 0x%04x\n",bodybytes);
      cwstate=4;
      chkbits=0;
      secbits=0;
      longread=false;
    }
    return;
  }

  /* We're going to skip the header copy and checksum copy in the emulator */
  /* and move straight to state 8 - file body once we've counted enough */
  /* pulses - 12,469 pulses in total - this is SKIP_L (12,469*2) bits */

  /* State 4 - long pulse, 256 short */
  /* State 5 - header copy (1024 pulses + 128 long pulses)*/
  /* State 6 - header checksum copy (16 pulses + 2 long pulses) */
  /* State 7 - long pulse, 11,000 short, 20 long, 20 short, long pulse */

  if (cwstate==4) {
    ++secbits;                   // Increment number of bits received
                                 // (1 pulse = 1 followed by 0 = 2 bits)
    if (secbits==SKIP_L) {
      // Assume all is ok - move onto state 8 - file body
      SHOW("Gap between header and copy assumed ok\n");
      cwstate=8;
      secbits=0;
    }
    return;
  }

  /* State 8 - file body */
  if (cwstate==8) {
    if (nextbit==0) {
      lowtime=get_absolute_time();
      if (absolute_time_diff_us(hightime,lowtime) < READPT)
        pulse=0;                 // We have a low (short) pulse
      else 
        pulse=1;                 // We have a high (long) pulse
      if (((secbits%8)==0) && (longread==false)) {
        // This is the long pulse that preceeds every byte of the body,
        // so we ignore it and blank the next byte of the body ready
        // for the next 8 bits
        body[secbits/8]=0x00;
        longread=true;
        mzspinny(1); //Increment tape counter
      }
      else {
        longread=false;    // Reset for next byte
        body[secbits/8]=(body[secbits/8]<<1)|pulse; // order is msb first 
        ++secbits;         // Increment data pulses counted
        chkbits += pulse;  // If pulse was long, increment chkbits count
      }
    }
    else {
      hightime=get_absolute_time();
    }
    /* Check to see if we're at the end of the body */
    if (secbits==bodybytes*8) {
      cwstate=9;
      secbits=0;
      longread=false;
    }
    return;
  }

  /* State 9 - file body checksum */
  if (cwstate==9) {
    if (nextbit==0) {
      lowtime=get_absolute_time();
      if (absolute_time_diff_us(hightime,lowtime) < READPT)
        pulse=0;                 // We have a low (short) pulse
      else 
        pulse=1;                 // We have a high (long) pulse
      if (((secbits%8)==0) && (longread==false)) {
        // This is the long pulse that preceeds every byte of the checksum,
        // so we ignore it and blank the next byte of the checksum ready
        // for the next 8 bits
        checksum[secbits/8]=0x00;
        longread=true;
      }
      else {
        longread=false;    // Reset for next byte
        checksum[secbits/8]=(checksum[secbits/8]<<1)|pulse; // msb first 
        ++secbits;         // Increment data pulses counted
      }
    }
    else {
      hightime=get_absolute_time();
    }
    /* Check to see if we're at the end of the checksum */
    if (secbits==CHK_L) {
      if (chkbits==(((checksum[0]<<8)&0xFF00)|checksum[1]))
        SHOW("Body checksum is ok\n");
      else
        SHOW("Body checksum bad ... pushing on anyway\n");
      cwstate=10;
      chkbits=0;
      secbits=0;
      longread=false;
    }
    return;
  }

  /* We're going to skip the body copy and checksum copy in the emulator */
  /* and move straight to end of file. This is */
  /* 1+256+bodybytes*8+bodybytes+16+2 pulses to get to state 13 - the */
  /* final long pulse */

  /* State 10 - long pulse, 256 short */
  /* State 11 - file body copy  - bodybytes*8 + bodybytes pulses */
  /* State 12 - file checksum copy  - CHK_L + 2 pulses */

  if (cwstate==10) {
    ++secbits;                 // Increment bits counted
    /* Check that we have received enough 1/0 bits - 2 x number of pulses */
    if (secbits==(L_L+S256_L+bodybytes*8+bodybytes+CHK_L+2)*2) {
      // Assumed all ok - move onto the final state
      SHOW("Skipped body copy - assumed ok\n");
      cwstate=13;
      secbits=0;
    }
    return;
  }

  /* State 13 - the last long pulse */
  if (cwstate==13) {
    if (nextbit==0) {
      lowtime=get_absolute_time();
      if (absolute_time_diff_us(hightime,lowtime) < READPT)
        ++low;                   // We have a low (short) pulse
      else
        ++high;                  // We have a high (long) pulse
      ++secbits;                 // Increment pulses counted
    }
    else {
      hightime=get_absolute_time();
    }
    /* Check that we have received 1 high pulse */
    /* when the total received is 1 */
    if (secbits==L_L) { 
      if (high==L_L) {
        // All ok - finish write
        SHOW("End of file reached ok - writing to sd card\n");
        tapewriter();
        SHOW("sd card written\n");
        cwstate=0;
        secbits=0;
        high=0;
        low=0;
      }
      else {
        SHOW("Error at end of file! %d %d %d\n",secbits,low,high);
        cwstate=0;
        secbits=0;
        high=0;
        low=0;
      }
    }
    return;
  }

  /* Shouldn't be able to get here */
  SHOW("Error - unknown cwrite() state %d\n",cwstate);
  cwstate=0;

  return;
}
