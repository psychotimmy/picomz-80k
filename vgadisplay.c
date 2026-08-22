/* Sharp MZ-80K, MZ-80A & MZ-700 emulator - VGA output */
/* Tim Holyoake, August 2024 - August 2026             */

#include "picomz.h"

#define VGA_MODE vga_mode_320x240_60    // This gives us a 40x30 display,
                                        // so we use the first 40x25 for the
                                        // Sharp MZs. Bottom 5 lines are used
                                        // for the status area.
#define VGA_WIDTH VGA_MODE.width
#define VGA_LINES 240
#define MIN_RUN 3

// MZ-80K/A/700 visible screen is 40 chars x 25 lines
#define DWIDTH          40
#define DLINES          25
#define CWIDTH          8      // MZ-80K/A/700 characters are 8 pixels wide
#define CHEIGHT         8      // ... and 8 pixels tall
#define DLASTLINE       (DLINES * CHEIGHT) // Last scanline of MZ-80K/A/700

/* Generate each pixel for the current scanline */
int32_t gen_scanline(uint32_t *buf, size_t buf_length, int lineNum)
{
  uint16_t *pixels = (uint16_t *) buf;
  int vrr = (lineNum/CHEIGHT)*DWIDTH;  // Find the row of the VRAM we're using
  int cpr = (lineNum%CHEIGHT);         // Find the pixel row in the character
                                       // ROM we need
  // Now work through the display columns to generate the correct scanline
  pixels += 1;
  for (uint8_t colidx=0;colidx<DWIDTH;colidx++) {
    uint8_t charbits,tb;
    uint16_t fgpix=whitepix; 
    uint16_t bgpix=blackpix; 
    if ((ukrom) && (mzmodel==MZ80K))
      charbits=cgromuk80k[mzvram[vrr+colidx]*CWIDTH+cpr];
    else if ((!ukrom) && (mzmodel==MZ80K))
      charbits=cgromjp80k[mzvram[vrr+colidx]*CWIDTH+cpr];
    else if ((mzuserram[0x0191]==0xFF) && (mzmodel==MZ80A)) { /* MZ80-K mode */
      charbits=cgromuk80a[mzvram[vrr+colidx]*CWIDTH+cpr];
    }
    else if (mzmodel==MZ80A) {                         /* MZ-80A native mode */
      // In this mode the full 2K VRAM is used, so need to work out where
      // the top of the screen is in the VRAM. Use monitor workarea addresses
      // 0x117D and 0x117E (4477 & 4478 decimal) to do this, and allow VRAM
      // to wrap around by masking the calculated address with 0x7FF (2048).
      int offset=((mzuserram[0x017E]<<8)|mzuserram[0x017D])-0xD000;
      charbits=cgromuk80a[mzvram[(vrr+colidx+offset)&0x7FF]*CWIDTH+cpr];
    } 
    else if (mzmodel==MZ700) {
      /* If the 7th bit (tb) of the colour VRAM that belongs to the character */
      /* is set, then add 256 to the character index (2nd character set used) */
      /* tb*2048 finds the correct character in the cgrom array */
      tb=(mzvram[0x0800+vrr+colidx]>>7)&0x01;
      if (ukrom)
        charbits=cgromuk700[(mzvram[vrr+colidx]*CWIDTH+cpr)+tb*2048];
      else
        charbits=cgromjp700[(mzvram[vrr+colidx]*CWIDTH+cpr)+tb*2048];
      /* MZ-700 is colour - override fgpix/bgpix */
      /* Background colour of character in bits 0-2, foreground in bits 4-6 */
      fgpix=colourpix[((mzvram[0x0800+vrr+colidx])>>4)&0x07];
      bgpix=colourpix[(mzvram[0x0800+vrr+colidx])&0x07];
    }
    *(++pixels) = (charbits & 0x80) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x40) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x20) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x10) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x08) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x04) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x02) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x01) ? fgpix : bgpix;
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
  int emusrow = ((lineNum-DLASTLINE)/CHEIGHT)*DWIDTH;  // Find row of the 
                                                       // emulator status area
  int cpixrow = (lineNum-DLASTLINE)%CHEIGHT;  // Find pixel row in the character
                                              // ROM we need
  // Now work through the display columns to generate the correct scanline
  pixels += 1;
  for (uint8_t colidx=0;colidx<DWIDTH;colidx++) {
    uint8_t charbits;
    uint16_t fgpix=whitepix; 
    uint16_t bgpix=blackpix; 
    if ((ukrom) && (mzmodel==MZ80K))
      charbits=cgromuk80k[mzemustatus[emusrow+colidx]*CWIDTH+cpixrow];
    else if ((!ukrom) && (mzmodel==MZ80K))
      charbits=cgromjp80k[mzemustatus[emusrow+colidx]*CWIDTH+cpixrow];
    else if (mzmodel==MZ80A)
      charbits=cgromuk80a[mzemustatus[emusrow+colidx]*CWIDTH+cpixrow];
    else if (mzmodel==MZ700) {
      if (ukrom)
        charbits=cgromuk700[mzemustatus[emusrow+colidx]*CWIDTH+cpixrow];
      else
        charbits=cgromjp700[mzemustatus[emusrow+colidx]*CWIDTH+cpixrow];
      fgpix=colourpix[7];
      bgpix=colourpix[0];
    }
    *(++pixels) = (charbits & 0x80) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x40) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x20) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x10) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x08) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x04) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x02) ? fgpix : bgpix;
    *(++pixels) = (charbits & 0x01) ? fgpix : bgpix;
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
