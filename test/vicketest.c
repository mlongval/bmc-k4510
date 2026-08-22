/* VICKe step 1: palette port, 8 bpp bitmap layer, text layer, transparency, scroll. */
#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/vicke.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static uint8_t fb[VICKE_WIDTH * VICKE_HEIGHT];
static void W(int r, uint8_t v) { io_write(IO_VICKE + r, v); }
static void W32(int r, uint32_t v) { for (int i = 0; i < 4; i++) W(r + i, (v >> (8 * i)) & 0xFF); }
static void W16(int r, uint16_t v) { W(r, v & 0xFF); W(r + 1, v >> 8); }

int main(void)
{
    printf("VICKe step 1\n");
    CHECK(mem_init() == 0, "mem_init");
    mem_reset();

    /* palette port: write entry 5 = #123456, index auto-increments */
    W(VR_PALIDX, 5); W(VR_PALR, 0x12); W(VR_PALG, 0x34); W(VR_PALB, 0x56);
    CHECK(vicke_palette_rgb(5) == 0x123456, "palette write");
    CHECK(io_read(IO_VICKE + VR_PALIDX) == 6, "palette index auto-increment");
    CHECK(vicke_palette_rgb(1) == 0xFFFFFF, "default palette entry 1 = white");

    /* layer 0: 8 bpp bitmap at phys $200000, stride 640, filled with index 7 except a hole of 0 */
    uint32_t bmp = 0x200000;
    for (int y = 0; y < VICKE_HEIGHT; y++) for (int x = 0; x < VICKE_WIDTH; x++)
        mem_poke(bmp + y * 640 + x, (x >= 100 && x < 200 && y >= 100 && y < 200) ? 0 : 7);
    W(VR_BGCOL, 2);
    W(VR_CTRL, 1);
    W32(VR_LAYER(0) + VL_DATA, bmp); W16(VR_LAYER(0) + VL_STRIDE, 640);
    W(VR_LAYER(0) + VL_CTRL, 1 | (VL_MODE_BITMAP << 1) | (3 << 3));   /* enable, bitmap, 8 bpp */
    vicke_render(fb, VICKE_WIDTH);
    printf("1. bitmap: px(10,10)=%d hole(150,150)=%d (lowest layer: 0 drawn, not bg)\n", fb[10*640+10], fb[150*640+150]);
    CHECK(fb[10 * 640 + 10] == 7, "bitmap pixel");
    CHECK(fb[150 * 640 + 150] == 0, "lowest layer draws index 0");

    /* layer 1: text on top, glyph 'X' at cell (0,0) with a 1bpp font where 'X' = all ones; palofs 3 -> indices 6/7... */
    uint32_t font = 0x10000, map = 0x800;
    for (int i = 0; i < 256 * 8; i++) mem_poke(font + i, 0);
    for (int r = 0; r < 8; r++) mem_poke(font + 'X' * 8 + r, 0xFF);
    for (int i = 0; i < 80 * 60; i++) mem_poke(map + i, ' ');
    mem_poke(map + 0, 'X');
    W32(VR_LAYER(1) + VL_DATA, font); W32(VR_LAYER(1) + VL_MAP, map); W16(VR_LAYER(1) + VL_STRIDE, 80);
    W(VR_LAYER(1) + VL_PALOFS, 9);                     /* 1 bpp: index = (9<<1)|pix = 18/19 */
    W(VR_LAYER(1) + VL_CTRL, 1 | (VL_MODE_TEXT << 1));
    vicke_render(fb, VICKE_WIDTH);
    printf("2. text over bitmap: (0,0)=%d  (20,0)=%d (space: transparent, bitmap shows)\n", fb[0], fb[20]);
    CHECK(fb[0] == 19, "text pixel with palette offset");
    CHECK(fb[20] == 7,  "transparent text cell shows bitmap");

    /* scroll text layer by 8 px: the X moves left off-screen, cell (1,0) now at x=0 */
    mem_poke(map + 1, 'X');
    W16(VR_LAYER(1) + VL_SCROLLX, 8);
    vicke_render(fb, VICKE_WIDTH);
    CHECK(fb[0] == 19 && fb[8] == 7, "scroll X by one cell");

    /* display off -> all background */
    W(VR_CTRL, 0); vicke_render(fb, VICKE_WIDTH);
    CHECK(fb[0] == 2 && fb[150 * 640 + 150] == 2, "display off = background colour");

    /* raster register reads back end-of-frame */
    CHECK(io_read(IO_VICKE + VR_RASTER) == (VICKE_HEIGHT & 0xFF), "raster low after frame");

    /* ---- tiles: 16x16 at 4 bpp, map entry with H-flip and palette offset ---- */
    mem_reset(); W(VR_CTRL, 1); W(VR_BGCOL, 0);
    uint32_t tiles = 0x300000, tmap = 0x310000;
    /* tile 1: left half index 1, right half index 2 (4 bpp: 8 bytes per row, 16 rows) */
    for (int r = 0; r < 16; r++) for (int b = 0; b < 8; b++) mem_poke(tiles + 128 + r * 8 + b, b < 4 ? 0x11 : 0x22);
    for (int i = 0; i < 40 * 30 * 2; i++) mem_poke(tmap + i, 0);
    mem_poke(tmap + 0, 1); mem_poke(tmap + 1, 0x00);          /* cell 0: tile 1, no flip, palofs 0 */
    mem_poke(tmap + 2, 1); mem_poke(tmap + 3, 0x04 | 0x30);   /* cell 1: tile 1, H-flip, palofs 3 */
    W32(VR_LAYER(0) + VL_DATA, tiles); W32(VR_LAYER(0) + VL_MAP, tmap); W16(VR_LAYER(0) + VL_STRIDE, 40);
    W(VR_LAYER(0) + VL_CTRL, 1 | (VL_MODE_TILE << 1) | (2 << 3) | (1 << 5));   /* tile, 4 bpp, 16 px */
    vicke_render(fb, VICKE_WIDTH);
    printf("3. tiles 16x16 4bpp: cell0 (0,5)=%d (12,5)=%d | cell1 flipped+palofs3 (16,5)=%d (28,5)=%d\n",
           fb[5*640+0], fb[5*640+12], fb[5*640+16], fb[5*640+28]);
    CHECK(fb[5*640+0] == 1 && fb[5*640+12] == 2, "tile halves");
    CHECK(fb[5*640+16] == (3<<4|2) && fb[5*640+28] == (3<<4|1), "H-flip and palette offset");

    /* ---- text32: per-cell fg/bg, reverse bit, 8x16 glyphs ---- */
    uint32_t f16 = 0x320000, m32 = 0x330000;
    for (int i = 0; i < 256 * 16; i++) mem_poke(f16 + i, 0);
    for (int r = 0; r < 16; r++) mem_poke(f16 + 'A' * 16 + r, 0xF0);   /* left half set */
    uint8_t cell0[4] = { 'A', 0x00, 9, 4 }, cell1[4] = { 'A', 0x80, 9, 4 };
    mem_load(m32, cell0, 4); mem_load(m32 + 4, cell1, 4);
    W32(VR_LAYER(1) + VL_DATA, f16); W32(VR_LAYER(1) + VL_MAP, m32); W16(VR_LAYER(1) + VL_STRIDE, 80);
    W(VR_LAYER(1) + VL_CTRL, 1 | (VL_MODE_TEXT32 << 1) | (1 << 5));   /* text32, 8x16 */
    vicke_render(fb, VICKE_WIDTH);
    printf("4. text32 8x16: cell0 (1,12)=%d (6,12)=%d | reversed cell1 (9,12)=%d (14,12)=%d\n",
           fb[12*640+1], fb[12*640+6], fb[12*640+9], fb[12*640+14]);
    CHECK(fb[12*640+1] == 9 && fb[12*640+6] == 4, "text32 fg/bg");
    CHECK(fb[12*640+9] == 4 && fb[12*640+14] == 9, "text32 reverse");
    CHECK(fb[15*640+1] == 9 && fb[16*640+1] != 9, "8x16 glyph height");

    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails);
    return fails != 0;
}
