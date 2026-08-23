/* BMC-K4510 demo: Mandelbrot set on the MATH unit. 320x240 (VICKe lowres),
 * 8 bpp bitmap at $200000, drawn row by row through a RAM buffer and DMA.
 * Every number is an IEEE single in one of the unit's eight registers at
 * $D700; an operation is two byte writes (FARG, FOP) and works in place,
 * so the inner loop is eleven register ops and one byte read. No table,
 * no fixed point, and the precision reaches eight zoom levels. Any key exits. */
#include "k4510.h"

#define W 320
#define H 240
#define FB   0x200000UL
#define TEXTMAP 0x123000UL
#define MAXIT 64
#define LEVELS 8

#define FOP(op, d, s)  do { REG(0xD721u) = (uint8_t)(((d) << 4) | (s)); REG(0xD720u) = (op); } while (0)
#define FI_LO          REG(0xD724u)
#define FI_SET(v)      w32(0xD724u, (uint32_t)(int32_t)(v))
/* registers: 0 x  1 y  2 cx  3 cy  4 x^2  5 y^2  6 tmp  7 step */

static uint8_t row[W];
static uint32_t cx0_bits;           /* cx at the left edge, saved as raw bits to reload F2 per row */

static uint8_t iterate(void)
{
    uint8_t i;
    /* interior shortcuts: main cardioid and the period-2 bulb, in F0/F1/F4/F5/F6 */
    FI_SET(1); FOP(MATH_ITOF, 6, 0);                  /* F6 = 1 */
    FOP(MATH_MOV, 4, 2); FOP(MATH_ADD, 4, 6);         /* F4 = cx + 1 */
    FOP(MATH_MUL, 4, 4);                              /* (cx+1)^2 */
    FOP(MATH_MOV, 5, 3); FOP(MATH_MUL, 5, 5);         /* F5 = cy^2 */
    FOP(MATH_ADD, 4, 5);                              /* F4 = (cx+1)^2 + cy^2 */
    FI_SET(16); FOP(MATH_ITOF, 6, 0); FOP(MATH_MUL, 4, 6);   /* x16: bulb if < 1 */
    FI_SET(1); FOP(MATH_ITOF, 6, 0); FOP(MATH_CMP, 4, 6);    /* F4 - 1 */
    if (REG(0xD722u) & 2) return 0;
    FI_SET(4); FOP(MATH_ITOF, 6, 0);                  /* F6 = 4 */
    FOP(MATH_MOV, 4, 2); FOP(MATH_MUL, 4, 6);         /* F4 = 4cx */
    FI_SET(1); FOP(MATH_ITOF, 0, 0); FOP(MATH_SUB, 4, 0);   /* 4cx - 1  (= 4(x - 1/4)) */
    FOP(MATH_MUL, 4, 4);                              /* 16 (x-1/4)^2 */
    FOP(MATH_MOV, 0, 5); FOP(MATH_MUL, 0, 6); FOP(MATH_MUL, 0, 6);   /* F0 = 16 y^2 */
    FOP(MATH_ADD, 4, 0);                              /* F4 = 16 q, q = (x-1/4)^2 + y^2 */
    FOP(MATH_MOV, 1, 2); FOP(MATH_MUL, 1, 6); FI_SET(1); FOP(MATH_ITOF, 6, 0); FOP(MATH_SUB, 1, 6);   /* F1 = 4(x - 1/4) */
    /* cardioid: q (q + x - 1/4) < y^2/4  <=>  16q (16q + 16(x-1/4)) < 64 y^2 */
    FOP(MATH_MOV, 6, 1); FI_SET(4); FOP(MATH_ITOF, 0, 0); FOP(MATH_MUL, 6, 0); FOP(MATH_ADD, 6, 4);  /* F6 = 16q + 16(x-1/4) */
    FOP(MATH_MUL, 6, 4);                              /* F6 = 16q (16q + 16(x-1/4)) = 256 q (q + x - 1/4) */
    FOP(MATH_MOV, 0, 5); FI_SET(64); FOP(MATH_ITOF, 1, 0); FOP(MATH_MUL, 0, 1);                    /* F0 = 64 y^2 = 256 (y^2/4) */
    FOP(MATH_CMP, 6, 0);
    if (REG(0xD722u) & 2) return 0;

    FOP(MATH_MOV, 0, 2); FOP(MATH_MOV, 1, 3);         /* z = c */
    for (i = 1; i < MAXIT; i++) {
        FOP(MATH_MOV, 4, 0); FOP(MATH_MUL, 4, 0);     /* x^2 */
        FOP(MATH_MOV, 5, 1); FOP(MATH_MUL, 5, 1);     /* y^2 */
        FOP(MATH_MOV, 6, 4); FOP(MATH_ADD, 6, 5);     /* |z|^2 */
        FOP(MATH_FTOI, 0, 6);
        if (FI_LO >= 4) return i;                     /* escaped (|z|^2 >= 4; the int is small here) */
        FOP(MATH_MUL, 1, 0); FOP(MATH_ADD, 1, 1); FOP(MATH_ADD, 1, 3);   /* y = 2xy + cy */
        FOP(MATH_MOV, 0, 4); FOP(MATH_SUB, 0, 5); FOP(MATH_ADD, 0, 2);   /* x = x^2 - y^2 + cx */
    }
    return 0;
}

