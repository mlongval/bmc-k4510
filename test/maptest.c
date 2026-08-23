/* Phase 3: 256 MB, MAP, flat pointers into high memory, lazy commit.
 *
 * All programs are assembled by hand; the MAP encoding is the 4510's with
 * the 45GS10 megabyte convention (X==$0F / Z==$0F).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static long rss_kb(void)
{
    FILE *f = fopen("/proc/self/statm", "r"); long sz, res; if (!f) return -1;
    if (fscanf(f, "%ld %ld", &sz, &res) != 2) res = -1;
    fclose(f); return res * 4;
}

static void boot(const uint8_t *prog, size_t len, int cycles)
{
    mem_reset();
    mem_load(0xC000, prog, len);                 /* block 6: safe from low-half MAPs */
    mem_poke(K4510_ROM_PHYS + 0xFFFC, 0x00); mem_poke(K4510_ROM_PHYS + 0xFFFD, 0xC0);
    cpu65_reset();
    cpu65_step(cycles);
}

int main(void)
{
    printf("K4510 Phase 3: memory system\n");
    long rss0 = rss_kb();
    CHECK(mem_init() == 0, "mem_init");
    long rss1 = rss_kb();
    printf("0. reserve 256 MB: RSS %ld KB -> %ld KB (lazy commit: should barely move)\n", rss0, rss1);
    CHECK(rss1 - rss0 < 4096, "reserving 256 MB committed %ld KB", rss1 - rss0);

    /* 1. MAP lower half: block 1 ($2000-$3FFF) -> phys $40000+ (offset $3E000 so $2000+$3E000=$40000) */
    {
        /* offset_low = $3E000 -> A = $E0 (bits 8-15 = $E0), X&15 = 3 (bits 16-19); mask block1 -> X = $13 */
        static const uint8_t p[] = {
            0xA9, 0xE0,             /* LDA #$E0           */
            0xA2, 0x23,             /* LDX #$23  (mask bit1 = blk1, off hi nibble 3) */
            0xA0, 0x00,             /* LDY #0             */
            0xA3, 0x00,             /* LDZ #0   (upper half unchanged, mask 0) */
            0x5C,                   /* MAP                */
            0xEA,                   /* EOM                */
            0xA9, 0x5A,             /* LDA #$5A           */
            0x8D, 0x00, 0x20,       /* STA $2000  -> phys $40000 */
            0xA9, 0xA5,
            0x8D, 0xFF, 0x3F,       /* STA $3FFF  -> phys $41FFF */
            0x4C, 0x14, 0xC0,       /* JMP *              */
        };
        boot(p, sizeof p, 500);
        const k4510_map_t *m = mem_map_state();
        printf("1. MAP blk1->$40000: mask=$%02X off_lo=$%05X  $2000->$%06X  phys[$40000]=$%02X phys[$41FFF]=$%02X\n",
               m->mask, m->offset_low, mem_cpu_to_phys(0x2000), mem_peek(0x40000), mem_peek(0x41FFF));
        CHECK(m->mask == 0x02, "mask");
        CHECK(mem_cpu_to_phys(0x2000) == 0x40000, "translation");
        CHECK(mem_peek(0x40000) == 0x5A && mem_peek(0x41FFF) == 0xA5, "writes landed in high memory");
        CHECK(mem_peek(0x2000) == 0x00, "unmapped phys $2000 untouched");
        CHECK(mem_cpu_to_phys(0x4000) == 0x4000, "block 2 still unmapped");
    }

    /* 2. Megabyte select: MAP with X=$0F, A=$05 -> lower MB = 5; then map blk0 with offset 0 -> $0000 = phys $500000 */
    {
        static const uint8_t p[] = {
            0xA9, 0x05, 0xA2, 0x0F, 0xA0, 0x00, 0xA3, 0x00, 0x5C, 0xEA,   /* MB_low = 5 */
            0xA9, 0x00, 0xA2, 0x10, 0xA0, 0x00, 0xA3, 0x00, 0x5C, 0xEA,   /* map blk0, offset 0 */
            0xA9, 0x77, 0x85, 0x10,                                       /* STA $10 (zp!) -> phys $500010 */
            0x4C, 0x18, 0xC0,
        };
        boot(p, sizeof p, 500);
        const k4510_map_t *m = mem_map_state();
        printf("2. MB select:       mb_lo=$%07X  $0010->$%06X  phys[$500010]=$%02X\n",
               m->mb_low, mem_cpu_to_phys(0x0010), mem_peek(0x500010));
        CHECK(m->mb_low == 0x500000, "megabyte register");
        CHECK(mem_peek(0x500010) == 0x77, "zero page mapped into MB 5");
    }

    /* 3. Flat pointer to the top of the 256 MB: $0FFFFFF0 */
    {
        static const uint8_t p[] = {
            0xA9, 0xF0, 0x85, 0x20, 0xA9, 0xFF, 0x85, 0x21, 0xA9, 0xFF, 0x85, 0x22, 0xA9, 0x0F, 0x85, 0x23,
            0xA3, 0x00,
            0xA9, 0xC3,
            0xEA, 0x92, 0x20,       /* NOP; STA ($20),Z = STA [$20],Z  -> phys $0FFFFFF0 */
            0xA3, 0x0F,
            0xEA, 0xB2, 0x20,       /* LDA [$20],Z (Z=$F) -> phys $0FFFFFFF */
            0x8D, 0x00, 0x04,
            0x4C, 0x1F, 0xC0,
        };
        mem_reset();
        mem_poke(0x0FFFFFFF, 0x3C);
        boot(p, sizeof p, 500);
        printf("3. flat to top:     phys[$0FFFFFF0]=$%02X  read back from $0FFFFFFF=$%02X\n", mem_peek(0x0FFFFFF0), mem_peek(0x400));
        CHECK(mem_peek(0x0FFFFFF0) == 0xC3, "STA [$20],Z to top of memory");
        CHECK(mem_peek(0x400) == 0x3C, "LDA [$20],Z from top of memory");
    }

    /* 4. Lazy commit after touching a handful of far-apart pages */
    long rss2 = rss_kb();
    printf("4. after the tests: RSS %ld KB (touched a few pages across 256 MB)\n", rss2);
    CHECK(rss2 - rss0 < 8192, "touching a few pages committed %ld KB", rss2 - rss0);

    /* 5. STQ through a flat pointer lands 4 bytes in high memory */
    {
        static const uint8_t p[] = {
            0xA9, 0x00, 0x85, 0x20, 0xA9, 0x00, 0x85, 0x21, 0xA9, 0x80, 0x85, 0x22, 0xA9, 0x00, 0x85, 0x23, /* ptr = $00800000 (8 MB) */
            0xA3, 0x00,
            0xA9, 0x11, 0xA2, 0x22, 0xA0, 0x33, 0xA3, 0x44,
            0x42, 0x42, 0xEA, 0x92, 0x20,   /* NEG NEG NOP STA ($20),Z = STQ [$20],Z */
            0x4C, 0x1F, 0xC0,
        };
        boot(p, sizeof p, 500);
        printf("5. STQ [$20],Z @8MB: %02X %02X %02X %02X\n", mem_peek(0x800000), mem_peek(0x800001), mem_peek(0x800002), mem_peek(0x800003));
        CHECK(mem_peek(0x800000) == 0x11 && mem_peek(0x800003) == 0x44, "STQ [ptr],Z");
    }

    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails);
    return fails != 0;
}
