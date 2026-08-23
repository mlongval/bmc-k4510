/* BMC-K4510: a program bigger than its window. K-01..K-04 together.
 * main() lives at $6000 as usual. Two overlays are linked for the SAME
 * address, $4000 (block 2), and stored at different physical addresses
 * (1 MB and 1 MB + 8 KB); the K4SG header (segdemo.s) tells the ROM's
 * LOAD where each goes. Calls go through the far gate: slot 0 banks
 * overlay 1 in and calls it, slot 1 does the same for overlay 2; each RTS
 * restores block 2. A 64 KB table lives at 2 MB and is read with
 * far_peek and through a bank. */
#include "k4510.h"
#include "far.h"

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);
static void print(const char *s) { while (*s) rom_chrout(*s++); }
static void nl(void) { rom_chrout('\n'); }
static void dec(uint32_t v) { char b[11]; uint8_t i = 10; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); print(b + i); }
static void hex32(uint32_t v) { static const char h[] = "0123456789ABCDEF"; int8_t i; for (i = 28; i >= 0; i -= 4) rom_chrout(h[(v >> i) & 15]); }

#define OVL1_PHYS 0x00100000UL
#define OVL2_PHYS 0x00102000UL
#define TABLE     0x00200000UL      /* 64 KB of bytes i*3 at 2 MB */

/* ---- overlay 1: squares ------------------------------------------------ */
#pragma code-name (push, "OVL1")
#pragma rodata-name (push, "OVL1R")   /* NOT the code segment: cc65 would emit the string before the function label */
unsigned __fastcall__ ovl1_square(unsigned n)
{
    static const char who[] = "  [overlay 1 at $4000 <- phys $100000] square(";
    print(who); dec(n); print(") = "); dec((uint32_t)n * n); nl();
    return n * n;
}
#pragma rodata-name (pop)
#pragma code-name (pop)

/* ---- overlay 2: sums the far table through block 1 ------------------- */
#pragma code-name (push, "OVL2")
#pragma rodata-name (push, "OVL2R")
unsigned __fastcall__ ovl2_sum(unsigned count)
{
    static const char who[] = "  [overlay 2 at $4000 <- phys $102000] sum of ";
    uint8_t *p = BANK_WINDOW(1); unsigned i; uint32_t s = 0;
    bank_set(1, TABLE);                     /* block 1 ($2000-$3FFF) -> the table */
    for (i = 0; i < count; i++) s += p[i];
    bank_off(1);
    print(who); dec(count); print(" table bytes via bank 1 = "); dec(s); nl();
    return (unsigned)s;
}
#pragma rodata-name (pop)
#pragma code-name (pop)

/* the descriptor table: slot n -> overlay n. The entry is the function's
 * link address inside the overlay (its strings may come first), so it is
 * filled in at run time. */
static far_desc_t far_tab[] = {
    { OVL1_PHYS, 2, 0, 0 },
    { OVL2_PHYS, 2, 0, 0 },
};
typedef unsigned (__fastcall__ *ufn_t)(unsigned);
#define SQUARE FAR_FN(0, ufn_t)
#define SUM    FAR_FN(1, ufn_t)

void main(void)
{
    unsigned i, r; uint32_t t;
    rom_chrout(12);
    print("BMC-K4510 segdemo: a program bigger than its window (K-01..K-04)"); nl(); nl();
    print("LOAD put main at $6000, overlay 1 at $100000, overlay 2 at $102000."); nl();
    print("Both overlays are linked for $4000. Block 2 now: $"); hex32(bank_get(2)); nl(); nl();

    far_tab[0].entry = (uint16_t)ovl1_square; far_tab[1].entry = (uint16_t)ovl2_sum;
    far_table(far_tab);
    print("far table at $DF80 = $"); hex32(far_r32(FAR_TAB)); nl();

    for (i = 1; i <= 3; i++) {
        r = SQUARE(i * 7);
        print("  back in main: got "); dec(r); print(", block 2 = $"); hex32(bank_get(2)); print(", depth "); dec(far_depth()); nl();
    }
    nl(); print("filling 64 KB at $200000 with i*3 by far_poke..."); nl();
    t = TABLE; for (i = 0; i < 256; i++) { far_fill(t, 256, (uint8_t)(i * 3)); t += 256; }
    r = SUM(1000);
    print("  back in main: got "); dec(r); print(", bank 1 = $"); hex32(bank_get(1)); nl();
    nl(); print("far_peek($200205) = "); dec(far_peek(TABLE + 0x205)); print("  far_peek16($200100) = "); dec(far_peek16(TABLE + 0x100)); nl();
    print("gate errors: "); dec(far_error()); nl();
    nl(); print("DONE -- press a key"); nl();
    while (rom_getin()) ; while (!rom_getin()) ;
}
