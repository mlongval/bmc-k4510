#include "io.h"
#include "mem.h"
#include "vicke.h"
#include <string.h>

/* ---- keyboard: a FIFO behind two registers (Wozmon polls them) -------- */
static uint8_t kbd_fifo[64];
static int     kbd_head, kbd_tail;
static uint8_t kbd_last;

void kbd_push(uint8_t ascii)
{
    int next = (kbd_tail + 1) & 63;
    if (next == kbd_head) return;
    kbd_fifo[kbd_tail] = ascii;
    kbd_tail = next;
}
static int kbd_ready(void) { return kbd_head != kbd_tail; }
static uint8_t kbd_read(void)
{
    if (kbd_ready()) { kbd_last = kbd_fifo[kbd_head] | 0x80; kbd_head = (kbd_head + 1) & 63; }
    return kbd_last;
}

/* ---- DMA ---------------------------------------------------------------- */
static uint8_t dma_reg[16];     /* SRC[4] DST[4] LEN[4] CMD STATUS .. */

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static void dma_run(uint8_t cmd)
{
    uint32_t src = rd32(&dma_reg[0]) & K4510_PHYS_MASK;
    uint32_t dst = rd32(&dma_reg[4]) & K4510_PHYS_MASK;
    uint32_t len = rd32(&dma_reg[8]) & K4510_PHYS_MASK;
    dma_reg[13] = cmd;
    switch (cmd) {
    case 1:   /* copy, memmove semantics (overlap-safe), wraps at 256 MB */
        if (src + len <= K4510_PHYS_SIZE && dst + len <= K4510_PHYS_SIZE) {
            memmove(k4510_ram + dst, k4510_ram + src, len);
        } else {
            for (uint32_t i = 0; i < len; i++)       /* rare: wrap */
                k4510_ram[(dst + i) & K4510_PHYS_MASK] = k4510_ram[(src + i) & K4510_PHYS_MASK];
        }
        break;
    case 2:   /* fill with SRC byte 0 */
        if (dst + len <= K4510_PHYS_SIZE) memset(k4510_ram + dst, dma_reg[0], len);
        else for (uint32_t i = 0; i < len; i++) k4510_ram[(dst + i) & K4510_PHYS_MASK] = dma_reg[0];
        break;
    case 3:   /* swap */
        for (uint32_t i = 0; i < len; i++) {
            uint8_t *a = &k4510_ram[(src + i) & K4510_PHYS_MASK], *b = &k4510_ram[(dst + i) & K4510_PHYS_MASK];
            uint8_t t = *a; *a = *b; *b = t;
        }
        break;
    default:
        dma_reg[13] = 0xFF;
    }
    dma_reg[12] = 0;   /* instant: idle again before the CPU sees the next instruction */
}

/* ---- dispatch ------------------------------------------------------------ */

void io_reset(void)
{
    kbd_head = kbd_tail = 0; kbd_last = 0;
    memset(dma_reg, 0, sizeof dma_reg);
    vicke_reset();
}

uint8_t io_read(uint16_t addr)
{
    switch (addr & 0xFF00) {
    case IO_VICKE:
        return vicke_read(addr & 0xFF);
    case IO_INPUT:
        if (addr == IO_KBD)   return kbd_read();
        if (addr == IO_KBDCR) return kbd_ready() ? 0x80 : 0x00;
        return 0xFF;
    case IO_DMA:
        if ((addr & 0xFF) < 16) return dma_reg[addr & 0xFF];
        return 0xFF;
    default:
        return 0xFF;
    }
}

void io_write(uint16_t addr, uint8_t v)
{
    switch (addr & 0xFF00) {
    case IO_VICKE:
        vicke_write(addr & 0xFF, v); return;
    case IO_DMA:
        if ((addr & 0xFF) < 12) { dma_reg[addr & 0xFF] = v; return; }
        if (addr == IO_DMA_CMD) { dma_run(v); return; }
        return;
    default:
        return;
    }
}
