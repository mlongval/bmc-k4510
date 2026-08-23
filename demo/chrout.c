/* BMC-K4510: CHROUT throughput benchmark. How fast is the ROM's terminal
 * path (C with cc65, scroll by DMA)?  Three passes through $FF80:
 *   A  a stream of printable characters, wrapping at the right edge
 *   B  79-character lines ending in newline (wrap + scroll each line)
 *   C  bare newlines (scroll only: the DMA path with no glyph work)
 * Timed with the $D50D 16-bit frame counter (60 Hz, 675,000 cycles/frame
 * at 40.5 MHz), so the result is independent of the host. */
#include "k4510.h"

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);

#define CYCLES_PER_FRAME 675000UL

static void print(const char *s) { while (*s) rom_chrout(*s++); }
static void nl(void) { rom_chrout('\n'); }
static void dec(uint32_t v)
{
    char b[11]; uint8_t i = 10; b[i] = 0;
    do { b[--i] = '0' + v % 10; v /= 10; } while (v);
    print(b + i);
}
static uint16_t frames_now(void) { return REG(SYS + 0x0D) | ((uint16_t)REG(SYS + 0x0E) << 8); }

typedef struct { const char *name; uint32_t chars; uint16_t frames; } result_t;
static result_t res[3];

static void report(const result_t *r)
{
    uint16_t f = r->frames ? r->frames : 1;
    print("  "); print(r->name); print(": "); dec(r->chars); print(" ch, "); dec(r->frames); print(" fr, ");
    dec((uint32_t)f * CYCLES_PER_FRAME / r->chars); print(" cyc/ch, ");
    dec((uint32_t)(r->chars / 100) * 6000 / f); print(" ch/s"); nl();
}

void main(void)
{
    uint16_t t0, i; uint8_t c; uint32_t n;

    rom_chrout(12);
    print("BMC-K4510 CHROUT benchmark (ROM jump table $FF80, 80x60 text)"); nl(); nl();

    /* A: 40,000 printable characters, no newline: the glyph path + wrap + scroll every 80 */
    res[0].name = "A stream"; res[0].chars = 40000UL; c = '!';
    t0 = frames_now();
    for (n = 0; n < 40000UL; n++) { rom_chrout(c); if (++c > '~') c = '!'; }
    res[0].frames = frames_now() - t0;

    /* B: 500 lines of 79 characters + newline */
    res[1].name = "B lines "; res[1].chars = 500UL * 80; c = 'A';
    t0 = frames_now();
    for (i = 0; i < 500; i++) { uint8_t k; for (k = 0; k < 79; k++) rom_chrout(c); rom_chrout('\n'); if (++c > 'Z') c = 'A'; }
    res[1].frames = frames_now() - t0;

    /* C: 3000 newlines: scroll by DMA, nothing else */
    res[2].name = "C nl    "; res[2].chars = 3000UL;
    t0 = frames_now();
    for (i = 0; i < 3000; i++) rom_chrout('\n');
    res[2].frames = frames_now() - t0;

    rom_chrout(12);
    print("CHROUT results (1 frame = 1/60 s = 675000 cycles at 40.5 MHz):"); nl();
    report(&res[0]); report(&res[1]); report(&res[2]);
    nl(); print("DONE -- press a key"); nl();
    while (rom_getin()) ; while (!rom_getin()) ;   /* the ROM clears the screen when we return */
}
