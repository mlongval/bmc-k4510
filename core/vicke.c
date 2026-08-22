#include "vicke.h"
#include "mem.h"
#include <string.h>

static uint8_t  reg[256];
static uint32_t pal[256];
static int      cur_line;

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
    default:            return reg[r];
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

/* Render one scanline of one layer into line[], honouring transparency.
 * opaque: this is the lowest enabled layer, so index 0 is drawn too. */
static void layer_line(int n, int y, uint8_t *line, int opaque)
{
    const uint8_t *L = &reg[VR_LAYER(n)];
    int mode  = (L[VL_CTRL] >> 1) & 3;
    int depth = (L[VL_CTRL] >> 3) & 3;          /* 0..3 -> 1,2,4,8 bpp */
    int bpp   = 1 << depth;
    int ppb   = 8 / bpp;                         /* pixels per byte */
    uint8_t  palofs = L[VL_PALOFS];
    uint32_t data   = rd32(&L[VL_DATA]);
    uint32_t map    = rd32(&L[VL_MAP]);
    uint16_t stride = rd16(&L[VL_STRIDE]);
    int sy = y + rd16(&L[VL_SCROLLY]);
    int sx0 = rd16(&L[VL_SCROLLX]);
    uint8_t mask = (uint8_t)((1 << bpp) - 1);
    uint8_t base = (uint8_t)(palofs << bpp);     /* palette offset shifted by depth */

    for (int x = 0; x < VICKE_WIDTH; x++) {
        int sx = x + sx0;
        uint8_t pix;
        if (mode == VL_MODE_BITMAP) {
            uint32_t a = data + (uint32_t)sy * stride + (sx / ppb);
            uint8_t b = ram(a);
            int shift = (bpp == 8) ? 0 : (8 - bpp - (sx % ppb) * bpp);    /* MSB-first packing */
            pix = (b >> shift) & mask;
        } else {
            /* tile/text: 8x8 cells, 1 bpp glyphs for now; map one byte per cell */
            int cx = sx >> 3, cy = sy >> 3;
            uint8_t cell = ram(map + (uint32_t)cy * stride + cx);
            uint8_t row  = ram(data + (uint32_t)cell * 8 + (sy & 7));
            pix = (row >> (7 - (sx & 7))) & 1;
        }
        if (pix || opaque) line[x] = (bpp == 8) ? pix : (uint8_t)(base | pix);
    }
}

void vicke_render(uint8_t *fb, int pitch)
{
    int enabled = reg[VR_CTRL] & 1;
    uint8_t bg = reg[VR_BGCOL];
    for (int y = 0; y < VICKE_HEIGHT; y++) {
        cur_line = y;
        uint8_t *line = fb + y * pitch;
        memset(line, bg, VICKE_WIDTH);
        if (!enabled) continue;
        int first = 1;
        for (int n = 0; n < VICKE_LAYERS; n++) {
            if (!(reg[VR_LAYER(n) + VL_CTRL] & 1)) continue;
            layer_line(n, y, line, first);
            first = 0;
        }
    }
    cur_line = VICKE_HEIGHT;   /* vblank */
}
