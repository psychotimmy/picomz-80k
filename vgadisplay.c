/* Sharp MZ-80K  & MZ-80A emulator - vga output */
/*  Tim Holyoake, August 2024 - February 2025   */

#include "picomz.h"

#define VGA_MODE vga_mode_320x240_60    // This gives us a 40x30 display,
                                        // so we use the first 40x25 for the
                                        // Sharp MZ-80K.
#define VGA_WIDTH VGA_MODE.width
#define VGA_LINES 240
#define MIN_RUN 3

// MZ-80K/A visible screen is 40 chars x 25 lines
#define DWIDTH          40
#define DLINES          25
#define CWIDTH          8      // MZ-80K/A characters are 8 pixels wide
#define CHEIGHT         8      // ... and 8 pixels tall
#define DLASTLINE       (DLINES * CHEIGHT) // Last scanline of MZ-80K/A

/* Generate each pixel for the current scanline */
int32_t gen_scanline(uint32_t *buf, size_t buf_length, int lineNum)
{
  uint16_t *pixels = (uint16_t *) buf;
  int vrr = lineNum/CHEIGHT;           // Find the row of the VRAM we're using
  int cpr = lineNum%CHEIGHT;           // Find the pixel row in the character
                                       // ROM we need
  // Now work through the display columns to generate the correct scanline
  pixels += 1;
  for (uint8_t colidx=0;colidx<DWIDTH;colidx++) {
    uint8_t charbits;
    if ((ukrom == true) && (mzmodel == MZ80K))
      charbits=cgromuk80k[mzvram[vrr*DWIDTH+colidx]*CWIDTH+cpr];
    else if ((ukrom == false) && (mzmodel == MZ80K))
      charbits=cgromjp80k[mzvram[vrr*DWIDTH+colidx]*CWIDTH+cpr];
    else if (mzuserram[0x0191] == 0xFF) {   /* MZ80-K mode */
      charbits=cgromuk80a[mzvram[vrr*DWIDTH+colidx]*CWIDTH+cpr];
    }
    else {                                  /* MZ-80A native mode */
      // In this mode the full 2K VRAM is used, so need to work out where
      // the top of the screen is in the VRAM. Use monitor workarea addresses
      // 0x117D and 0x117E (4477 & 4478 decimal) to do this, and allow VRAM
      // to wrap around by masking the calculated address with 0x7FF (2048).
      int offset=(((mzuserram[0x017E]<<8)&0xFF00)|mzuserram[0x017D])-0xD000;
      charbits=cgromuk80a[mzvram[(vrr*DWIDTH+colidx+offset)&0x7FF]*CWIDTH+cpr];
    }
    *(++pixels) = (charbits & 0x80) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x40) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x20) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x10) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x08) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x04) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x02) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x01) ? whitepix : blackpix;
  }
  *(++pixels) = 0;
  *(++pixels) = COMPOSABLE_EOL_ALIGN;
  pixels = (uint16_t *) buf;
  pixels[0] = COMPOSABLE_RAW_RUN;
  pixels[1] = pixels[2];
  pixels[2] = DWIDTH*CWIDTH-2;

  return (DWIDTH*CWIDTH-4);
}

/* The bottom 40 scanlines are used for emulator status messages */
int32_t gen_last40_scanlines(uint32_t *buf, size_t buf_len, int lineNum)
{
  uint16_t *pixels = (uint16_t *) buf;
  int emusrow = (lineNum-DLASTLINE)/CHEIGHT;  // Find row of the emulator status
  int cpixrow = (lineNum-DLASTLINE)%CHEIGHT;  // Find pixel row in the character
                                              // ROM we need
  // Now work through the display columns to generate the correct scanline
  pixels += 1;
  for (uint8_t colidx=0;colidx<DWIDTH;colidx++) {
    uint8_t charbits;
    if ((ukrom == true) && (mzmodel == MZ80K))
      charbits=cgromuk80k[mzemustatus[emusrow*DWIDTH+colidx]*CWIDTH+cpixrow];
    else if ((ukrom == false) && (mzmodel == MZ80K))
      charbits=cgromjp80k[mzemustatus[emusrow*DWIDTH+colidx]*CWIDTH+cpixrow];
    else
      charbits=cgromuk80a[mzemustatus[emusrow*DWIDTH+colidx]*CWIDTH+cpixrow];
    *(++pixels) = (charbits & 0x80) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x40) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x20) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x10) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x08) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x04) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x02) ? whitepix : blackpix;
    *(++pixels) = (charbits & 0x01) ? whitepix : blackpix;
  }
  *(++pixels) = 0;
  *(++pixels) = COMPOSABLE_EOL_ALIGN;
  pixels = (uint16_t *) buf;
  pixels[0] = COMPOSABLE_RAW_RUN;
  pixels[1] = pixels[2];
  pixels[2] = DWIDTH*CWIDTH-2;

  return (DWIDTH*CWIDTH-4);
}

/* Output the composed scanline to the display */
void render_scanline(struct scanvideo_scanline_buffer *dest, int core)
{
  uint32_t *buf = dest->data;
  size_t buf_length = dest->data_max;
  int lineNum = scanvideo_scanline_number(dest->scanline_id);

  /* If we're beyond the last scanline of the MZ-80K/A display,
     output the emulator status area. Toggle vblank as required */
  if (lineNum == 0) 
    vblank = 0;
  if (lineNum >= DLASTLINE)  { 
    dest->data_used = gen_last40_scanlines(buf, buf_length, lineNum);
    if (lineNum == VGA_LINES-1) 
      vblank = 1; 
  }
  else
    dest->data_used = gen_scanline(buf, buf_length, lineNum);

  dest->status = SCANLINE_OK;
  return;
}

/* Prepare the next scanline and send it for display on core 1 */
void render_loop(void)
{
  int core_num = get_core_num();

  for(;;) {

    // Start a new buffer
    struct scanvideo_scanline_buffer *sb=
      scanvideo_begin_scanline_generation(true);

    // Fill this buffer with content
    render_scanline(sb, core_num);

    // Send the buffer for display
    scanvideo_end_scanline_generation(sb);
  }

  return;
}

/* Initialise the VGA code and render forever on core 1*/
void vga_main(void)
{
  scanvideo_setup(&VGA_MODE);
  scanvideo_timing_enable(true);

  render_loop();  // Core 1 never returns from here
  return;         // ... so this function will never return
}
