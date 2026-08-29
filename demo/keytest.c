/* K4510: keyboard test. Asks for every key of a C64 keyboard in matrix
 * order, checks the code the $D100 device delivers against what the GPIO
 * driver (pi/c64kbd.cpp) should produce, checks the three modifiers through
 * the status register, tries shifted combinations, then prints a summary
 * and saves fs/keytest.txt. A key that never arrives times out after 8 s.
 * Works with a USB keyboard too (then the expected codes are the same). */
#include "k4510.h"

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);

static void print(const char *s) { while (*s) rom_chrout(*s++); }
static void nl(void) { rom_chrout('\n'); }
static void hex2(uint8_t v) { static const char h[] = "0123456789ABCDEF"; rom_chrout(h[v >> 4]); rom_chrout(h[v & 15]); }

static char report[6000]; static uint16_t rlen;
static void rep(const char *s) { while (*s && rlen < sizeof report - 2) report[rlen++] = *s++; }
static void repc(char c) { if (rlen < sizeof report - 2) report[rlen++] = c; }
static void rephex(uint8_t v) { static const char h[] = "0123456789ABCDEF"; repc(h[v >> 4]); repc(h[v & 15]); }
static void both(const char *s) { print(s); rep(s); }

static uint8_t wait_key(uint8_t seconds)
{
    uint8_t f0 = REG(SYS + 0x0D), k; uint16_t frames = 0;
    for (;;) {
        k = rom_getin(); if (k) return k;
        if (REG(SYS + 0x0D) != f0) { f0 = REG(SYS + 0x0D); if (++frames >= (uint16_t)seconds * 60) return 0; }
    }
}
static void flush_keys(void) { while (rom_getin()) ; }

typedef struct { const char *label; uint8_t code; } key_t;
static const key_t keys[] = {
    {"INST/DEL", 0x08}, {"RETURN", 0x0D}, {"CRSR RIGHT", 0x83}, {"F7", 0x96}, {"F1", 0x90}, {"F3", 0x92}, {"F5", 0x94}, {"CRSR DOWN", 0x81},
    {"3", '3'}, {"W", 'w'}, {"A", 'a'}, {"4", '4'}, {"Z", 'z'}, {"S", 's'}, {"E", 'e'},
    {"5", '5'}, {"R", 'r'}, {"D", 'd'}, {"6", '6'}, {"C", 'c'}, {"F", 'f'}, {"T", 't'}, {"X", 'x'},
    {"7", '7'}, {"Y", 'y'}, {"G", 'g'}, {"8", '8'}, {"B", 'b'}, {"H", 'h'}, {"U", 'u'}, {"V", 'v'},
    {"9", '9'}, {"I", 'i'}, {"J", 'j'}, {"0", '0'}, {"M", 'm'}, {"K", 'k'}, {"O", 'o'}, {"N", 'n'},
    {"+", '+'}, {"P", 'p'}, {"L", 'l'}, {"-", '-'}, {".", '.'}, {":", ':'}, {"@", '@'}, {",", ','},
    {"POUND", '\\'}, {"*", '*'}, {";", ';'}, {"CLR/HOME", 0x84}, {"=", '='}, {"UP ARROW", '^'}, {"/", '/'},
    {"1", '1'}, {"LEFT ARROW", '`'}, {"2", '2'}, {"SPACE", ' '}, {"Q", 'q'}, {"RUN/STOP", 0x1B},
};
static const key_t shifted[] = {
    {"SHIFT+3 (#)", '#'}, {"SHIFT+A", 'A'}, {"SHIFT+2 (\")", '"'}, {"SHIFT+CRSR DOWN (up)", 0x80}, {"SHIFT+CRSR RIGHT (left)", 0x82},
    {"SHIFT+F1 (F2)", 0x91}, {"SHIFT+INST/DEL (DEL)", 0x89}, {"SHIFT+CLR/HOME (END)", 0x85}, {"SHIFT+: ([)", '['}, {"SHIFT+; (])", ']'},
    {"CTRL+A (code 01)", 0x01},
};

static uint8_t test_key(const key_t *k)
{
    uint8_t got;
    print("  press "); print(k->label); print(" ");
    rep("  "); rep(k->label); rep(": ");
    flush_keys();
    got = wait_key(8);
    if (!got) { both("NO KEY (timeout)"); nl(); repc('\n'); return 0; }
    print("-> $"); hex2(got); rep("got $"); rephex(got);
    if (got >= 0x20 && got < 0x7F) { print(" '"); rom_chrout(got); print("'"); repc(' '); repc('\''); repc(got); repc('\''); }
    if (got == k->code) { both("  OK"); nl(); repc('\n'); return 1; }
    print("  FAIL, expected $"); hex2(k->code); rep("  FAIL, expected $"); rephex(k->code); nl(); repc('\n');
    return 0;
}

static uint8_t test_mod(const char *label, uint8_t bit)
{
    uint8_t f0 = REG(SYS + 0x0D); uint16_t frames = 0;
    print("  hold "); print(label); print(" ... "); rep("  "); rep(label); rep(": ");
    for (;;) {
        if (REG(KBDST) & bit) { both("seen, OK"); nl(); repc('\n'); while (REG(KBDST) & bit) ; return 1; }
        if (REG(SYS + 0x0D) != f0) { f0 = REG(SYS + 0x0D); if (++frames >= 8 * 60) break; }
    }
    both("NOT SEEN (timeout)"); nl(); repc('\n'); return 0;
}

static void save_report(void)
{
    static const char name[] = "keytest.txt";
    w32(0xD304u, (uint16_t)name); w32(0xD308u, (uint16_t)report); w32(0xD30Cu, rlen);
    REG(0xD300u) = 10;
    if (REG(0xD301u) == 0) print("report saved as fs/keytest.txt"); else print("could not save the report");
    nl();
}

void main(void)
{
    uint8_t i, ok = 0, n = 0, mods = 0;
    rom_chrout(12);
    print("K4510 keyboard test -- press each key as asked (8 s each)"); nl(); nl();
    rep("K4510 keyboard test\n\nkeys:\n");
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++) { n++; ok += test_key(&keys[i]); }
    nl(); print("modifiers (status register $D101):"); nl(); rep("\nmodifiers:\n");
    mods += test_mod("LEFT or RIGHT SHIFT", 0x01);
    mods += test_mod("CTRL", 0x02);
    mods += test_mod("C= (Commodore)", 0x04);
    nl(); print("shifted combinations:"); nl(); rep("\nshifted:\n");
    for (i = 0; i < sizeof shifted / sizeof shifted[0]; i++) { n++; ok += test_key(&shifted[i]); }
    nl();
    print("result: "); rep("\nresult: ");
    { char b[8]; uint8_t v = ok; b[0] = '0' + v / 10; b[1] = '0' + v % 10; b[2] = '/'; b[3] = '0' + n / 10; b[4] = '0' + n % 10; b[5] = 0; both(b); }
    both(" keys, "); repc('0' + mods); rom_chrout('0' + mods); both("/3 modifiers"); nl(); repc('\n');
    save_report();
    nl(); print("free typing -- codes echo in hex, RUN/STOP ends:"); nl();
    flush_keys();
    for (;;) {
        uint8_t k = wait_key(60);
        if (!k || k == 0x1B) break;
        print("$"); hex2(k); if (k >= 0x20 && k < 0x7F) { print(" '"); rom_chrout(k); print("'"); } print("   ");
    }
    nl();
}
