/* Shared hardware access for the K4510 demo ROMs (cc65). Mirrors core/io.h
 * and core/vicke.h. */
#ifndef K4510_DEMO_H
#define K4510_DEMO_H
#include <stdint.h>
#include <string.h>

#define REG(a) (*(volatile uint8_t *)(a))
#define VICKE  0xD000u
#define KBD    0xD100u
#define DMA    0xD200u
#define SID0   0xD400u

#define V_CTRL   0xD000u
#define V_BGCOL  0xD001u
#define V_IRQST  0xD004u
#define V_PALIDX 0xD006u
#define V_PALR   0xD007u
#define V_PALG   0xD008u
#define V_PALB   0xD009u
#define V_SPRTAB 0xD00Au
#define V_SPRCTL 0xD00Eu
#define V_LAYER(n) (0xD010u + (n) * 0x10)
#define V_SHEILA 0xD060u
#define V_SHEILACTL 0xD064u

#define FONT8    0x00010000UL        /* 256 ASCII glyphs, placed by the frontend */
#define WINDOW   ((uint8_t *)0x2000) /* the MAP window, 40 KB, see crt0.s */

extern volatile uint8_t frames;
void __fastcall__ map_window(unsigned long phys);

static void w32(uint16_t r, uint32_t v) { REG(r) = v; REG(r + 1) = v >> 8; REG(r + 2) = v >> 16; REG(r + 3) = v >> 24; }
static void w16(uint16_t r, uint16_t v) { REG(r) = v; REG(r + 1) = v >> 8; }

static void pal(uint8_t i, uint8_t r, uint8_t g, uint8_t b) { REG(V_PALIDX) = i; REG(V_PALR) = r; REG(V_PALG) = g; REG(V_PALB) = b; }

static void wait_vblank(void) { uint8_t f = frames; while (frames == f) ; }

/* DMA: copy len bytes phys src -> phys dst */
static void dma_copy(uint32_t src, uint32_t dst, uint32_t len) { w32(DMA, src); w32(DMA + 4, dst); w32(DMA + 8, len); REG(DMA + 12) = 1; }
/* DMA: fill len bytes at phys dst with value (taken from SRC register byte 0) */
static void dma_fill(uint8_t value, uint32_t dst, uint32_t len) { REG(DMA) = value; w32(DMA + 4, dst); w32(DMA + 8, len); REG(DMA + 12) = 2; }

/* A text8 layer (1 byte per cell, 1-bit glyphs, colour = LPALOFS<<1|1) */
static void text8_layer(uint8_t n, uint16_t map, uint8_t cols, uint8_t palofs)
{
    uint16_t L = V_LAYER(n);
    REG(L + 1) = palofs; w16(L + 2, 0); w16(L + 4, 0); w16(L + 6, cols);
    w32(L + 8, FONT8); w32(L + 12, map);
    REG(L) = 1 | (2 << 1);          /* enable, text8, 8x8 */
}
static void text8_print(uint16_t map, uint8_t cols, uint8_t x, uint8_t y, const char *s)
{
    uint8_t *p = (uint8_t *)(map + (uint16_t)y * cols + x);
    while (*s) *p++ = *s++;
}
#endif
