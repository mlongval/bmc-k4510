/* BMC-K4510 system ROM, Stage 2. cc65, 65C02 subset of the 45GS02.
 *
 * A colour text terminal on VICKe text32, a keyboard driver, the host
 * filesystem, and a shell that keeps Wozmon's syntax and adds files.
 * Everything the machine can do from software is reached through the
 * registers in io.h; this file is the first user of them.
 */
#include <stdint.h>
#include <string.h>

/* ---- hardware (mirrors core/io.h and core/vicke.h) -------------------- */
#define REG(a) (*(volatile uint8_t *)(a))
#define VICKE  0xD000u
#define KBD    0xD100u
#define KBDST  0xD101u
#define DMA    0xD200u
#define FS     0xD300u
#define SID0   0xD400u

#define SCREEN   0x0800u              /* text32: 80x60 cells x 4 bytes = 19200 -> $0800-$5300 */
#define FONT     0x010000UL           /* placed by the loader */
#define COLS 80
#define ROWS 60

#define C_BG   0x06   /* VIC-II blue     */
#define C_FG   0x0E   /* light blue      */
#define C_HI   0x01   /* white           */
#define C_ERR  0x0A   /* light red       */
#define C_DIM  0x0C   /* grey            */

/* ---- terminal ---------------------------------------------------------- */
static uint8_t cx, cy, fg = C_FG, bg = C_BG;
static uint8_t cursor_on;

static void w32(uint16_t r, uint32_t v) { REG(r) = v; REG(r + 1) = v >> 8; REG(r + 2) = v >> 16; REG(r + 3) = v >> 24; }
static void w16(uint16_t r, uint16_t v) { REG(r) = v; REG(r + 1) = v >> 8; }

static uint8_t *cell(uint8_t x, uint8_t y) { return (uint8_t *)(SCREEN + ((uint16_t)y * COLS + x) * 4); }

static void draw_cursor(uint8_t on)
{
    uint8_t *c = cell(cx, cy);
    c[1] = on ? 0x80 : 0x00;                 /* reverse bit = cursor */
}

static void cls(void)
{
    /* DMA fill is byte-wide; seed row 0 with the 4-byte cell, DMA-copy it to each row */
    uint8_t *c = (uint8_t *)SCREEN; uint8_t i;
    for (i = 0; i < COLS; i++) { c[0] = ' '; c[1] = 0; c[2] = fg; c[3] = bg; c += 4; }
    w32(DMA + 0, SCREEN); w32(DMA + 8, COLS * 4);
    for (i = 1; i < ROWS; i++) { w32(DMA + 4, SCREEN + (uint16_t)i * COLS * 4); REG(DMA + 12) = 1; }
    cx = cy = 0;
}

static void scroll(void)
{
    uint8_t *c; uint8_t i;
    w32(DMA + 0, SCREEN + COLS * 4); w32(DMA + 4, SCREEN); w32(DMA + 8, (uint32_t)(ROWS - 1) * COLS * 4);
    REG(DMA + 12) = 1;
    c = cell(0, ROWS - 1);
    for (i = 0; i < COLS; i++) { c[0] = ' '; c[1] = 0; c[2] = fg; c[3] = bg; c += 4; }
}

static void newline(void)
{
    cx = 0;
    if (++cy >= ROWS) { cy = ROWS - 1; scroll(); }
}

void __fastcall__ k_chrout(uint8_t ch)
{
    uint8_t *c;
    draw_cursor(0);
    if (ch == '\r' || ch == '\n') { newline(); return; }
    if (ch == 8) { if (cx) { cx--; c = cell(cx, cy); c[0] = ' '; c[1] = 0; } return; }
    if (ch == 12) { cls(); return; }
    if (ch < 0x20) return;
    c = cell(cx, cy); c[0] = ch; c[1] = 0; c[2] = fg; c[3] = bg;
    if (++cx >= COLS) newline();
}

static void puts_(const char *s) { while (*s) k_chrout(*s++); }
static void puthex(uint8_t v) { static const char h[] = "0123456789ABCDEF"; k_chrout(h[v >> 4]); k_chrout(h[v & 15]); }
static void putdec(uint32_t v) { char b[11]; uint8_t i = 10; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); puts_(&b[i]); }

