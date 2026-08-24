/* Stage 2 ROM: boots, shell works, LOAD from the host filesystem, DIR, run. */
#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/vicke.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static uint8_t fb[640 * 480];
static void frames(int n) { for (; n; n--) { vicke_begin_frame(fb, 640); for (int y = 0; y < 480; y++) { cpu65.irqLevel = vicke_irq() ? 1 : 0; cpu65_step(40500000 / 60 / 480); vicke_line(y); } vicke_end_frame(); } }
static void type(const char *s) { for (; *s; s++) { kbd_push(*s == '\n' ? 0x0D : (uint8_t)*s); frames(1); } frames(3); }
static void row(int r, char *out) { for (int c = 0; c < 80; c++) { uint8_t ch = mem_peek(0x30000 + (r * 80 + c) * 4); out[c] = (ch >= 0x20 && ch < 0x7F) ? ch : '.'; } out[80] = 0; for (int i = 79; i >= 0 && out[i] == ' '; i--) out[i] = 0; }
static int findsub(const char *sub) { char r[81]; for (int i = 0; i < 60; i++) { row(i, r); if (strstr(r, sub)) return i; } return -1; }
static int find(const char *pre) { char r[81]; for (int i = 0; i < 60; i++) { const char *q; row(i, r); q = r; while (*q == ' ') q++; if (!strncmp(q, pre, strlen(pre))) return i; } return -1; }   /* skips the margin */
int main(void)
{
    uint8_t font[2048]; FILE *f = fopen("data/font8.bin", "rb"); fread(font, 1, 2048, f); fclose(f);
    mem_init(); fs_set_root("fs"); mem_load(K4510_FONT8_PHYS, font, 2048);
    CHECK(mem_load_rom("rom/kernal.bin") == 24576, "rom");
    cpu65_reset(); frames(40);      /* boot + the !BOOT grace window */
    char r[81]; row(0, r); printf("banner: %s\n", r);
    CHECK(findsub("BMC-K4510") >= 0, "banner");
    CHECK(findsub("/]") >= 0, "prompt");
    type("mon E000.E00F\n");
    CHECK(find("0000E000: 78 D8 A2 FF 9A") >= 0, "examine ROM (cc65 crt0: SEI CLD LDX TXS)");
    type("load hello.txt 6000\n");
    CHECK(find("loaded 31 bytes at 00006000") >= 0, "LOAD message");
    CHECK(memcmp(&k4510_ram[0x6000], "hello from", 10) == 0, "file landed in RAM");
    type("mon 6000.6009\n");
    CHECK(find("00006000: 68 65 6C 6C 6F") >= 0, "examine loaded bytes");
    /* user program: LDA #$42 ; STA $5F00 ; RTS  at $3000, run with 3000R */
    type("mon 3000:A9 42 8D 00 5F 60\n");
    type("mon 3000R\n");
    CHECK(mem_peek(0x5F00) == 0x42, "3000R ran user code and returned to the shell");
    type("fill 1000000.100000F AA\n");
    CHECK(mem_peek(0x1000000) == 0xAA && mem_peek(0x100000F) == 0xAA && mem_peek(0x1000010) != 0xAA, "FILL at 16 MB through DMA");
    type("mon 1000000.1000003\n");
    CHECK(find("01000000: AA AA AA AA") >= 0, "examine beyond 64 KB");
    type("mon 1000004:55\n");
    CHECK(mem_peek(0x1000004) == 0x55, "store beyond 64 KB");
    type("type hello.txt\n");
    CHECK(find("hello from") >= 0, "TYPE prints the file");
    type("info -cm\n");
    CHECK(findsub("measured 40.") >= 0, "INFO -c measures the clock");
    CHECK(findsub("ROM $A000-$FFFF (24 KB)") >= 0, "INFO -m reports the ROM");
    type("info -t\n");
    CHECK(find("TIME") >= 0 && findsub("frames)") >= 0, "INFO -t");
    type("load balls.prg\n");
    CHECK(findsub("4646 bytes at $00006000, run address 6000") >= 0 || findsub("bytes at 00006000, run address 6000") >= 0, "LOAD honours the .prg header");
    CHECK(mem_peek(0x6000) == 0xA9, "program image landed at $6000 without its header");
    type("run balls.prg\n");
    frames(30);
    CHECK((io_read(IO_VICKE + 0x0E) & 1) == 1, "balls.prg is running (sprites on)");
    type("x\n");
    frames(10);
    CHECK((io_read(IO_VICKE + 0x0E) & 1) == 0 && findsub("/]") >= 0, "a key returns to the shell and video is restored");
    type("dir\n");
    CHECK(findsub("hello.txt") >= 0, "DIR lists hello.txt");
    CHECK(findsub("/]") >= 0, "prompt after commands");
    printf("screen:\n"); for (int i = 0; i < 60; i++) { row(i, r); if (*r) printf("  |%s\n", r); }
    printf(fails ? "\n%d FAILED\n" : "\nALL OK\n", fails); return fails != 0;
}