static uint8_t render(void)
{
    uint16_t px; uint8_t py;
    for (py = 0; py < H; py++) {
        w32(0xD708u, cx0_bits);                       /* F2 = cx at the left edge */
        for (px = 0; px < W; px++) { row[px] = iterate(); FOP(MATH_ADD, 2, 7); }   /* cx += step */
        FOP(MATH_ADD, 3, 7);                          /* cy += step */
        dma_copy((uint32_t)(uint16_t)row, FB + (uint32_t)py * W, W);
        if (key_hit()) return 1;
    }
    return 0;
}

static void set_palette(uint8_t phase)
{
    uint8_t i;
    for (i = 1; i < MAXIT; i++) {
        uint8_t t = (uint8_t)((i + phase) * 4);
        uint8_t r = t < 128 ? t * 2 : (255 - t) * 2;
        uint8_t g = (uint8_t)(t + 85) < 128 ? (uint8_t)(t + 85) * 2 : (255 - (uint8_t)(t + 85)) * 2;
        uint8_t b = (uint8_t)(t + 170) < 128 ? (uint8_t)(t + 170) * 2 : (255 - (uint8_t)(t + 170)) * 2;
        pal(i, r, g, b);
    }
}

/* view for a zoom level: centre (cxc, cyc) in 1/4096 units, step = 3/320 / 2^level */
static void set_view(int16_t cxc, int16_t cyc, uint8_t level)
{
    FI_SET(3); FOP(MATH_ITOF, 7, 0);
    FI_SET(320L << level); FOP(MATH_ITOF, 6, 0); FOP(MATH_DIV, 7, 6);           /* F7 = step */
    FI_SET(4096); FOP(MATH_ITOF, 6, 0);
    FI_SET(cxc); FOP(MATH_ITOF, 2, 0); FOP(MATH_DIV, 2, 6);                      /* F2 = centre x */
    FI_SET(cyc); FOP(MATH_ITOF, 3, 0); FOP(MATH_DIV, 3, 6);                      /* F3 = centre y */
    FI_SET(160); FOP(MATH_ITOF, 6, 0); FOP(MATH_MUL, 6, 7); FOP(MATH_SUB, 2, 6); /* left edge */
    FI_SET(120); FOP(MATH_ITOF, 6, 0); FOP(MATH_MUL, 6, 7); FOP(MATH_SUB, 3, 6); /* top edge */
    cx0_bits = (uint32_t)REG(0xD708u) | ((uint32_t)REG(0xD709u) << 8) | ((uint32_t)REG(0xD70Au) << 16) | ((uint32_t)REG(0xD70Bu) << 24);
}

void main(void)
{
    uint8_t level = 0, phase = 0, k;
    REG(V_CTRL) = 0;
    REG(V_BGCOL) = 0;
    pal(255, 255, 255, 255);
    set_palette(0);
    {   uint16_t L = V_LAYER(0);
        REG(L + 1) = 0; w16(L + 2, 0); w16(L + 4, 0); w16(L + 6, W); w32(L + 8, FB); w32(L + 12, 0);
        REG(L) = 1 | (0 << 1) | (3 << 3);
    }
    dma_fill(0, FB, (uint32_t)W * H);
    dma_fill(' ', TEXTMAP, 40 * 30);
    text8_layer(1, TEXTMAP, 40, 127);
    text8_print(TEXTMAP, 40, 1, 29, "any key returns to the shell");
    REG(V_CTRL) = 1 | 2;
    for (;;) {
        uint8_t f0, f1, secs;
        text8_print(TEXTMAP, 40, 1, 0, "BMC-K4510 Mandelbrot FPU  zoom ");
        far_poke(TEXTMAP + 32, '0' + level);
        if (level == 0) set_view(-2458, 0, 0); else set_view(-3052, 410, level);      /* -0.6,0 then -0.745,0.1 */
        f0 = REG(SYS + 0x0D); f1 = REG(SYS + 0x0E);
        if (render()) return;
        secs = (uint8_t)((((uint16_t)REG(SYS + 0x0E) << 8 | REG(SYS + 0x0D)) - ((uint16_t)f1 << 8 | f0)) / 60);
        text8_print(TEXTMAP, 40, 34, 0, "   s"); put_num(TEXTMAP, 40, 34, 0, secs);
        for (k = 0; k < 180; k++) { wait_vblank(); set_palette(++phase); if (key_hit()) return; }
        if (++level == LEVELS) level = 0;
    }
}
