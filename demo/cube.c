/* BMC-K4510 demo: rotating cube, wireframe then solid with hidden faces
 * removed. 320x240 (VICKe lowres), 8 bpp bitmap, two frame buffers in far
 * RAM ($110000 / $130000). Pixels go out with the 45GS02 flat store,
 * spans with DMA fill, the clear with DMA fill, and the flip is one write
 * to the layer's DATA pointer. Any key exits. */
#include "k4510.h"

#define W 320
#define H 240
#define BUF0 0x110000UL
#define BUF1 0x130000UL
#define TEXTMAP 0x123000UL

static const int16_t sintab[256] = {
    0,6,13,19,25,31,38,44,50,56,62,68,74,80,86,92,98,104,109,115,121,126,132,137,142,147,152,157,162,167,172,177,
    181,185,190,194,198,202,206,209,213,216,220,223,226,229,231,234,237,239,241,243,245,247,248,250,251,252,253,254,255,255,256,256,
    256,256,256,255,255,254,253,252,251,250,248,247,245,243,241,239,237,234,231,229,226,223,220,216,213,209,206,202,198,194,190,185,
    181,177,172,167,162,157,152,147,142,137,132,126,121,115,109,104,98,92,86,80,74,68,62,56,50,44,38,31,25,19,13,6,
    0,-6,-13,-19,-25,-31,-38,-44,-50,-56,-62,-68,-74,-80,-86,-92,-98,-104,-109,-115,-121,-126,-132,-137,-142,-147,-152,-157,-162,-167,-172,-177,
    -181,-185,-190,-194,-198,-202,-206,-209,-213,-216,-220,-223,-226,-229,-231,-234,-237,-239,-241,-243,-245,-247,-248,-250,-251,-252,-253,-254,-255,-255,-256,-256,
    -256,-256,-256,-255,-255,-254,-253,-252,-251,-250,-248,-247,-245,-243,-241,-239,-237,-234,-231,-229,-226,-223,-220,-216,-213,-209,-206,-202,-198,-194,-190,-185,
    -181,-177,-172,-167,-162,-157,-152,-147,-142,-137,-132,-126,-121,-115,-109,-104,-98,-92,-86,-80,-74,-68,-62,-56,-50,-44,-38,-31,-25,-19,-13,-6 };
#define SIN(a) sintab[(uint8_t)(a)]
#define COS(a) sintab[(uint8_t)((a) + 64)]

static const int8_t  vert[8][3] = { {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1} };
static const uint8_t face[6][4] = { {0,3,2,1},{4,5,6,7},{0,1,5,4},{3,7,6,2},{0,4,7,3},{1,2,6,5} };
static const uint8_t edge[12][2] = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };
static const uint8_t facecol[6] = { 2, 3, 4, 5, 6, 7 };
#define R 56

static int16_t sx[8], sy[8];
static int16_t xl[H], xr[H];
static uint8_t a, b, c, back;
static uint32_t buf;

static void plot(int16_t x, int16_t y, uint8_t col)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    far_poke(buf + (uint32_t)y * W + x, col);
}

static void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t col)
{
    int16_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int16_t sxs = x0 < x1 ? 1 : -1, sys = y0 < y1 ? 1 : -1, err = dx - dy, e2;
    for (;;) {
        plot(x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 += sxs; }
        if (e2 <  dx) { err += dx; y0 += sys; }
    }
}

static void span(int16_t y, int16_t x0, int16_t x1, uint8_t col)
{
    if (x0 > x1 || y < 0 || y >= H) return;
    if (x0 < 0) x0 = 0; if (x1 >= W) x1 = W - 1;
    dma_fill(col, buf + (uint32_t)y * W + x0, (uint16_t)(x1 - x0 + 1));
}

static void edge_walk(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    long xa; long step; int16_t y, t;
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y0 == y1) { if (x0 < xl[y0]) xl[y0] = x0; if (x1 > xr[y0]) xr[y0] = x1; if (x1 < xl[y0]) xl[y0] = x1; if (x0 > xr[y0]) xr[y0] = x0; return; }
    xa = (long)x0 << 8; step = ((long)(x1 - x0) << 8) / (y1 - y0);
    for (y = y0; y <= y1; y++, xa += step) {
        int16_t x = (int16_t)(xa >> 8);
        if (x < xl[y]) xl[y] = x;
        if (x > xr[y]) xr[y] = x;
    }
}

