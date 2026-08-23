/* K-01 bank registers and K-02 far-call gate. Programs hand-assembled, as in maptest. */
#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static void boot(const uint8_t *prog, size_t len, int cycles)
{
    mem_reset();
    mem_load(0xC000, prog, len);
    mem_poke(0xFFFC, 0x00); mem_poke(0xFFFD, 0xC0);
    cpu65_reset();
    cpu65_step(cycles);
}
static void w32(uint16_t r, uint32_t v) { for (int i = 0; i < 4; i++) io_write(r + i, (uint8_t)(v >> (8 * i))); }
static uint32_t r32(uint16_t r) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)io_read(r + i) << (8 * i); return v; }
int main(void)
{
    printf("K4510 K-01/K-02: bank registers and the far-call gate\n");
    CHECK(mem_init() == 0, "mem_init");

    /* 1. bank register from the host side: block 2 ($4000) -> 1 MB + $100 (byte granularity) */
    mem_reset();
    w32(IO_BANK + 4 * 2, 0x00100100);
    CHECK(mem_cpu_to_phys(0x4000) == 0x00100100, "bank 2 base: $4000 -> $%06X", mem_cpu_to_phys(0x4000));
    CHECK(mem_cpu_to_phys(0x5FFF) == 0x001020FF, "bank 2 end");
    CHECK(mem_cpu_to_phys(0x3FFF) == 0x3FFF && mem_cpu_to_phys(0x6000) == 0x6000, "neighbours untouched");
    CHECK(r32(IO_BANK + 8) == 0x00100100, "read back $%08X", r32(IO_BANK + 8));
    CHECK(io_read(IO_BANK + 0x20) == 0x04, "bank mask $%02X", io_read(IO_BANK + 0x20));
    mem_poke(0x00100123, 0x5A);
    CHECK(cpu65_read_callback(0x4023) == 0x5A, "CPU reads through the bank");
    cpu65_write_callback(0x4024, 0xA5);
    CHECK(mem_peek(0x00100124) == 0xA5, "CPU writes through the bank");
    io_write(IO_BANK + 8 + 3, 0xFF);
    CHECK(mem_cpu_to_phys(0x4000) == 0x4000 && r32(IO_BANK + 8) == 0xFFFFFFFF, "off via byte 3 bit 7");
    printf("1. bank register set/read/off: ok\n");

    /* 2. from the CPU: STA to the register bytes, then read the banked memory; MAP clears it */
    {
        static const uint8_t p[] = {
            0xA9, 0x00, 0x8D, 0x04, 0xD6,       /* STA $D604 : bank 1 base = $00200000 */
            0xA9, 0x00, 0x8D, 0x05, 0xD6,
            0xA9, 0x20, 0x8D, 0x06, 0xD6,
            0xA9, 0x00, 0x8D, 0x07, 0xD6,
            0xAD, 0x34, 0x22,                   /* LDA $2234 -> phys $200234 */
            0x8D, 0x00, 0x04,                   /* STA $0400 */
            0xA9, 0x00, 0xA2, 0x00, 0xA0, 0x00, 0xA3, 0x00, 0x5C, 0xEA,   /* MAP all off; EOM */
            0xAD, 0x34, 0x22,                   /* LDA $2234 -> plain RAM now */
            0x8D, 0x01, 0x04,                   /* STA $0401 */
            0x4C, 0x2A, 0xC0,                   /* JMP * */
        };
        mem_reset(); mem_poke(0x200234, 0x77); mem_poke(0x2234, 0x11);
        boot(p, sizeof p, 400);
        CHECK(mem_peek(0x400) == 0x77, "CPU-set bank read $%02X", mem_peek(0x400));
        CHECK(mem_peek(0x401) == 0x11 && mem_bank_mask() == 0, "MAP off cleared the bank ($%02X, mask %d)", mem_peek(0x401), mem_bank_mask());
        printf("2. bank from the CPU, cleared by MAP: ok\n");
    }

    /* 3. far-call gate: slot 0 banks 1 MB into block 2 and calls $4000; nested call via slot 1/2 */
    {
        static const uint8_t main_p[] = {
            0xA9, 0x00, 0x8D, 0x80, 0xDF,       /* FARTAB = $0200 */
            0xA9, 0x02, 0x8D, 0x81, 0xDF,
            0xA9, 0x00, 0x8D, 0x82, 0xDF, 0x8D, 0x83, 0xDF,
            0xA9, 0x11,                         /* LDA #$11 : passes through the gate */
            0x20, 0x00, 0xDF,                   /* JSR slot 0 */
            0x8D, 0x00, 0x04,                   /* STA $0400 : A from the callee */
            0xAD, 0x00, 0x40, 0x8D, 0x01, 0x04, /* LDA $4000 ; STA $0401 : block 2 restored? */
            0x20, 0x04, 0xDF,                   /* JSR slot 1 (nested) */
            0x8D, 0x02, 0x04,                   /* STA $0402 */
            0xAD, 0x84, 0xDF, 0x8D, 0x03, 0x04, /* depth -> $0403 */
            0x4C, 0x2C, 0xC0,                   /* JMP * */
        };
        static const uint8_t f0[] = { 0x69, 0x11, 0xEE, 0x10, 0x04, 0x60 };            /* ADC #$11 ; INC $0410 ; RTS */
        static const uint8_t f1[] = { 0x20, 0x08, 0xDF, 0x69, 0x01, 0x60 };            /* JSR slot 2 ; ADC #1 ; RTS  (at $4000 in block 2) */
        static const uint8_t f2[] = { 0xA9, 0x30, 0x60 };                              /* LDA #$30 ; RTS  (at $2000 in block 1) */
        static const uint8_t tab[] = {
            0x00, 0x00, 0x10, 0x00, 2, 0, 0x00, 0x40,     /* slot 0: $100000, block 2, entry $4000 */
            0x00, 0x00, 0x11, 0x00, 2, 0, 0x00, 0x40,     /* slot 1: $110000, block 2, entry $4000 */
            0x00, 0x00, 0x12, 0x00, 1, 0, 0x00, 0x20,     /* slot 2: $120000, block 1, entry $2000 */
        };
        static const uint8_t idle[] = { 0x4C, 0x00, 0xC0 };
        uint16_t sp0;
        boot(idle, sizeof idle, 50); sp0 = cpu65.s | cpu65.sphi;
        mem_reset();
        mem_load(0x100000, f0, sizeof f0); mem_load(0x110000, f1, sizeof f1); mem_load(0x120000, f2, sizeof f2);
        mem_load(0x200, tab, sizeof tab); mem_poke(0x4000, 0xEE); for (int i = 0x400; i < 0x420; i++) mem_poke(i, 0);
        boot(main_p, sizeof main_p, 600);
        CHECK(mem_peek(0x400) == 0x22, "slot 0 returned A=$%02X (want $22)", mem_peek(0x400));
        CHECK(mem_peek(0x410) == 1, "callee ran once");
        CHECK(mem_peek(0x401) == 0xEE, "block 2 restored after return ($%02X)", mem_peek(0x401));
        CHECK(mem_peek(0x402) == 0x31, "nested far call A=$%02X (want $31)", mem_peek(0x402));
        CHECK(mem_peek(0x403) == 0 && mem_bank_mask() == 0, "depth 0, banks clear after nesting (depth %d mask %d)", mem_peek(0x403), mem_bank_mask());
        CHECK(far_err == 0, "no gate error (%d)", far_err);
        CHECK((cpu65.s | cpu65.sphi) == sp0, "stack balanced (SP=$%04X, was $%04X)", cpu65.s | cpu65.sphi, sp0);
        printf("3. far-call gate: call, return, bank restore, nesting, A passes: ok\n");
    }

    /* 4. return gate with nothing to return from: acts as RTS, sets error 2 */
    {
        static const uint8_t p[] = { 0x20, 0xF0, 0xDF, 0xAD, 0x85, 0xDF, 0x8D, 0x00, 0x04, 0x4C, 0x09, 0xC0 };
        boot(p, sizeof p, 200);
        CHECK(mem_peek(0x400) == 2, "underflow error %d", mem_peek(0x400));
        printf("4. underflow handled: ok\n");
    }

    printf(fails ? "%d FAILED\n" : "ALL OK\n", fails);
    return fails != 0;
}
