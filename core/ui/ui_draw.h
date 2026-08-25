/* Text-cell drawing into the menu overlay: an 8-bit 640x480 buffer, 0 =
 * see through to the (dimmed) machine picture, 1-7 the UI's colours
 * (ui_palette_rgb). The font is the host's 8x8 chargen for the menu
 * (data/fonts/unscii/font8-unscii.bin, CP437 layout), never the
 * machine's: the menu must draw when the guest has wrecked everything. */
#ifndef K4510_UI_DRAW_H
#define K4510_UI_DRAW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define UI_W 640
#define UI_H 480
#define UI_COLS (UI_W / 8)
#define UI_ROWS (UI_H / 8)
enum { UIC_NONE, UIC_PANEL, UIC_FRAME, UIC_TEXT, UIC_TITLE, UIC_DIM, UIC_BAR, UIC_BARTEXT, UIC_COUNT };
void     ui_font(const uint8_t *font2048);            /* 256 glyphs x 8 rows, MSB left */
void     ui_clear(uint8_t *ov);
void     ui_fill(uint8_t *ov, int cx, int cy, int w, int h, uint8_t c);
void     ui_text(uint8_t *ov, int cx, int cy, uint8_t fg, uint8_t bg, const char *s);
void     ui_box(uint8_t *ov, int cx, int cy, int w, int h, uint8_t fg, uint8_t bg);   /* double line, filled */
uint32_t ui_palette_rgb(int c);                       /* 0x00RRGGBB */
#ifdef __cplusplus
}
#endif
#endif
