/* BMC-K4510 demo: 15 bouncing balls. 128-sprite engine, 4 bpp sprites with
 * per-sprite palette offsets (one ball bitmap, 15 colour banks), sprite
 * table double-buffered by flipping SPRTAB, SHEILA sky gradient, text8
 * caption. 640x480. Every table lives in far memory (written with the
 * 45GS10 flat store); the program itself is 16 KB at $6000. Any key exits. */
#include "k4510.h"

#define N        15
#define BALLDATA 0x120000UL      /* 32x32 4 bpp = 512 bytes */
#define SPRTAB_A 0x121000UL      /* two 128 x 16 byte tables */
#define SPRTAB_B 0x122000UL
#define TEXTMAP  0x123000UL      /* 80 x 60 */
#define SHEILA   0x125000UL
#define SIZE     32

static int16_t px[N], py[N], vx[N], vy[N];      /* 12.4 fixed, pixels */
static uint8_t cur;

static void make_ball(void)
{
    uint32_t d = BALLDATA; uint8_t x, y;
    for (y = 0; y < SIZE; y++) for (x = 0; x < SIZE; x += 2) {
        uint8_t v[2], k;
        for (k = 0; k < 2; k++) {
            int16_t dx = (int16_t)(x + k) - 16, dy = (int16_t)y - 16;
            uint16_t r2 = dx * dx + dy * dy;
            if (r2 > 15 * 15) v[k] = 0;
            else {
                int16_t hx = dx + 6, hy = dy + 6;
                uint16_t h = hx * hx + hy * hy;
                v[k] = 15 - (h >> 5); if (v[k] < 2) v[k] = 2; if (v[k] > 15) v[k] = 15;
            }
        }
        far_poke(d++, (v[0] << 4) | v[1]);
    }
}

static void make_palette(void)
{
    static const uint8_t hue[15][3] = {
        {255,60,60},{255,150,40},{255,240,50},{120,255,60},{40,220,120},{40,230,230},{60,140,255},
        {120,80,255},{220,60,255},{255,80,180},{255,255,255},{200,160,120},{120,200,255},{255,200,120},{160,255,200} };
    uint8_t b, s;
    for (b = 0; b < N; b++) for (s = 0; s < 16; s++)
        pal((b + 1) * 16 + s, (uint8_t)(((uint16_t)hue[b][0] * s) / 15), (uint8_t)(((uint16_t)hue[b][1] * s) / 15), (uint8_t)(((uint16_t)hue[b][2] * s) / 15));
    pal(1, 255, 255, 255);
}

static void make_sheila(void)
{
    uint32_t p = SHEILA; uint8_t i;
    for (i = 0; i < 16; i++) pal(240 + i, 10 + i * 2, 20 + i * 4, 60 + i * 9);
    for (i = 0; i < 16; i++) {
        far_poke(p, 1); far_poke(p + 1, (uint8_t)(i * 30)); far_poke(p + 2, (i * 30) >> 8); far_poke(p + 3, 0); p += 4;   /* WAIT */
        far_poke(p, 2); far_poke(p + 1, 0x01); far_poke(p + 2, 240 + i); far_poke(p + 3, 0); p += 4;                     /* MOVE BGCOL */
    }
    far_poke(p, 1); far_poke(p + 1, (uint8_t)466); far_poke(p + 2, 466 >> 8); far_poke(p + 3, 0); p += 4;
    far_poke(p, 2); far_poke(p + 1, 0x01); far_poke(p + 2, 0x0B); far_poke(p + 3, 0); p += 4;                            /* floor */
    far_poke(p, 0);
    w32(V_SHEILA, SHEILA); REG(V_SHEILACTL) = 1;
}

static void write_table(uint32_t tab)
{
    uint8_t i;
    for (i = 0; i < N; i++, tab += 16) {
        far_poke16(tab, (uint16_t)(px[i] >> 4));
        far_poke16(tab + 2, (uint16_t)(py[i] >> 4));
    }
}

static void init_table(uint32_t tab)
{
    uint8_t i;
    dma_fill(0, tab, 2048);
    for (i = 0; i < N; i++, tab += 16) {
        far_poke16(tab + 4, (uint16_t)BALLDATA); far_poke16(tab + 6, (uint16_t)(BALLDATA >> 16));
        far_poke(tab + 8, 1);                    /* enable, 4 bpp, Z 0 */
        far_poke(tab + 9, 2 | (2 << 2));         /* 32 x 32 */
        far_poke(tab + 10, i + 1);               /* palette bank */
    }
}

void main(void)
{
    uint8_t i; uint16_t seed = 12345;
    REG(V_CTRL) = 0;
    make_ball(); make_palette(); make_sheila();
    for (i = 0; i < N; i++) {
        seed = seed * 25173 + 13849;
        px[i] = (int16_t)(((seed >> 4) % 600) << 4); seed = seed * 25173 + 13849;
        py[i] = (int16_t)(((seed >> 4) % 300) << 4); seed = seed * 25173 + 13849;
        vx[i] = (int16_t)((seed >> 4) % 100) - 50;
        vy[i] = 0;
    }
    dma_fill(' ', TEXTMAP, 80 * 60);
    text8_print(TEXTMAP, 80, 1, 0, "BMC-K4510  VICKe: 15 x 32x32 4bpp sprites, 15 palette banks, SHEILA sky");
    text8_print(TEXTMAP, 80, 1, 59, "any key returns to the shell        FPS ");
    text8_layer(0, TEXTMAP, 80, 0);
    init_table(SPRTAB_A); init_table(SPRTAB_B);
    write_table(SPRTAB_A); w32(V_SPRTAB, SPRTAB_A); REG(V_SPRCTL) = 1;
    REG(V_CTRL) = 1;
    while (!key_hit()) {
        uint32_t back = cur ? SPRTAB_A : SPRTAB_B;
        for (i = 0; i < N; i++) {
            vy[i] += 3;
            px[i] += vx[i]; py[i] += vy[i];
            if (px[i] < 0)            { px[i] = 0;            vx[i] = -vx[i]; }
            if (px[i] > (608 << 4))   { px[i] = 608 << 4;     vx[i] = -vx[i]; }
            if (py[i] > (434 << 4))   { py[i] = 434 << 4;     vy[i] = -vy[i] + (vy[i] >> 4); if (vy[i] > -8) vy[i] = -190; }
            if (py[i] < 0)            { py[i] = 0;            vy[i] = -vy[i]; }
        }
        write_table(back);
        wait_vblank();
        w32(V_SPRTAB, back); cur ^= 1;
        fps_tick(); put_num(TEXTMAP, 80, 41, 59, fps_value); fps_last = REG(SYS + 0x0D);
    }
}