/* ---- keyboard ---------------------------------------------------------- */
uint8_t k_getin(void) { return (REG(KBDST) & 0x80) ? REG(KBD) : 0; }

static volatile uint8_t ticks;
void k_irq_handler(void)
{
    uint8_t st = REG(VICKE + 4);
    if (st & 1) { ticks++; if ((ticks & 31) == 0) { cursor_on ^= 1; draw_cursor(cursor_on); } }
    REG(VICKE + 4) = st;                      /* ack everything */
}

uint8_t k_chrin(void)
{
    uint8_t k;
    while (!(k = k_getin())) ;
    return k;
}

static uint8_t readline(char *buf, uint8_t max)
{
    uint8_t n = 0, k;
    for (;;) {
        draw_cursor(1);
        k = k_chrin();
        if (k == 13) { draw_cursor(0); buf[n] = 0; newline(); return n; }
        if (k == 8) { if (n) { n--; k_chrout(8); } continue; }
        if (k == 27) { while (n) { n--; k_chrout(8); } continue; }
        if (k >= 0x20 && k < 0x7F && n < max - 1) { buf[n++] = k; k_chrout(k); }
    }
}

/* ---- filesystem -------------------------------------------------------- */
static uint8_t fs_cmd(uint8_t cmd) { REG(FS) = cmd; return REG(FS + 1); }
static void fs_name(const char *name) { w32(FS + 4, (uint16_t)name); }

/* jump-table entry points use zero page $F0.. as the parameter block */
#define P_NAME  (*(volatile uint16_t *)0xF0)
#define P_ADDR  (*(volatile uint32_t *)0xF2)
#define P_LEN   (*(volatile uint32_t *)0xF6)
uint8_t k_load(void) { uint8_t st; w32(FS + 4, P_NAME); w32(FS + 8, P_ADDR); st = fs_cmd(9); P_LEN = *(volatile uint32_t *)(FS + 12); return st; }
uint8_t k_save(void) { w32(FS + 4, P_NAME); w32(FS + 8, P_ADDR); w32(FS + 12, P_LEN); return fs_cmd(10); }

/* ---- shell ------------------------------------------------------------- */
static char line[96];
static uint32_t xam;          /* last opened address */
static uint8_t mode;          /* 0 xam, 1 store, 2 block */

