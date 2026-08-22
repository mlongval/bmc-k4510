#include "vicke.h"
#include "mem.h"
#include <string.h>

static uint8_t  reg[256];
static uint32_t pal[256];
static int      cur_line;
static uint8_t  col_ss[16], col_sl[16];     /* collision accumulators for the frame in progress */
static uint8_t  owner[VICKE_WIDTH];          /* per-pixel: 0 = layers only, else sprite n+1 */
static uint8_t  layer_hit[VICKE_WIDTH];      /* per-pixel: a layer drew a non-zero index here */

static const uint32_t c64_palette[16] = {   /* VIC-II colours as the first 16 entries (spec §2) */
    0x000000, 0xFFFFFF, 0x880000, 0xAAFFEE, 0xCC44CC, 0x00CC55, 0x0000AA, 0xEEEE77,
    0xDD8855, 0x664400, 0xFF7777, 0x333333, 0x777777, 0xAAFF66, 0x0088FF, 0xBBBBBB,
};

void vicke_reset(void)
{
    memset(reg, 0, sizeof reg);
    for (int i = 0; i < 256; i++) pal[i] = (i < 16) ? c64_palette[i] : (uint32_t)(i * 0x010101);
    cur_line = 0;
}

uint32_t vicke_palette_rgb(int i) { return pal[i & 0xFF]; }

uint8_t vicke_read(uint8_t r)
{
    switch (r) {
    case VR_RASTER:     return cur_line & 0xFF;
    case VR_RASTER + 1: return cur_line >> 8;
    default:
        if (r >= VR_COLSS && r < VR_COLSS + 16) { uint8_t v = reg[r]; if (r == VR_COLSS) memset(&reg[VR_COLSS], 0, 16); return v; }
        if (r >= VR_COLSL && r < VR_COLSL + 16) { uint8_t v = reg[r]; if (r == VR_COLSL) memset(&reg[VR_COLSL], 0, 16); return v; }
        return reg[r];
    }
}

void vicke_write(uint8_t r, uint8_t v)
{
    reg[r] = v;
    if (r == VR_PALB) {
        uint8_t i = reg[VR_PALIDX];
        pal[i] = ((uint32_t)reg[VR_PALR] << 16) | ((uint32_t)reg[VR_PALG] << 8) | v;
        reg[VR_PALIDX] = i + 1;
    }
}

static inline uint32_t rd32(const uint8_t *p) { return (p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)) & K4510_PHYS_MASK; }
static inline uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static inline uint8_t  ram(uint32_t a) { return k4510_ram[a & K4510_PHYS_MASK]; }
#define ram_ptr(a) k4510_ram[(a) & K4510_PHYS_MASK]

/* Render one scanline of one layer into line[], honouring transparency.
 * opaque: this is the lowest enabled layer, so index 0 is drawn too. */
static void layer_line(int n, int y, uint8_t *line, int opaque)
{
    const uint8_t *L = &reg[VR_LAYER(n)];
    int mode  = (L[VL_CTRL] >> 1) & 3;
    int depth = (L[VL_CTRL] >> 3) & 3;          /* 0..3 -> 1,2,4,8 bpp */
    int csz   = (L[VL_CTRL] >> 5) & 3;          /* cell size field */
    int bpp   = 1 << depth;
    uint8_t  palofs = L[VL_PALOFS];
    uint32_t data   = rd32(&L[VL_DATA]);
    uint32_t map    = rd32(&L[VL_MAP]);
    uint16_t stride = rd16(&L[VL_STRIDE]);
    int sy  = y + rd16(&L[VL_SCROLLY]);
    int sx0 = rd16(&L[VL_SCROLLX]);
    uint8_t mask = (uint8_t)((1 << bpp) - 1);

    if (mode == VL_MODE_BITMAP) {
        int ppb = 8 / bpp;
        uint8_t base = (uint8_t)(palofs << bpp);
        uint32_t row = data + (uint32_t)sy * stride;
        for (int x = 0; x < VICKE_WIDTH; x++) {
            int sx = x + sx0;
            uint8_t b = ram(row + sx / ppb);
            int shift = (bpp == 8) ? 0 : (8 - bpp - (sx % ppb) * bpp);
            uint8_t pix = (b >> shift) & mask;
            if (pix || opaque) { line[x] = (bpp == 8) ? pix : (uint8_t)(base | pix); if (pix) layer_hit[x] = 1; }
        }
        return;
    }
    if (mode == VL_MODE_TILE) {
        int size = 8 << csz;                               /* 8,16,32,64 */
        int tbytes = size * size * bpp / 8;
        int rowbytes = size * bpp / 8;
        int cy = sy / size, ty = sy % size;
        for (int x = 0; x < VICKE_WIDTH; ) {
            int sx = x + sx0;
            int cx = sx / size, tx0 = sx % size;
            uint32_t e = map + ((uint32_t)cy * stride + cx) * 2;
            uint16_t ent = ram(e) | (ram(e + 1) << 8);
            int idx = ent & 0x3FF, hf = ent & 0x400, vf = ent & 0x800;
            uint8_t base = (uint8_t)((ent >> 12) << bpp);
            int ry = vf ? (size - 1 - ty) : ty;
            uint32_t trow = data + (uint32_t)idx * tbytes + (uint32_t)ry * rowbytes;
            for (int tx = tx0; tx < size && x < VICKE_WIDTH; tx++, x++) {
                int px = hf ? (size - 1 - tx) : tx;
                uint8_t b = ram(trow + px * bpp / 8);
                int shift = (bpp == 8) ? 0 : (8 - bpp - (px % (8 / bpp)) * bpp);
                uint8_t pix = (b >> shift) & mask;
                if (pix || opaque) { line[x] = (bpp == 8) ? pix : (uint8_t)(base | pix); if (pix) layer_hit[x] = 1; }
            }
        }
        return;
    }
    /* text modes: 1-bpp glyphs, 8 px wide, H = 8 or 16 rows */
    int H = csz ? 16 : 8;
    int cy = sy / H, gy = sy % H;
    if (mode == VL_MODE_TEXT) {
        uint8_t base = (uint8_t)(palofs << 1);
        for (int x = 0; x < VICKE_WIDTH; ) {
            int sx = x + sx0, cx = sx >> 3, gx0 = sx & 7;
            uint8_t cell = ram(map + (uint32_t)cy * stride + cx);
            uint8_t row  = ram(data + (uint32_t)cell * H + gy);
            for (int gx = gx0; gx < 8 && x < VICKE_WIDTH; gx++, x++) {
                uint8_t pix = (row >> (7 - gx)) & 1;
                if (pix || opaque) { line[x] = (uint8_t)(base | pix); if (pix) layer_hit[x] = 1; }
            }
        }
        return;
    }
    /* text32 */
    for (int x = 0; x < VICKE_WIDTH; ) {
        int sx = x + sx0, cx = sx >> 3, gx0 = sx & 7;
        uint32_t e = map + ((uint32_t)cy * stride + cx) * 4;
        uint16_t g = ram(e) | ((ram(e + 1) & 0x7F) << 8);
        int rev = ram(e + 1) & 0x80;
        uint8_t fg = ram(e + 2), bg = ram(e + 3);
        if (rev) { uint8_t t = fg; fg = bg; bg = t; }
        uint8_t row = ram(data + (uint32_t)g * H + gy);
        for (int gx = gx0; gx < 8 && x < VICKE_WIDTH; gx++, x++)
            line[x] = ((row >> (7 - gx)) & 1) ? fg : bg;
        layer_hit[x] = 1;
    }
}

