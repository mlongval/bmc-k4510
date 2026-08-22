/* VICKe -- the K4510 video chip. Spike version: one text layer.
 *
 * Screen RAM holds one byte per cell, an 8-bit glyph index (ASCII in
 * practice). The chip reads it from main RAM every frame and renders
 * into an 8-bit indexed framebuffer the frontend hands it. No VRAM, no
 * registers yet: geometry and pointers are fixed here for the spike.
 */
#ifndef K4510_VICKE_H
#define K4510_VICKE_H
#include <stdint.h>

#define VICKE_COLS       80
#define VICKE_ROWS       60
#define VICKE_CELL_W     8
#define VICKE_CELL_H     8
#define VICKE_WIDTH      (VICKE_COLS * VICKE_CELL_W)   /* 640 */
#define VICKE_HEIGHT     (VICKE_ROWS * VICKE_CELL_H)   /* 480 */

#define VICKE_SCREEN_BASE  0x0800u      /* 80*60 = 4800 bytes: $0800-$1AC0 */
#define VICKE_FG           1            /* palette index */
#define VICKE_BG           0

void vicke_init(void);
void vicke_set_font(const uint8_t *glyphs_256x8);   /* 256 glyphs * 8 rows, row = 8 px MSB-left */
void vicke_render(uint8_t *fb, int pitch);          /* fb: VICKE_WIDTH x VICKE_HEIGHT, 1 byte/px */
void vicke_palette(uint32_t *rgb, int n);           /* fill n entries of 0x00RRGGBB */

#endif
