/* K4510 demo: Mandelbrot set on the MATH unit. 320x240 (VICKY lowres),
 * 8 bpp bitmap at $200000, drawn row by row through a RAM buffer and DMA.
 * Every number is an IEEE single in one of the unit's eight registers at
 * $D700; an operation is two byte writes (FARG, FOP) and works in place,
 * and the whole iteration is a *math list* the unit runs by itself: per
 * pixel the CPU writes one byte and reads two. No table, no fixed point,
 * eight zoom levels. Any key exits. */
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

/* The iteration as a math list: the unit runs it, the CPU writes one byte
 * per pixel and reads back how many passes it took. F2/F3 hold c. */
static uint8_t mlist[] = {
    MATH_MOV, 0x02, MATH_MOV, 0x13,                         /* z = c */
    MATH_MOV, 0x40, MATH_MUL, 0x40,                         /* loop: F4 = x^2 */
    MATH_MOV, 0x51, MATH_MUL, 0x51,                         /* F5 = y^2 */
    MATH_MOV, 0x64, MATH_ADD, 0x65, MATH_FTOI, 0x06,        /* FI = int(|z|^2) */
    ML_STOPFIGE, 4,                                         /* escaped? */
    MATH_MUL, 0x10, MATH_ADD, 0x11, MATH_ADD, 0x13,         /* y = 2xy + cy */
    MATH_MOV, 0x04, MATH_SUB, 0x05, MATH_ADD, 0x02,         /* x = x^2 - y^2 + cx */
    ML_DJNZ, (uint8_t)-15,                                  /* 15 ops back (from the op after DJNZ) to the loop */
    ML_END, 0 };

static uint8_t iterate(void)
{
    REG(0xD72Eu) = MAXIT; REG(0xD72Fu) = 0;
    REG(0xD72Cu) = 1;
    return REG(0xD72Du) ? (uint8_t)(MAXIT - REG(0xD72Eu)) : 0;      /* stopped: iterations taken; ran out: inside */
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
    w32(0xD728u, (uint16_t)mlist);                            /* the unit's program */
    REG(V_CTRL) = 1 | 2;
    for (;;) {
        uint8_t f0, f1, secs;
        text8_print(TEXTMAP, 40, 1, 0, "K4510 Mandelbrot mathlist zoom ");
        far_poke(TEXTMAP + 36, '0' + level);
        if (level == 0) set_view(-2458, 0, 0); else set_view(-3052, 410, level);      /* -0.6,0 then -0.745,0.1 */
        f0 = REG(SYS + 0x0D); f1 = REG(SYS + 0x0E);
        if (render()) return;
        secs = (uint8_t)((((uint16_t)REG(SYS + 0x0E) << 8 | REG(SYS + 0x0D)) - ((uint16_t)f1 << 8 | f0)) / 60);
        text8_print(TEXTMAP, 40, 36, 1, "   s"); put_num(TEXTMAP, 40, 36, 1, secs);
        for (k = 0; k < 180; k++) { wait_vblank(); set_palette(++phase); if (key_hit()) return; }
        if (++level == LEVELS) level = 0;
    }
}
