/* BMC-K4510 demo: Mandelbrot set. 320x240 (VICKe lowres), 8 bpp bitmap at
 * $200000, rendered row by row into a RAM buffer and DMA'd out so you watch
 * it draw. No multiplier on a 45GS02: squares come from a 32 KB table of
 * 4.12 fixed-point squares built at start and reached through the MAP
 * window, and 2xy = (x+y)^2 - x^2 - y^2. Then palette cycling, then zoom. */
#include <string.h>
#include "k4510.h"

#define W 320
#define H 240
#define FB   0x200000UL
#define SQT  0x110000UL                 /* 16385 x uint16: (v*v)>>12 for v = -8192..8192 */
#define TEXTMAP 0x1000u                 /* 40 x 30 = 1200 bytes */
#define MAXIT 40

static uint8_t row[W];
static uint16_t *sq = (uint16_t *)0x2000;
#define SQ(v) sq[(uint16_t)((v) + 8192)]

static void build_table(void)
{
    uint16_t i; int16_t v;
    map_window(SQT);
    for (i = 0; i <= 16384; i++) { v = (int16_t)i - 8192; sq[i] = (uint16_t)(((long)v * v) >> 12); }
}

static uint8_t iterate(int16_t cx, int16_t cy)
{
    int16_t x = cx, y = cy, x2, y2; uint8_t i;
    /* interior shortcuts, all in 4.12 through the square table:
     * period-2 bulb: (x+1)^2 + y^2 < 1/16
     * main cardioid: q = (x-1/4)^2 + y^2;  q*(q + x - 1/4) < y^2/4 */
    if (cx > -8192 && cx < 4096) {
        uint16_t q = (uint16_t)SQ((int16_t)(cx + 4096)) + (uint16_t)SQ(cy);
        if (q < 256) return 0;
        q = (uint16_t)SQ((int16_t)(cx - 1024)) + (uint16_t)SQ(cy);
        if (cx > -7168 && q < 4096) {                                      /* keep cx-1/4 inside the table */
            int16_t a = (int16_t)(q >> 6), b = (int16_t)((q + cx - 1024) >> 6);   /* 4.6 each; product 8.12 */
            if (a * b < (int16_t)(SQ(cy) >> 2)) return 0;
        }
    }
    for (i = 1; i < MAXIT; i++) {
        if (x > 8192 || x < -8192 || y > 8192 || y < -8192) return i;    /* |re| or |im| > 2: escaped */
        x2 = SQ(x); y2 = SQ(y);
        if ((uint16_t)x2 + (uint16_t)y2 > 16384) return i;
        y = (int16_t)(((uint16_t)SQ((int16_t)((x + y) >> 1)) << 2) - (uint16_t)x2 - (uint16_t)y2 + (uint16_t)cy);
        x = x2 - y2 + cx;
    }
    return 0;                                                          /* inside: index 0 = BGCOL black */
}

static void render(int16_t cx0, int16_t cy0, int16_t step)
{
    uint16_t px; uint8_t py; int16_t cx, cy = cy0;
    for (py = 0; py < H; py++, cy += step) {
        cx = cx0;
        for (px = 0; px < W; px++, cx += step) row[px] = iterate(cx, cy);
        dma_copy((uint32_t)row, FB + (uint32_t)py * W, W);
    }
}

static void set_palette(uint8_t phase)
{
    uint8_t i;
    for (i = 1; i < MAXIT; i++) {
        uint8_t t = (uint8_t)((i + phase) * 6);                         /* 0..255 wraps */
        uint8_t r = t < 128 ? t * 2 : (255 - t) * 2;
        uint8_t g = (uint8_t)(t + 85) < 128 ? (uint8_t)(t + 85) * 2 : (255 - (uint8_t)(t + 85)) * 2;
        uint8_t b = (uint8_t)(t + 170) < 128 ? (uint8_t)(t + 170) * 2 : (255 - (uint8_t)(t + 170)) * 2;
        pal(i, r, g, b);
    }
}

void main(void)
{
    static const int16_t ccx[5] = { -2458, -3052, -3052, -3052, -3052 };   /* 4.12: -0.6, then -0.745 */
    static const int16_t ccy[5] = {     0,   410,   410,   410,   410 };   /* 0, then 0.1 */
    uint8_t level = 0, phase = 0, k;
    REG(V_CTRL) = 0;
    REG(V_BGCOL) = 0;
    pal(255, 255, 255, 255);
    build_table();
    set_palette(0);
    {   uint16_t L = V_LAYER(0);
        REG(L + 1) = 0; w16(L + 2, 0); w16(L + 4, 0); w16(L + 6, W); w32(L + 8, FB); w32(L + 12, 0);
        REG(L) = 1 | (0 << 1) | (3 << 3);                               /* enable, bitmap, 8 bpp */
    }
    memset((void *)TEXTMAP, ' ', 40 * 30);
    text8_layer(1, TEXTMAP, 40, 127);                                    /* colour index 255 */
    REG(V_CTRL) = 1 | 2;
    for (;;) {
        int16_t step = 38 >> level;                                     /* 3.0 / 320 in 4.12, halved per level */
        memset((void *)TEXTMAP, ' ', 40);
        text8_print(TEXTMAP, 40, 1, 0, "BMC-K4510 Mandelbrot  zoom level ");
        ((uint8_t *)TEXTMAP)[34] = '0' + level;
        render(ccx[level] - 160 * step, ccy[level] - 120 * step, step);
        for (k = 0; k < 180; k++) { wait_vblank(); set_palette(++phase); }
        if (++level == 5) level = 0;
    }
}
