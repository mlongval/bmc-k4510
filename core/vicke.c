#include "vicke.h"
#include "mem.h"
#include <string.h>

static uint8_t font[256 * 8];

void vicke_init(void)
{
    memset(font, 0, sizeof font);
}

void vicke_set_font(const uint8_t *g)
{
    memcpy(font, g, sizeof font);
}

void vicke_palette(uint32_t *rgb, int n)
{
    /* Spike: two colours in the C64 spirit, the rest dark. */
    for (int i = 0; i < n; i++) rgb[i] = 0x000000;
    if (n > 0) rgb[0] = 0x352879;   /* VIC-II blue   -- background */
    if (n > 1) rgb[1] = 0x6C5EB5;   /* VIC-II lblue  -- text       */
}

void vicke_render(uint8_t *fb, int pitch)
{
    const uint8_t *screen = &k4510_ram[VICKE_SCREEN_BASE];
    for (int row = 0; row < VICKE_ROWS; row++) {
        for (int col = 0; col < VICKE_COLS; col++) {
            const uint8_t *g = &font[screen[row * VICKE_COLS + col] * 8];
            for (int y = 0; y < VICKE_CELL_H; y++) {
                uint8_t *p = fb + (row * VICKE_CELL_H + y) * pitch + col * VICKE_CELL_W;
                uint8_t bits = g[y];
                for (int x = 0; x < 8; x++)
                    p[x] = (bits & (0x80 >> x)) ? VICKE_FG : VICKE_BG;
            }
        }
    }
}
