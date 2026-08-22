/* Step 3/4 headless: boot the Wozmon ROM, type a command, read the screen.
 *
 *   "FF00.FF0F"  -> Wozmon dumps 16 bytes of ROM
 *   "0300:41 42 43" then "300.302" -> store and read back
 *   "R" at an address -> runs user code (we plant LDA #$99 / STA $0700 / loop)
 */
#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/vicke.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void type(const char *s)
{
    for (; *s; s++) {
        kbd_push((uint8_t)*s);
        cpu65_step(20000);                 /* let the ROM consume it */
    }
}

static void screen_row(int row, char *out)
{
    for (int c = 0; c < VICKE_COLS; c++) {
        uint8_t ch = mem_peek(VICKE_SCREEN_BASE + row * VICKE_COLS + c);
        out[c] = (ch >= 0x20 && ch < 0x7F) ? ch : '.';
    }
    out[VICKE_COLS] = 0;
    /* trim */
    for (int i = VICKE_COLS - 1; i >= 0 && out[i] == ' '; i--) out[i] = 0;
}

static int find_line(const char *prefix)
{
    char r[VICKE_COLS + 1];
    for (int i = 0; i < VICKE_ROWS; i++) { screen_row(i, r); if (strncmp(r, prefix, strlen(prefix)) == 0) return 1; }
    return 0;
}

static void dump(int rows)
{
    char r[VICKE_COLS + 1];
    for (int i = 0; i < rows; i++) { screen_row(i, r); printf("  |%s\n", r); }
}

int main(void)
{
    mem_init();
    if (mem_load_rom("rom/wozmon.bin") != 4096) { printf("need rom/wozmon.bin\n"); return 1; }
    cpu65_reset();
    cpu65_step(200000);                    /* CLS + two CRs */
    printf("after reset: PC=$%04X\n", cpu65.pc);
    CHECK(cpu65.pc >= 0xF000, "PC not in ROM");

    type("FF00.FF0F\r");
    cpu65_step(200000);
    /* Wozmon prints "\\" on reset (authentic), then our command echo, then the dump. */
    CHECK(find_line("\\"), "no reset backslash");
    CHECK(find_line("FF00.FF0F"), "command echo missing");
    CHECK(find_line("FF00: 00 00 00 00 00 00 00 00"), "dump line missing");

    /* store then examine */
    type("0300:41 42 43\r");
    type("300.302\r");
    cpu65_step(200000);
    CHECK(mem_peek(0x300) == 0x41 && mem_peek(0x302) == 0x43, "STOR did not write RAM");
    CHECK(find_line("0300: 41 42 43"), "XAM readback line not found");

    /* run user code: at $0400: LDA #$99 ; STA $0700 ; JMP * */
    type("0400:A9 99 8D 00 07 4C 05 04\r");
    type("400R\r");
    cpu65_step(200000);
    CHECK(mem_peek(0x700) == 0x99, "R did not run user code ($0700=%02X)", mem_peek(0x700));
    CHECK(cpu65.pc == 0x0405,       "user code not in its hang loop (PC=%04X)", cpu65.pc);

    printf("\nscreen after the session:\n"); dump(16);
    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails);
    return fails != 0;
}
