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
 *   $0A-$0D SPRTAB 28-bit pointer to the sprite attribute table (128 x 16 B)
 *   $0E     SPRCTL bit0 sprites enable
 *   $0F            reserved (copper pointer: later)
 *   $40-$4F COLSS  read: sprite-sprite collision bits, one bit per sprite
 *                  (sprite n hit another sprite this frame). Cleared on read of $40.
 *   $50-$5F COLSL  read: sprite-layer collision bits (sprite n over a
 *                  non-transparent layer pixel). Cleared on read of $50.
 *
 *   Sprite attribute entry, 16 bytes, in main RAM:
 *   +0,1 X (signed 16)   +2,3 Y (signed 16)   +4..7 DATA 28-bit pointer
 *   +8   CTRL  bit0 enable, bit1 8 bpp (else 4), bit2 H-flip, bit3 V-flip,
 *              bits4-5 Z: drawn after layer Z (0..3)
 *   +9   SIZE  bits0-1 width 8/16/32/64, bits2-3 height 8/16/32/64
 *   +10  PALOFS (4 bpp: index = PALOFS<<4 | pixel)
 *   +11..15 reserved
 *   Pixel 0 is transparent. No per-line limit. 128 sprites.
 *
 *   Layers 0..3 at $10 + n*$10, 16 bytes each:
 *   +0   LCTRL     bit0 enable, bits1-2 mode (0 bitmap, 1 tile, 2 text8,
 *                  3 text32), bits3-4 bpp (0=1, 1=2, 2=4, 3=8),
 *                  bits5-6 cell size (tile: 8/16/32/64 px square;
 *                  text: 0 = 8x8, 1 = 8x16)
 *   +1   LPALOFS   palette offset for <8 bpp: index = (value << depth) | pixel
 *   +2,3 SCROLLX   16-bit, pixels
 *   +4,5 SCROLLY   16-bit, pixels
 *   +6,7 STRIDE    bitmap: bytes per row. tile/text: map entries per row.
 *   +8..+B DATA    28-bit pointer: pixels (bitmap) or glyph/tile set
 *   +C..+F MAP     28-bit pointer: the map
 *
 * Map formats:
 *   tile    2 bytes/cell: bits 0-9 tile index, 10 H-flip, 11 V-flip,
 *           12-15 palette offset (used for <8 bpp). Tile pixel data at
 *           DATA + index * (size*size*bpp/8), rows MSB-first packed.
 *   text8   1 byte/cell: glyph index. 1 bpp 8xH glyphs at DATA + g*H.
 *           Colours from LPALOFS: index = (LPALOFS<<1) | pixel.
 *   text32  4 bytes/cell: glyph lo, glyph hi (16-bit index), fg, bg --
 *           byte-wide palette indices per cell. bit7 of glyph hi = reverse.
 *
 * Layer 0 is bottom. Pixel index 0 is transparent in every layer except
 * the lowest enabled one (text32 bg is never transparent).
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
#define VR_SPRTAB   0x0A
#define VR_SPRCTL   0x0E
#define VR_COLSS    0x40
#define VR_COLSL    0x50
#define VICKE_SPRITES 128
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
#define VL_MODE_TEXT   2    /* text8  */
#define VL_MODE_TEXT32 3

void     vicke_reset(void);
uint8_t  vicke_read(uint8_t reg);
void     vicke_write(uint8_t reg, uint8_t v);
void     vicke_render(uint8_t *fb, int pitch);        /* one full frame */
uint32_t vicke_palette_rgb(int index);                /* 0x00RRGGBB */

#endif
