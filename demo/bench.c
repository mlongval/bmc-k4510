/* BMC-K4510: BENCH -- the machine measures itself and writes the numbers to
 * disk.
 *
 * Doc found a Pi build "about 10x slower" than the one before it, and the
 * same code on the desktop is not slower at all, so the difference is the
 * Pi's alone. A screen you have to read back is no use when the screen may
 * be the slow thing, so this runs from STARTUP.BAT, writes
 * /SYSTEM/BENCH-<when>.TXT onto the card, and the card carries the numbers
 * to a machine that can read them.
 *
 * The headline is frames per second. The emulator steps a fixed number of
 * CPU cycles per scanline inside its frame loop, so the host's frame rate
 * and the emulated machine's speed are the same number: 60 means the host
 * is keeping up and the machine runs at its nominal 40.5 MHz, and 6 means
 * it is not, and everything the machine does is ten times slower for that
 * one reason. The other three figures separate the paths -- plain CPU work,
 * the ROM's console (far memory and scrolling), and the DMA engine -- so if
 * the frame rate is right and one of those is not, it says which.
 *
 * Timing is the host wall clock at SYS+4, which counts whole seconds, so
 * each figure is work counted over a fixed number of seconds rather than a
 * time measured for fixed work. That keeps the run the same length whatever
 * the speed: a slow machine reports smaller numbers, not a longer wait. */
#include "k4510.h"

#define TERM 0xDA00u
#define SECS 3                       /* per figure; four figures, so about 12 s */

void __fastcall__ rom_chrout(unsigned char c);
unsigned char __fastcall__ rom_shell(const char *line);
static unsigned char rom_save(void) { return ((unsigned char (*)(void))0xFF8C)(); }
static unsigned char rom_load(void) { return ((unsigned char (*)(void))0xFF89)(); }
#define SCRATCH 0x0E800000UL          /* somewhere to land a probe read */
static void zp16(uint8_t a, uint16_t v) { REG(a) = v; REG(a + 1) = v >> 8; }
static void zp32(uint8_t a, uint32_t v) { REG(a)=v; REG(a+1)=v>>8; REG(a+2)=v>>16; REG(a+3)=v>>24; }

#define BUFMAX 1024
static char BUF[BUFMAX]; static unsigned len;
static char name[64];
static volatile unsigned long sink;          /* so the CPU loop cannot be optimised away */

static void say(const char *s) { while (*s) rom_chrout((unsigned char)*s++); }
static void add(const char *s) { while (*s && len < BUFMAX - 1) BUF[len++] = *s++; }
static void addc(char c) { if (len < BUFMAX - 1) BUF[len++] = c; }
static void nl(void) { addc('\n'); }
static void addn(unsigned long v, uint8_t w)
{
    char t[12]; uint8_t n = 0;
    do { t[n++] = (char)('0' + (uint8_t)(v % 10)); v /= 10; } while (v);
    while (w > n) { addc('0'); w--; }
    while (n) addc(t[--n]);
}
/* screen only: this must NOT go through addn(), which appends to the
 * report and would put every figure into the file twice. */
static void sayn(unsigned long v)
{
    char t[12]; uint8_t n = 0;
    do { t[n++] = (char)('0' + (uint8_t)(v % 10)); v /= 10; } while (v);
    while (n) rom_chrout((unsigned char)t[--n]);
}

/* the clock: SYS+4 latches, and the seconds are at SYS+5 */
/* The latch read must land in a variable: cc65 drops `(void)REG(SYS+4);`
 * entirely -- it emitted `lda $D505` and no `lda $D504` -- so the clock
 * never updated and the first wait span forever. bug.c does it this way
 * for the same reason. */
static uint8_t now(void) { volatile uint8_t d = REG(SYS + 4); (void)d; return REG(SYS + 5); }
static uint8_t since(uint8_t s0, uint8_t s) { return (uint8_t)(s >= s0 ? s - s0 : 60 - s0 + s); }
static uint8_t edge(void) { uint8_t s = now(); while (now() == s) ; return now(); }
static unsigned long fcount(void)
{
    return (unsigned long)REG(SYS + 0x0D) | ((unsigned long)REG(SYS + 0x0E) << 8)
         | ((unsigned long)REG(SYS + 0x0F) << 16);
}

/* count how many times `body` runs in SECS whole seconds */
#define MEASURE(out, body) do { \
        uint8_t s0_ = edge(), s_; \
        (out) = 0; \
        do { body; (out)++; s_ = now(); } while (since(s0_, s_) < SECS); \
        (out) /= SECS; \
    } while (0)

static void line(const char *label, unsigned long v, const char *unit)
{
    add(label); add(": "); addn(v, 0); add(unit); nl();
    say(label); say(": "); sayn(v); say(unit); say("\n");
}