static void fill_quad(const uint8_t *f, uint8_t col)
{
    int16_t ymin = H, ymax = -1, y; uint8_t i;
    for (i = 0; i < 4; i++) { if (sy[f[i]] < ymin) ymin = sy[f[i]]; if (sy[f[i]] > ymax) ymax = sy[f[i]]; }
    for (y = ymin; y <= ymax; y++) { xl[y] = W; xr[y] = -1; }
    for (i = 0; i < 4; i++) edge_walk(sx[f[i]], sy[f[i]], sx[f[(i + 1) & 3]], sy[f[(i + 1) & 3]]);
    for (y = ymin; y <= ymax; y++) span(y, xl[y], xr[y], col);
}

static void transform(void)
{
    uint8_t i;
    int16_t sa = SIN(a), ca = COS(a), sb = SIN(b), cb = COS(b), sc = SIN(c), cc = COS(c);
    for (i = 0; i < 8; i++) {
        int16_t x = vert[i][0] * R, y = vert[i][1] * R, z = vert[i][2] * R, t;
        t = (y * ca - z * sa) >> 8; z = (y * sa + z * ca) >> 8; y = t;
        t = (x * cb + z * sb) >> 8; z = (z * cb - x * sb) >> 8; x = t;
        t = (x * cc - y * sc) >> 8; y = (x * sc + y * cc) >> 8; x = t;
        z += 300;
        sx[i] = 160 + (int16_t)(((long)x * 230) / z);
        sy[i] = 120 + (int16_t)(((long)y * 230) / z);
    }
}

static uint8_t visible(const uint8_t *f)
{
    long cr = (long)(sx[f[1]] - sx[f[0]]) * (sy[f[2]] - sy[f[0]]) - (long)(sy[f[1]] - sy[f[0]]) * (sx[f[2]] - sx[f[0]]);
    return cr < 0;
}

void main(void)
{
    uint16_t frame = 0; uint8_t i, j, solid = 0;
    REG(V_CTRL) = 0;
    pal(1, 255, 255, 255);
    pal(2, 220, 40, 40); pal(3, 40, 200, 60); pal(4, 50, 90, 240); pal(5, 240, 200, 40); pal(6, 200, 60, 220); pal(7, 40, 210, 210);
    REG(V_BGCOL) = 0;
    {   uint16_t L = V_LAYER(0);
        REG(L + 1) = 0; w16(L + 2, 0); w16(L + 4, 0); w16(L + 6, W); w32(L + 8, BUF0); w32(L + 12, 0);
        REG(L) = 1 | (0 << 1) | (3 << 3);                               /* enable, bitmap, 8 bpp */
    }
    dma_fill(' ', TEXTMAP, 40 * 30);
    text8_print(TEXTMAP, 40, 1, 0, "BMC-K4510 cube: flat stores, DMA spans,");
    text8_print(TEXTMAP, 40, 1, 1, "double buffered by one pointer write");
    text8_print(TEXTMAP, 40, 1, 29, "any key returns to the shell");
    text8_layer(1, TEXTMAP, 40, 0);
    dma_fill(0, BUF0, (uint32_t)W * H);
    REG(V_CTRL) = 1 | 2;                                                /* display on, lowres */
    back = 1;
    while (!key_hit()) {
        buf = back ? BUF1 : BUF0;
        dma_fill(0, buf, (uint32_t)W * H);
        a += 1; b += 2; c += 1;
        transform();
        if (!solid) {
            for (i = 0; i < 12; i++) line(sx[edge[i][0]], sy[edge[i][0]], sx[edge[i][1]], sy[edge[i][1]], 1);
        } else {
            for (i = 0; i < 6; i++) if (visible(face[i])) {
                fill_quad(face[i], facecol[i]);
                for (j = 0; j < 4; j++) line(sx[face[i][j]], sy[face[i][j]], sx[face[i][(j + 1) & 3]], sy[face[i][(j + 1) & 3]], 1);
            }
        }
        wait_vblank();
        w32(V_LAYER(0) + 8, buf);
        back ^= 1;
        if (++frame == 300) { frame = 0; solid ^= 1; }
    }
}
