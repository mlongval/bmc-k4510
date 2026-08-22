/* VICKe -- the K4510 video chip.
 *
 * Register block at IO_VICKE ($D000), 256 bytes, byte-addressed. Every
 * pointer is a 28-bit physical address into main RAM; there is no video
 * memory. Rendering is per scanline into an 8-bit indexed framebuffer the
 * frontend supplies; the frontend applies vicke_palette_rgb().
 *
 *   $00  CTRL      bit0 display enable
 *   $01  BGCOL     background palette index (where nothing is drawn)
 *   $02  RASTER    read: current line (low 8)   $03: high bits
 *   $04  IRQSTAT   (reserved: vblank/raster/copper/collision)
 *   $05  IRQMASK   (reserved)
 *   $06  PALIDX    palette index for the write port
 *   $07  PALR      $08 PALG   $09 PALB  -- writing B commits the entry and
 *                  increments PALIDX
 *   $0A-$0F        reserved (sprite table ptr, copper ptr: later steps)
 *
 *   Layers 0..3 at $10 + n*$10, 16 bytes each:
 *   +0   LCTRL     bit0 enable, bits1-2 mode (0 bitmap, 1 tile, 2 text),
 *                  bits3-4 bpp (0=1, 1=2, 2=4, 3=8)
 *   +1   LPALOFS   palette offset for <8 bpp: index = (value << depth) | pixel
 *   +2,3 SCROLLX   16-bit, pixels
 *   +4,5 SCROLLY   16-bit, pixels
 *   +6,7 STRIDE    bitmap: bytes per row. tile/text: map entries per row.
 *   +8..+B DATA    28-bit pointer: pixels (bitmap) or glyph/tile set
 *   +C..+F MAP     28-bit pointer: tile map (one byte per cell for now)
 *
 * Layer 0 is bottom. Pixel index 0 is transparent in every layer except
 * the lowest enabled one. Text/tile cells are 8x8 at 1 bpp for now.
 */
#ifndef K4510_VICKE_H
#define K4510_VICKE_H
#include <stdint.h>

#define VICKE_WIDTH   640
#define VICKE_HEIGHT  480
#define VICKE_LAYERS  4

/* register offsets */
#define VR_CTRL     0x00
#define VR_BGCOL    0x01
#define VR_RASTER   0x02
#define VR_PALIDX   0x06
#define VR_PALR     0x07
#define VR_PALG     0x08
#define VR_PALB     0x09
#define VR_LAYER(n) (0x10 + (n) * 0x10)
#define VL_CTRL     0
#define VL_PALOFS   1
#define VL_SCROLLX  2
#define VL_SCROLLY  4
#define VL_STRIDE   6
#define VL_DATA     8
#define VL_MAP      12

#define VL_MODE_BITMAP 0
#define VL_MODE_TILE   1
#define VL_MODE_TEXT   2

void     vicke_reset(void);
uint8_t  vicke_read(uint8_t reg);
void     vicke_write(uint8_t reg, uint8_t v);
void     vicke_render(uint8_t *fb, int pitch);        /* one full frame */
uint32_t vicke_palette_rgb(int index);                /* 0x00RRGGBB */

#endif