void main(void)
{
    unsigned long fps, cpu, chr, dma, f0;
    uint8_t s0, s, i;

    { volatile uint8_t d = REG(SYS + 4); (void)d; }
    say("\nBENCH -- measuring, about 12 seconds.\n\n");

    /* 1. frames per second: the one that says whether the host keeps up */
    s0 = edge(); f0 = fcount();
    do { s = now(); } while (since(s0, s) < SECS);
    fps = (fcount() - f0) / SECS;

    /* 2. plain CPU work, no I/O but the clock */
    MEASURE(cpu, { unsigned j; for (j = 0; j < 64; j++) sink += j; });

    /* 3. the ROM's console: 64 characters and a newline, so this includes
     *    the far-memory screen and, every 30 lines, a scroll */
    MEASURE(chr, { for (i = 0; i < 64; i++) rom_chrout('.'); rom_chrout('\r'); });

    /* 4. the DMA engine: 4 KB far to far */
    MEASURE(dma, { dma_copy(0x0E000000UL, 0x0E100000UL, 4096UL); });

    rom_shell("CLS");
    say("\nBENCH results\n\n");

    add("BMC-K4510 self-test\n===================\n\n");
    add("Machine:  "); add(REG(SYS + 0x22) ? "Raspberry Pi 3B+" : "desktop"); nl();
    add("Build:    ");
    for (i = 0; i < 16 && REG(SYS + 0x10 + i); i++) addc((char)REG(SYS + 0x10 + i));
    nl();
    add("Nominal:  "); addn((unsigned long)REG(SYS) | ((unsigned long)REG(SYS + 1) << 8), 0); add(" kHz, ");
    addn((unsigned long)REG(SYS + 2) | ((unsigned long)REG(SYS + 3) << 8), 0); add(" MB\n");
    add("Screen:   "); addn(REG(TERM + 5), 0); addc('x'); addn(REG(TERM + 6), 0);
    /* the geometry says the mode without my having to decode it: 80x60 is
     * MODE 0, 80x30 MODE 1, 40x30 MODE 2. The raw CTRL byte is there too, so
     * a reader can check it against VICKY's register map. */
    add(", VICKY CTRL $"); { uint8_t c = REG(0xD000), hi = (uint8_t)(c >> 4), lo = (uint8_t)(c & 15);
      addc((char)(hi < 10 ? '0' + hi : 'A' + hi - 10)); addc((char)(lo < 10 ? '0' + lo : 'A' + lo - 10)); }
    nl();
    add("When:     ");
    addn((unsigned long)REG(SYS + 0x0A) | ((unsigned long)REG(SYS + 0x0B) << 8), 4); addc('-');
    addn(REG(SYS + 9), 2); addc('-'); addn(REG(SYS + 8), 2); addc(' ');
    addn(REG(SYS + 7), 2); addc(':'); addn(REG(SYS + 6), 2); addc(':'); addn(REG(SYS + 5), 2);
    nl(); nl();

    line("Frames per second   ", fps, "   (60 is right)");
    line("CPU loops per second", cpu, "");
    line("Console lines/second", chr, "");
    line("DMA 4K copies/second", dma, "");
    nl();
    add("The frame rate is the machine's speed: the emulator runs its cycle\n");
    add("budget inside the host's frame loop, so 60 fps is 40.5 MHz and half\n");
    add("that frame rate is half the machine. If the frame rate is right and\n");
    add("one of the others is low, the fault is in that path instead.\n");
    add("(Written by BENCH on the machine itself.)\n");

    rom_shell("MKDIR /SYSTEM");
    /* The Pi has no clock of its own: every boot reports the same date, so a
     * name built from it overwrote the previous run -- and comparing runs is
     * the whole point. Take the first free number instead, found by trying to
     * open each one. The date and time are inside the file either way. */
    { uint8_t k;
      for (k = 1; k < 100; k++) {
          const char *p = "/SYSTEM/BENCH-"; uint8_t n = 0;
          while (*p) name[n++] = *p++;
          name[n++] = (char)('0' + k / 10); name[n++] = (char)('0' + k % 10);
          { const char *e = ".TXT"; while (*e) name[n++] = *e++; }
          name[n] = 0;
          zp16(0xF0, (uint16_t)name); zp32(0xF2, SCRATCH);
          if (rom_load()) break;                    /* will not open: free */
      }
    }

    zp16(0xF0, (uint16_t)name); zp32(0xF2, (uint32_t)(uint16_t)BUF); zp32(0xF6, (uint32_t)len);
    if (rom_save()) { say("\nBENCH: could not write "); say(name); say("\n"); return; }
    say("\nWritten: "); say(name); say("\n");
}