static uint8_t ishex(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
static uint8_t hexval(char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static uint32_t parsehex(const char **p, uint8_t *digits)
{
    uint32_t v = 0; *digits = 0;
    while (ishex(**p)) { v = (v << 4) | hexval(**p); (*p)++; (*digits)++; }
    return v;
}
static uint8_t peek(uint32_t a) { return a < 0x10000UL ? *(uint8_t *)(uint16_t)a : 0; }   /* 64 KB view for now */
static void poke(uint32_t a, uint8_t v) { if (a < 0x10000UL) *(uint8_t *)(uint16_t)a = v; }

static void dump(uint32_t from, uint32_t to)
{
    uint8_t n = 0;
    for (; from <= to; from++) {
        if (n == 0) { puthex(from >> 16); puthex(from >> 8); puthex(from); puts_(": "); }
        puthex(peek(from)); k_chrout(' ');
        if (++n == 16) { n = 0; newline(); }
        if (from == 0xFFFFFFFFUL) break;
    }
    if (n) newline();
}

static void error(const char *m) { uint8_t o = fg; fg = C_ERR; puts_(m); newline(); fg = o; }

static void cmd_dir(void)
{
    char name[64]; uint16_t count = 0;
    if (fs_cmd(6)) { error("dir: no device"); return; }
    for (;;) {
        w32(FS + 8, (uint16_t)name);
        if (fs_cmd(7)) break;
        puts_(name); { uint8_t i = strlen(name); while (i++ < 24) k_chrout(' '); }
        putdec(*(volatile uint32_t *)(FS + 16)); newline(); count++;
    }
    putdec(count); puts_(" file(s)"); newline();
}

static void cmd_load(const char *p)
{
    char name[64]; uint8_t i = 0, d; uint32_t addr = 0x1000;
    while (*p == ' ') p++;
    while (*p && *p != ' ' && i < 63) name[i++] = *p++;
    name[i] = 0;
    while (*p == ' ') p++;
    if (*p) addr = parsehex(&p, &d);
    if (!i) { error("load: name?"); return; }
    fs_name(name); w32(FS + 8, addr);
    if (fs_cmd(9)) { error("load: not found"); return; }
    puts_("loaded "); putdec(*(volatile uint32_t *)(FS + 12)); puts_(" bytes at "); puthex(addr >> 8); puthex(addr); newline();
    xam = addr;
}

static void cmd_save(const char *p)
{
    char name[64]; uint8_t i = 0, d; uint32_t from, to;
    while (*p == ' ') p++;
    while (*p && *p != ' ' && i < 63) name[i++] = *p++;
    name[i] = 0;
    while (*p == ' ') p++;
    from = parsehex(&p, &d); if (!d || *p != '.') { error("save: name from.to"); return; }
    p++; to = parsehex(&p, &d); if (!d || to < from) { error("save: name from.to"); return; }
    fs_name(name); w32(FS + 8, from); w32(FS + 12, to - from + 1);
    if (fs_cmd(10)) { error("save: failed"); return; }
    puts_("saved "); putdec(to - from + 1); puts_(" bytes"); newline();
}

typedef void (*fn_t)(void);

static void shell_line(const char *p)
{
    uint8_t d; uint32_t v;
    while (*p == ' ') p++;
    if (!*p) return;
    if (!strncmp(p, "DIR", 3) || !strncmp(p, "dir", 3)) { cmd_dir(); return; }
    if (!strncmp(p, "LOAD", 4) || !strncmp(p, "load", 4)) { cmd_load(p + 4); return; }
    if (!strncmp(p, "SAVE", 4) || !strncmp(p, "save", 4)) { cmd_save(p + 4); return; }
    if (!strncmp(p, "CLS", 3) || !strncmp(p, "cls", 3)) { cls(); return; }
    if (!strncmp(p, "HELP", 4) || !strncmp(p, "help", 4)) {
        puts_("addr            examine    addr.addr   block    addr:b b b  store"); newline();
        puts_("addrR           run        LOAD name [addr]       SAVE name from.to"); newline();
        puts_("DIR  CLS  HELP  --  hex is 45GS02-flat, 28-bit"); newline();
        return;
    }
    /* Wozmon grammar */
    mode = 0;
    for (;;) {
        while (*p == ' ') p++;
        if (!*p) return;
        if (*p == ':') { mode = 1; p++; continue; }
        if (*p == '.') { mode = 2; p++; continue; }
        if (*p == 'R' || *p == 'r') { ((fn_t)(uint16_t)xam)(); return; }
        v = parsehex(&p, &d);
        if (!d) { error("?"); return; }
        if (mode == 1) { poke(xam++, (uint8_t)v); continue; }
        if (mode == 2) { dump(xam, v); xam = v + 1; mode = 0; continue; }
        xam = v; dump(v, v);
    }
}

static void video_init(void)
{
    uint8_t i;
    REG(VICKE + 0) = 0;
    REG(VICKE + 1) = C_BG;
    /* layer 0: text32, 8x8, map SCREEN, glyphs FONT, 80 cells/row */
    w16(VICKE + 0x16, COLS);
    w32(VICKE + 0x1C, SCREEN);
    w32(VICKE + 0x18, FONT);
    w16(VICKE + 0x12, 0); w16(VICKE + 0x14, 0);
    REG(VICKE + 0x11) = 0;
    REG(VICKE + 0x10) = 0x01 | (3 << 1);      /* enable | text32 */
    for (i = 1; i < 4; i++) REG(VICKE + 0x10 + i * 0x10) = 0;
    REG(VICKE + 0x0E) = 0; REG(VICKE + 0x64) = 0;
    REG(VICKE + 5) = 1;                        /* IRQ on vblank */
    REG(VICKE + 0) = 1;
}

int main(void)
{
    video_init();
    cls();
    fg = C_HI; puts_("BMC-K4510   system ROM stage 2   45GS02 / VICKe / SHEILA"); newline();
    fg = C_DIM; puts_("256 MB   4 x SID   host filesystem at $D300   type HELP"); newline(); newline();
    fg = C_FG;
    for (;;) {
        puts_("] ");
        readline(line, sizeof line);
        shell_line(line);
    }
    return 0;
}
