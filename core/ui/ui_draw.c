#include "ui_draw.h"
#include <string.h>
static const uint8_t *font;
static uint8_t blank_font[2048];
static int chh = 8;                                   /* cell height: 8 (60 rows) or 16 (30) */
int  ui_cell_h(int h) { int n = (h >= 16) ? 16 : 8, was = chh; chh = n; return n != was; }
int  ui_rows(void)    { return UI_H / chh; }
/* the machine's palette (the C64 set the ROM installs), by name -- fixed here on purpose */
static const uint32_t rgb[UIC_COUNT] = { 0, 0x0000AA, 0x0088FF, 0xFFFFFF, 0xEEEE77, 0x777777, 0x0088FF, 0x000000 };
uint32_t ui_palette_rgb(int c) { return (c > 0 && c < UIC_COUNT) ? rgb[c] : 0; }
void ui_font(const uint8_t *f) { font = f ? f : blank_font; }
void ui_clear(uint8_t *ov) { memset(ov, 0, UI_W * UI_H); }
void ui_fill(uint8_t *ov, int cx, int cy, int w, int h, uint8_t c)
{
    for (int y = cy * chh; y < (cy + h) * chh && y < UI_H; y++) if (y >= 0) memset(ov + y * UI_W + cx * 8, c, (size_t)(w * 8));
}
static void glyph(uint8_t *ov, int cx, int cy, uint8_t ch, uint8_t fg, uint8_t bg)
{
    if (!font) font = blank_font;
    if (cx < 0 || cy < 0 || cx >= UI_COLS || cy >= UI_ROWS) return;
    const uint8_t *g = font + ch * 8;
    for (int r = 0; r < chh; r++) {                   /* chh 16: every glyph row drawn twice */
        uint8_t bits = g[r * 8 / chh], *p = ov + (cy * chh + r) * UI_W + cx * 8;
        for (int b = 0; b < 8; b++) p[b] = (bits & (0x80 >> b)) ? fg : bg;
    }
}
void ui_text(uint8_t *ov, int cx, int cy, uint8_t fg, uint8_t bg, const char *s) { while (*s) glyph(ov, cx++, cy, (uint8_t) *s++, fg, bg); }
void ui_box(uint8_t *ov, int cx, int cy, int w, int h, uint8_t fg, uint8_t bg)
{
    ui_fill(ov, cx, cy, w, h, bg);
    for (int x = 1; x < w - 1; x++) { glyph(ov, cx + x, cy, 0xCD, fg, bg); glyph(ov, cx + x, cy + h - 1, 0xCD, fg, bg); }
    for (int y = 1; y < h - 1; y++) { glyph(ov, cx, cy + y, 0xBA, fg, bg); glyph(ov, cx + w - 1, cy + y, 0xBA, fg, bg); }
    glyph(ov, cx, cy, 0xC9, fg, bg); glyph(ov, cx + w - 1, cy, 0xBB, fg, bg);
    glyph(ov, cx, cy + h - 1, 0xC8, fg, bg); glyph(ov, cx + w - 1, cy + h - 1, 0xBC, fg, bg);
}
