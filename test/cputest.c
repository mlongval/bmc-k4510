/* Step 1 of the spike: does the 45GS02 core run inside our wrapper?
 *
 * Three hand-assembled programs, no assembler needed yet:
 *   1. 6502 subset: count 0..255 into $0200, then loop forever.
 *   2. 65CE02: Z register, 16-bit stack pointer, INW/DEW.
 *   3. 45GS02: a 32-bit pointer in base page, LDA [$nn],Z, and Q ops.
 * Each checks registers/memory after a fixed number of steps.
 */
#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void run(const uint8_t *prog, size_t len, int cycles)
{
    mem_init();
    mem_load(0x1000, prog, len);
    mem_poke(0xFFFC, 0x00); mem_poke(0xFFFD, 0x10);   /* reset vector -> $1000 */
    cpu65_reset();
    cpu65_step(cycles);
}

int main(void)
{
    printf("K4510 spike step 1: cpu65 (45GS02) in our wrapper\n");

    /* --- 1. plain 6502 ------------------------------------------------ */
    {
        static const uint8_t p[] = {
            0xA2, 0x00,             /* LDX #0         */
            0x8E, 0x00, 0x02,       /* loop: STX $0200 */
            0xE8,                   /* INX            */
            0xD0, 0xFA,             /* BNE loop       */
            0x4C, 0x08, 0x10,       /* JMP *  (hang)  */
        };
        run(p, sizeof p, 4000);
        printf("1. 6502 counter:   $0200=$%02X X=$%02X PC=$%04X\n", mem_peek(0x200), cpu65.x, cpu65.pc);
        CHECK(mem_peek(0x200) == 0xFF, "counter did not reach $FF");
        CHECK(cpu65.pc == 0x1008,      "did not end in the hang loop");
    }

    /* --- 2. 65CE02: Z, INW, 16-bit SP ----------------------------------- */
    {
        static const uint8_t p[] = {
            0xA3, 0x42,             /* LDZ #$42                       */
            0xA9, 0xFF, 0x85, 0x10, /* LDA #$FF ; STA $10             */
            0xA9, 0x00, 0x85, 0x11, /* LDA #0   ; STA $11   -> $10/11 = $00FF */
            0xE3, 0x10,             /* INW $10              -> $0100  */
            0x4C, 0x0C, 0x10,       /* JMP * */
        };
        run(p, sizeof p, 200);
        uint16_t w = mem_peek(0x10) | (mem_peek(0x11) << 8);
        printf("2. 65CE02:         Z=$%02X  INW($10)=$%04X\n", cpu65.z, w);
        CHECK(cpu65.z == 0x42, "LDZ failed");
        CHECK(w == 0x0100,     "INW failed (%04X)", w);
    }

    /* --- 3. 45GS02: 32-bit flat pointer + Q register --------------------- */
    {
        static const uint8_t p[] = {
            /* pointer at $20..$23 = $00003000 ; data at $3000 = $11 $22 $33 $44 */
            0xA9, 0x00, 0x85, 0x20,
            0xA9, 0x30, 0x85, 0x21,
            0xA9, 0x00, 0x85, 0x22,
            0xA9, 0x00, 0x85, 0x23,
            0xA3, 0x00,                   /* LDZ #0                         */
            0xEA, 0xB2, 0x20,             /* NOP ; LDA ($20),Z  => LDA [$20],Z  (flat) */
            0x8D, 0x00, 0x04,             /* STA $0400   -> expect $11      */
            0xA3, 0x02,                   /* LDZ #2                         */
            0xEA, 0xB2, 0x20,             /* LDA [$20],Z -> $33             */
            0x8D, 0x01, 0x04,             /* STA $0401                      */
            /* Q ops: LDQ $3000 (abs) via NEG NEG LDA $3000 ; then STQ $0500 */
            0x42, 0x42, 0xAD, 0x00, 0x30, /* NEG NEG LDA $3000 = LDQ $3000  */
            0x42, 0x42, 0x8D, 0x00, 0x05, /* NEG NEG STA $0500 = STQ $0500  */
            0x4C, 0x2B, 0x10,             /* JMP *                          */
        };
        mem_init();
        mem_load(0x1000, p, sizeof p);
        static const uint8_t d[] = { 0x11, 0x22, 0x33, 0x44 };
        mem_load(0x3000, d, sizeof d);
        mem_poke(0xFFFC, 0x00); mem_poke(0xFFFD, 0x10);
        cpu65_reset();
        cpu65_step(400);
        printf("3. 45GS02 flat:    [$20],Z=0 -> $%02X   [$20],Z=2 -> $%02X\n", mem_peek(0x400), mem_peek(0x401));
        printf("   45GS02 Q:       LDQ $3000 -> A=%02X X=%02X Y=%02X Z=%02X ; STQ $0500 -> %02X %02X %02X %02X\n",
               cpu65.a, cpu65.x, cpu65.y, cpu65.z,
               mem_peek(0x500), mem_peek(0x501), mem_peek(0x502), mem_peek(0x503));
        CHECK(mem_peek(0x400) == 0x11, "flat LDA [$20],Z (Z=0)");
        CHECK(mem_peek(0x401) == 0x33, "flat LDA [$20],Z (Z=2)");
        CHECK(cpu65.a == 0x11 && cpu65.x == 0x22 && cpu65.y == 0x33 && cpu65.z == 0x44, "LDQ");
        CHECK(mem_peek(0x500) == 0x11 && mem_peek(0x503) == 0x44, "STQ");
    }

    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails);
    return fails != 0;
}
