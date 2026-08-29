/* K4510 demo: TINY -- a scrolling tile map with sprites. Kenney's Tiny
 * Dungeon (CC0) as VICKY 16x16 tiles at 8 bpp on layer 0, the Tiled sample
 * map tiled 2x2 into a 64x40 world (1024x640 px) seen through a 320x240
 * window; the hero and a crowd of monsters are 8 bpp sprites whose DATA
 * pointers aim straight into the tile set (a 16x16 8 bpp sprite and a 16x16
 * 8 bpp tile have the same layout), the camera follows the hero, a text8
 * caption on layer 1. The tiles and map come in as a K4SG segment
 * (tiny-header.s), made by tools/mktiny.py.
 *
 *   arrows   the hero turns and keeps walking     space   stop
 *   Esc / Q  back to the shell
 *
 * The keyboard is an event queue, not a state, so the hero walks until told
 * otherwise -- which also makes the demo run itself. */
#include "k4510.h"
#include "tiny.h"

#define TINY_PHYS 0x00110000UL           /* the K4SG segment: tiles, then the map (tiny-header.s) */
#define TILES     TINY_PHYS
#define MAP       (TINY_PHYS + TINY_MAP)
#define SPRTAB_A  0x00130000UL           /* two 128 x 16 byte sprite tables */
#define SPRTAB_B  0x00131000UL
#define TEXTMAP   0x00132000UL           /* 40 x 30 */

#define VIEW_W   320
#define VIEW_H   240
#define WORLD_W  (TINY_MAPW * 16)
#define WORLD_H  (TINY_MAPH * 16)
#define NMON     40
#define NSPR     (1 + NMON)
#define HERO     96                      /* the knight */
#define CAPTION_PAL 127                  /* text8 colour = (127 << 1) | 1 = 255 */

static const uint8_t monster_kind[] = { 108, 109, 110, 111, 112, 120, 121, 122, 123, 124, 84, 99, 97, 98 };

/* one actor: a 16x16 body; feet on the bottom half, so the hit box is the
 * lower 8 rows -- a head may overlap a wall's top edge, as in the games */
typedef struct { int16_t x, y; int8_t dx, dy; uint8_t tile, timer, flip; } actor_t;
static actor_t act[NSPR];
static int16_t camx, camy;
static uint8_t cur, frame;
static uint16_t seed = 0x2A17;
static uint8_t rnd(void) { seed = seed * 25173u + 13849u; return (uint8_t)(seed >> 8); }

static uint8_t floor_at(int16_t px, int16_t py)          /* is world pixel (px,py) on a floor tile? */
{
    uint32_t e; uint16_t ent;
    if (px < 0 || py < 0 || px >= WORLD_W || py >= WORLD_H) return 0;
    e = MAP + (((uint32_t)(py >> 4) * TINY_MAPW + (px >> 4)) << 1);
    ent = far_peek(e) | ((uint16_t)(far_peek(e + 1) & 3) << 8);
    return ent < TINY_NTILES ? walkable[ent] : 0;
}
static uint8_t can_stand(int16_t x, int16_t y)           /* the hit box: rows 8..15, columns 2..13 */
{
    return floor_at(x + 2, y + 8) && floor_at(x + 13, y + 8) && floor_at(x + 2, y + 15) && floor_at(x + 13, y + 15);
}
static void place(actor_t *a)                            /* somewhere on a floor, by trial */
{
    do { a->x = (int16_t)((rnd() % TINY_MAPW) << 4); a->y = (int16_t)((rnd() % TINY_MAPH) << 4); } while (!can_stand(a->x, a->y));
}
static void turn(actor_t *a, int8_t dx, int8_t dy) { a->dx = dx; a->dy = dy; if (dx) a->flip = dx < 0; }
static void wander(actor_t *a)
{
    switch (rnd() & 3) { case 0: turn(a, 1, 0); break; case 1: turn(a, -1, 0); break; case 2: turn(a, 0, 1); break; default: turn(a, 0, -1); }
    a->timer = 40 + (rnd() & 63);
}
static uint8_t step(actor_t *a)                          /* one pixel along its heading; 0 when blocked */
{
    int16_t nx = a->x + a->dx, ny = a->y + a->dy;
    if (!can_stand(nx, ny)) return 0;
    a->x = nx; a->y = ny; return 1;
}