/* Draw every enabled sprite with Z == z that covers line y. */
static void sprites_line(int z, int y, uint8_t *line)
{
    if (!(reg[VR_SPRCTL] & 1)) return;
    uint32_t tab = rd32(&reg[VR_SPRTAB]);
    for (int n = 0; n < VICKE_SPRITES; n++) {
        uint32_t e = tab + (uint32_t)n * 16;
        uint8_t ctrl = ram(e + 8);
        if (!(ctrl & 1) || ((ctrl >> 4) & 3) != z) continue;
        int16_t sxp = (int16_t)(ram(e) | (ram(e + 1) << 8));
        int16_t syp = (int16_t)(ram(e + 2) | (ram(e + 3) << 8));
        uint8_t size = ram(e + 9);
        int w = 8 << (size & 3), h = 8 << ((size >> 2) & 3);
        int ry = y - syp;
        if (ry < 0 || ry >= h) continue;
        int bpp = (ctrl & 2) ? 8 : 4;
        int rowbytes = w * bpp / 8;
        uint32_t data = rd32(&ram_ptr(e + 4));
        if (ctrl & 8) ry = h - 1 - ry;                  /* V-flip */
        uint32_t row = data + (uint32_t)ry * rowbytes;
        uint8_t base = (uint8_t)(ram(e + 10) << 4);
        for (int px = 0; px < w; px++) {
            int x = sxp + px;
            if (x < 0 || x >= VICKE_WIDTH) continue;
            int sp = (ctrl & 4) ? (w - 1 - px) : px;    /* H-flip */
            uint8_t b = ram(row + sp * bpp / 8);
            uint8_t pix = (bpp == 8) ? b : ((sp & 1) ? (b & 0x0F) : (b >> 4));
            if (!pix) continue;
            if (owner[x]) { int o = owner[x] - 1; col_ss[o >> 3] |= 1 << (o & 7); col_ss[n >> 3] |= 1 << (n & 7); }
            else owner[x] = (uint8_t)(n + 1);
            if (layer_hit[x]) col_sl[n >> 3] |= 1 << (n & 7);
            line[x] = (bpp == 8) ? pix : (uint8_t)(base | pix);
        }
    }
}

void vicke_render(uint8_t *fb, int pitch)
{
    int enabled = reg[VR_CTRL] & 1;
    uint8_t bg = reg[VR_BGCOL];
    memset(col_ss, 0, 16); memset(col_sl, 0, 16);
    for (int y = 0; y < VICKE_HEIGHT; y++) {
        cur_line = y;
        uint8_t *line = fb + y * pitch;
        memset(line, bg, VICKE_WIDTH);
        if (!enabled) continue;
        memset(owner, 0, VICKE_WIDTH); memset(layer_hit, 0, VICKE_WIDTH);
        int first = 1;
        for (int n = 0; n < VICKE_LAYERS; n++) {
            if (reg[VR_LAYER(n) + VL_CTRL] & 1) { layer_line(n, y, line, first); first = 0; }
            sprites_line(n, y, line);
        }
    }
    for (int i = 0; i < 16; i++) { reg[VR_COLSS + i] |= col_ss[i]; reg[VR_COLSL + i] |= col_sl[i]; }
    cur_line = VICKE_HEIGHT;   /* vblank */
}