static void make_palette(void)
{
    uint8_t i;
    pal(0, 20, 12, 28);                                  /* the ground under everything */
    for (i = 0; i < TINY_NCOL; i++) pal(i + 1, tiny_pal[i][0], tiny_pal[i][1], tiny_pal[i][2]);
    pal(255, 255, 255, 255);                             /* the caption */
}
static void init_tables(void)
{
    uint8_t i; uint32_t t;
    dma_fill(0, SPRTAB_A, 4096); dma_fill(0, SPRTAB_B, 4096);
    for (t = SPRTAB_A; t <= SPRTAB_B; t += SPRTAB_B - SPRTAB_A)
        for (i = 0; i < NSPR; i++) {
            uint32_t e = t + (uint32_t)i * 16;
            far_poke(e + 8, 1 | 2);                      /* enable, 8 bpp, Z 0: above the map, under the caption layer */
            far_poke(e + 9, 1 | (1 << 2));               /* 16 x 16 */
        }
}
static void write_table(uint32_t t)
{
    uint8_t i;
    for (i = 0; i < NSPR; i++, t += 16) {
        actor_t *a = &act[i];
        int16_t sx = a->x - camx, sy = a->y - camy;
        uint32_t d = TILES + ((uint32_t)a->tile << 8);
        far_poke16(t, (uint16_t)sx); far_poke16(t + 2, (uint16_t)sy);
        far_poke16(t + 4, (uint16_t)d); far_poke16(t + 6, (uint16_t)(d >> 16));
        far_poke(t + 8, (uint8_t)(1 | 2 | (a->flip ? 4 : 0)));
    }
}
static void tile_layer(void)
{
    uint16_t L = V_LAYER(0);
    REG(L + 1) = 0; w16(L + 2, 0); w16(L + 4, 0); w16(L + 6, TINY_MAPW);
    w32(L + 8, TILES); w32(L + 12, MAP);
    REG(L) = 1 | (1 << 1) | (3 << 3) | (1 << 5);         /* enable, tile, 8 bpp, 16 px cells */
}
static void camera(void)
{
    camx = act[0].x - (VIEW_W / 2 - 8); camy = act[0].y - (VIEW_H / 2 - 8);
    if (camx < 0) camx = 0; if (camx > WORLD_W - VIEW_W) camx = WORLD_W - VIEW_W;
    if (camy < 0) camy = 0; if (camy > WORLD_H - VIEW_H) camy = WORLD_H - VIEW_H;
    w16(V_LAYER(0) + 2, (uint16_t)camx); w16(V_LAYER(0) + 4, (uint16_t)camy);
}

void main(void)
{
    uint8_t i, k;
    REG(V_CTRL) = 0;
    make_palette();
    act[0].tile = HERO; place(&act[0]); turn(&act[0], 1, 0);
    for (i = 1; i < NSPR; i++) { act[i].tile = monster_kind[(i - 1) % sizeof monster_kind]; place(&act[i]); wander(&act[i]); }
    dma_fill(' ', TEXTMAP, 40 * 30);
    text8_print(TEXTMAP, 40, 0, 0,  "TINY DUNGEON 16x16 8bpp tiles+41 spr fps");
    text8_print(TEXTMAP, 40, 0, 29, "arrows steer, space stops, Esc leaves   ");
    text8_layer(1, TEXTMAP, 40, CAPTION_PAL);
    tile_layer(); camera();
    init_tables(); write_table(SPRTAB_A); w32(V_SPRTAB, SPRTAB_A); REG(V_SPRCTL) = 1;
    REG(V_CTRL) = 1 | 2 | 4;                             /* 320 x 240 */
    for (;;) {
        uint32_t back = cur ? SPRTAB_A : SPRTAB_B;
        k = key_get();
        if (k == 0x1B || k == 'q' || k == 'Q') break;
        if (k == 0x80) turn(&act[0], 0, -1); else if (k == 0x81) turn(&act[0], 0, 1);
        else if (k == 0x82) turn(&act[0], -1, 0); else if (k == 0x83) turn(&act[0], 1, 0);
        else if (k == ' ') { act[0].dx = act[0].dy = 0; }
        if (act[0].dx || act[0].dy) { if (!step(&act[0])) { act[0].dx = act[0].dy = 0; } }
        for (i = 1; i < NSPR; i++) {
            actor_t *a = &act[i];
            if (frame & 1) continue;                     /* monsters at half the hero's pace */
            if (!a->timer || !step(a)) wander(a); else a->timer--;
        }
        camera();
        write_table(back);
        wait_vblank();
        w32(V_SPRTAB, back); cur ^= 1; frame++;
        fps_tick(); put_num(TEXTMAP, 40, 37, 0, fps_value); fps_last = REG(SYS + 0x0D);
    }
}
