/* BMC-K4510 system ROM, Stage 3. cc65, 65C02 subset of the 45GS02.
 *
 * A colour text terminal on VICKe text32, a keyboard driver, the host
 * filesystem, and a shell that keeps Wozmon's syntax and adds files,
 * 28-bit memory access through DMA, INFO, and a handful of utilities.
 * 24 KB ROM at $A000-$FFFF with the I/O hole at $D000 (rom/k4510.cfg).
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
#define SYS    0xD500u
#define BANK   0xD600u

#define SCREEN   0x030000UL           /* text32 cells, 80x60 x 4 bytes, in far memory: the CPU's 64 KB is for programs */
#define FONT     0x010000UL           /* placed by the loader */
#define USER     0x0800u              /* free RAM for programs: $0800-$9FFF (38 KB); .prg files say where they load */
#define USER_END 0xA000u
#define MAXCOLS 80
#define MAXROWS 60
static uint8_t COLS = 80, ROWS = 60, vmode, margin;   /* MODE 0: 80x60 (640x480)  1: 80x30 (640x240)  2: 40x30 (320x240) */
static uint8_t PCOLS = 80, PROWS = 60;               /* physical text cells; with margin = 1 the terminal uses (PCOLS-1)x(PROWS-1) from (1,1) */
#define OX margin
#define OY margin
#define ROM_VERSION "stage 4"

#define C_BG   0x06   /* VIC-II blue     */
#define C_FG   0x07   /* yellow          */
#define C_HI   0x01   /* white           */
#define C_ERR  0x0A   /* light red       */
#define C_DIM  0x0C   /* grey            */

/* ---- terminal ---------------------------------------------------------- */
static uint8_t cx, cy, fg = C_FG, bg = C_BG;
extern volatile uint8_t ticks, cursor_vis;       /* crt0.s */
extern uint32_t cursor_far;                      /* crt0.s: far address of the cell attribute under the cursor */
uint16_t speed_loop(void);                       /* crt0.s */
void __fastcall__ far_poke(unsigned long a, unsigned char v);   /* crt0.s: 45GS02 flat store */
void __fastcall__ call_prog(unsigned addr);                     /* crt0.s: JSR with the ROM zero page saved around it */

static void w32(uint16_t r, uint32_t v) { REG(r) = v; REG(r + 1) = v >> 8; REG(r + 2) = v >> 16; REG(r + 3) = v >> 24; }
static void w16(uint16_t r, uint16_t v) { REG(r) = v; REG(r + 1) = v >> 8; }
static uint32_t r32(uint16_t r) { return (uint32_t)REG(r) | ((uint32_t)REG(r + 1) << 8) | ((uint32_t)REG(r + 2) << 16) | ((uint32_t)REG(r + 3) << 24); }
static uint16_t r16(uint16_t r) { return (uint16_t)REG(r) | ((uint16_t)REG(r + 1) << 8); }

static uint32_t cell(uint8_t x, uint8_t y) { return SCREEN + ((uint16_t)(y + OY) * PCOLS + x + OX) * 4; }
#define ROWTPL   0x03F000UL           /* far: one blank text row in the current colours */
static uint8_t tpl_fg, tpl_bg, tpl_cols, cellbuf[4];
static void blank_row(uint8_t y)
{
    if (tpl_fg != fg || tpl_bg != bg || tpl_cols != PCOLS) {     /* (re)build the template: one cell, copied across */
        uint8_t i;
        cellbuf[0] = ' '; cellbuf[1] = 0; cellbuf[2] = fg; cellbuf[3] = bg;
        w32(DMA + 0, (uint16_t)cellbuf); w32(DMA + 8, 4);
        for (i = 0; i < PCOLS; i++) { w32(DMA + 4, ROWTPL + (uint16_t)i * 4); REG(DMA + 12) = 1; }
        tpl_fg = fg; tpl_bg = bg; tpl_cols = PCOLS;
    }
    /* a whole physical row (the margin column stays blank because every row is blanked whole) */
    w32(DMA + 0, ROWTPL); w32(DMA + 4, SCREEN + (uint32_t)(y + OY) * PCOLS * 4); w32(DMA + 8, PCOLS * 4); REG(DMA + 12) = 1;
}

static void draw_cursor(uint8_t on)
{
    uint32_t c = cell(cx, cy) + 1;
    cursor_vis = 0;
    far_poke(c, on ? 0x80 : 0x00);           /* reverse bit = cursor; IRQ blinks it */
    cursor_far = c;
    cursor_vis = on;
}

static void cls(void)
{
    uint8_t i;
    for (i = 0; i < PROWS; i++) blank_row(i - OY);       /* every physical row, margins included */
    cx = cy = 0;
}

static void scroll(void)
{
    w32(DMA + 0, SCREEN + (uint32_t)(OY + 1) * PCOLS * 4); w32(DMA + 4, SCREEN + (uint32_t)OY * PCOLS * 4);
    w32(DMA + 8, (uint32_t)(ROWS - 1) * PCOLS * 4);
    REG(DMA + 12) = 1;
    blank_row(ROWS - 1);
}

static void newline(void)
{
    cx = 0;
    if (++cy >= ROWS) { cy = ROWS - 1; scroll(); }
}

void __fastcall__ k_chrout(uint8_t ch)
{
    uint32_t c;
    draw_cursor(0);
    if (ch == '\r' || ch == '\n') { newline(); return; }
    if (ch == 8) { if (cx) { cx--; c = cell(cx, cy); far_poke(c, ' '); far_poke(c + 1, 0); } return; }
    if (ch == 12) { cls(); return; }
    if (ch == 9) { do { k_chrout(' '); } while (cx & 7); return; }
    if (ch < 0x20) return;
    c = cell(cx, cy); far_poke(c, ch); far_poke(c + 1, 0); far_poke(c + 2, fg); far_poke(c + 3, bg);
    if (++cx >= COLS) newline();
}

static void puts_(const char *s) { while (*s) k_chrout(*s++); }
static void puthex(uint8_t v) { static const char h[] = "0123456789ABCDEF"; k_chrout(h[v >> 4]); k_chrout(h[v & 15]); }
static void puthex16(uint16_t v) { puthex(v >> 8); puthex(v); }
static void puthex28(uint32_t v) { puthex(v >> 24); puthex(v >> 16); puthex(v >> 8); puthex(v); }
static void putdec(uint32_t v) { char b[11]; uint8_t i = 10; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); puts_(&b[i]); }
static void putdec2(uint8_t v) { k_chrout('0' + v / 10); k_chrout('0' + v % 10); }
static void pad(uint8_t col) { while (cx < col) k_chrout(' '); }
static void label(const char *s) { uint8_t o = fg; fg = C_HI; puts_(s); fg = o; pad(8); }
static void onoff(uint8_t v) { puts_(v ? "on" : "off"); }

/* ---- keyboard ---------------------------------------------------------- */
/* GETIN shows the cursor while a program waits for a key (BASIC reads this way) */
static void draw_cursor(uint8_t on);
uint8_t k_getin(void)
{
    if (REG(KBDST) & 0x80) { if (cursor_vis) draw_cursor(0); return REG(KBD); }
    if (!cursor_vis) draw_cursor(1);
    return 0;
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

/* jump-table entry points (crt0.s saves and restores the ROM's zero page around
 * each, so a program may own $00-$FF) use $F0.. as the parameter block */
#define P_NAME  (*(volatile uint16_t *)0xF0)
#define P_ADDR  (*(volatile uint32_t *)0xF2)
#define P_LEN   (*(volatile uint32_t *)0xF6)
uint8_t k_load(void) { uint8_t st; w32(FS + 4, P_NAME); w32(FS + 8, P_ADDR); st = fs_cmd(9); P_LEN = r32(FS + 12); return st; }
uint8_t k_save(void) { w32(FS + 4, P_NAME); w32(FS + 8, P_ADDR); w32(FS + 12, P_LEN); return fs_cmd(10); }

/* ---- 28-bit memory through DMA ------------------------------------------ */
static uint8_t dmabuf[16];
static void dma_copy(uint32_t src, uint32_t dst, uint32_t len) { w32(DMA, src); w32(DMA + 4, dst); w32(DMA + 8, len); REG(DMA + 12) = 1; }
static void dma_fill(uint8_t v, uint32_t dst, uint32_t len) { REG(DMA) = v; w32(DMA + 4, dst); w32(DMA + 8, len); REG(DMA + 12) = 2; }
static uint8_t peek(uint32_t a)
{
    if (a < 0x10000UL) return *(uint8_t *)(uint16_t)a;              /* CPU view: I/O and ROM as the CPU sees them */
    dma_copy(a, (uint16_t)dmabuf, 1); return dmabuf[0];
}
static void poke(uint32_t a, uint8_t v)
{
    if (a < 0x10000UL) { *(uint8_t *)(uint16_t)a = v; return; }
    dmabuf[0] = v; dma_copy((uint16_t)dmabuf, a, 1);
}

/* ---- shell ------------------------------------------------------------- */
static char line[96];
static uint32_t xam;          /* last opened address */
static uint8_t mode;          /* 0 xam, 1 store, 2 block */
static char last_name[64]; static uint32_t last_addr, last_len; static uint16_t last_run; static uint8_t last_segs;   /* last LOAD */

static uint8_t ishex(char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
static uint8_t hexval(char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; }
static uint32_t parsehex(const char **p, uint8_t *digits)
{
    uint32_t v = 0; *digits = 0;
    while (ishex(**p)) { v = (v << 4) | hexval(**p); (*p)++; (*digits)++; }
    return v;
}
static void skipsp(const char **p) { while (**p == ' ') (*p)++; }
static uint8_t getname(const char **p, char *name)
{
    uint8_t i = 0;
    skipsp(p);
    while (**p && **p != ' ' && i < 63) name[i++] = *(*p)++;
    name[i] = 0; skipsp(p);
    return i;
}
/* case-insensitive command match; on success *p points past the word */
static uint8_t is_cmd(const char **p, const char *cmd)
{
    const char *s = *p;
    while (*cmd) { if (((*s) | 0x20) != ((*cmd) | 0x20)) return 0; s++; cmd++; }
    if (*s && *s != ' ') return 0;
    *p = s; skipsp(p);
    return 1;
}

static void dump(uint32_t from, uint32_t to)
{
    uint8_t n = 0;
    for (; from <= to; from++) {
        if (n == 0) { puthex28(from); puts_(": "); }
        puthex(peek(from)); k_chrout(' ');
        if (++n == 16) { n = 0; newline(); }
        if (from == 0x0FFFFFFFUL) break;
    }
    if (n) newline();
}

static void error(const char *m) { uint8_t o = fg; fg = C_ERR; puts_(m); newline(); fg = o; }
static void put_cwd(void);

static void cmd_dir(void)
{
    char name[64]; uint16_t count = 0; uint32_t total = 0, sz;
    if (fs_cmd(6)) { error("dir: no device"); return; }
    { uint8_t o = fg; fg = C_HI; puts_("directory of "); put_cwd(); fg = o; newline(); }
    for (;;) {
        uint8_t col = cx;
        w32(FS + 8, (uint16_t)name);
        if (fs_cmd(7)) break;
        sz = r32(FS + 16);
        if (sz == 0xFFFFFFFFUL) { uint8_t o = fg; fg = C_HI; puts_(name); fg = o; pad(col + 20); puts_("<DIR>"); }
        else { puts_(name); pad(col + 20); putdec(sz); count++; total += sz; }
        if (COLS >= 78 && col == 0) pad(COLS / 2); else newline();
    }
    if (cx) newline();
    putdec(count); puts_(" file(s), "); putdec(total); puts_(" bytes"); newline();
}

static void cmd_cd(const char *p)
{
    char name[64];
    if (!getname(&p, name)) strcpy(name, "/");
    fs_name(name);
    if (fs_cmd(11)) { error("cd: no such directory"); return; }
}
static void cmd_mkdir(const char *p)
{
    char name[64];
    if (!getname(&p, name)) { error("mkdir: name?"); return; }
    fs_name(name);
    if (fs_cmd(12)) { error("mkdir: failed"); return; }
}
static void cmd_rm(const char *p)
{
    char name[64]; uint8_t st;
    if (!getname(&p, name)) { error("rm: name?"); return; }
    fs_name(name);
    st = fs_cmd(13);
    if (st == 1) { error("rm: not found"); return; }
    if (st) { error("rm: not a file"); return; }
}
static void cmd_rmdir(const char *p)
{
    char name[64]; uint8_t st;
    if (!getname(&p, name)) { error("rmdir: name?"); return; }
    fs_name(name);
    st = fs_cmd(14);
    if (st == 1) { error("rmdir: not found"); return; }
    if (st) { error("rmdir: not a directory, or not empty"); return; }
}
static void put_cwd(void) { char cwd[64]; w32(FS + 8, (uint16_t)cwd); fs_cmd(15); puts_(cwd); }

static uint8_t is_prg(const char *name)
{
    uint8_t n = strlen(name);
    return n > 4 && name[n - 4] == '.' && (name[n - 3] | 0x20) == 'p' && (name[n - 2] | 0x20) == 'r' && (name[n - 1] | 0x20) == 'g';
}

/* returns 0 on success. A .prg carries a 4-byte header: load address, run address.
 * A segmented program (K-03) starts with "K4SG" instead:
 *   4 nseg  5 flags  6-7 entry (CPU address)
 *   then nseg x 12 (nseg <= 8): phys[4] len[4] block (bank it there, $FF = no) pad[3]
 *   then the segments' bytes, in order. Each lands at its physical address
 *   anywhere in 256 MB; a block number also sets that bank register. */
static uint8_t do_load(const char *name, uint32_t addr, uint8_t has_addr)
{
    uint8_t hdr[4];
    last_run = 0; last_segs = 0;
    fs_name(name);
    if (is_prg(name)) {
        if (fs_cmd(1)) return 1;
        w32(FS + 8, (uint16_t)hdr); w32(FS + 12, 4);
        if (fs_cmd(3) || r32(FS + 12) != 4) { fs_cmd(5); return 2; }
        if (hdr[0] == 'K' && hdr[1] == '4' && hdr[2] == 'S' && hdr[3] == 'G') {
            static uint8_t tab[8 * 12];          /* the table comes first, then the segments' bytes */
            uint8_t i, n; uint32_t phys, len, total = 0;
            w32(FS + 8, (uint16_t)hdr); w32(FS + 12, 4);
            if (fs_cmd(3) || r32(FS + 12) != 4) { fs_cmd(5); return 2; }
            n = hdr[0]; last_run = hdr[2] | ((uint16_t)hdr[3] << 8);
            if (n == 0 || n > 8) { fs_cmd(5); return 2; }
            w32(FS + 8, (uint16_t)tab); w32(FS + 12, (uint32_t)n * 12);
            if (fs_cmd(3) || r32(FS + 12) != (uint32_t)n * 12) { fs_cmd(5); return 2; }
            for (i = 0; i < n; i++) {
                uint8_t *e = tab + i * 12;
                phys = r32((uint16_t)e) & 0x0FFFFFFFUL; len = r32((uint16_t)e + 4);
                if (len) {
                    w32(FS + 8, phys); w32(FS + 12, len);
                    if (fs_cmd(3) || r32(FS + 12) != len) { fs_cmd(5); return 2; }
                }
                if (e[8] < 8) w32(BANK + 4 * e[8], phys);
                if (i == 0) addr = phys;
                total += len;
            }
            fs_cmd(5);
            last_len = total; last_segs = n;
        } else {
            if (!has_addr) addr = hdr[0] | ((uint16_t)hdr[1] << 8);
            last_run = hdr[2] | ((uint16_t)hdr[3] << 8);
            w32(FS + 8, addr); w32(FS + 12, 0x100000UL);
            if (fs_cmd(3)) { fs_cmd(5); return 2; }
            last_len = r32(FS + 12); fs_cmd(5);
        }
    } else {
        w32(FS + 8, addr);
        if (fs_cmd(9)) return 1;
        last_len = r32(FS + 12);
    }
    last_addr = addr; strcpy(last_name, name); xam = addr;
    return 0;
}

static void cmd_load(const char *p)
{
    char name[64]; uint8_t d, st, has = 0; uint32_t addr = USER;
    if (!getname(&p, name)) { error("load: name?"); return; }
    if (*p) { addr = parsehex(&p, &d); has = 1; }
    st = do_load(name, addr, has);
    if (st == 1) { error("load: not found"); return; }
    if (st) { error("load: bad file"); return; }
    puts_("loaded "); putdec(last_len); puts_(" bytes at "); puthex28(last_addr);
    if (last_segs) { puts_(" in "); putdec(last_segs); puts_(" segments"); }
    if (last_run) { puts_(", run address "); puthex16(last_run); }
    newline();
}

static void cmd_save(const char *p)
{
    char name[64]; uint8_t d; uint32_t from, to;
    if (!getname(&p, name)) { error("save: name from.to"); return; }
    from = parsehex(&p, &d); if (!d || *p != '.') { error("save: name from.to"); return; }
    p++; to = parsehex(&p, &d); if (!d || to < from) { error("save: name from.to"); return; }
    fs_name(name); w32(FS + 8, from); w32(FS + 12, to - from + 1);
    if (fs_cmd(10)) { error("save: failed"); return; }
    puts_("saved "); putdec(to - from + 1); puts_(" bytes"); newline();
}

static void cmd_type(const char *p)
{
    char name[64]; uint32_t n; uint16_t i;
    if (!getname(&p, name)) { error("type: name?"); return; }
    fs_name(name);
    if (fs_cmd(1)) { error("type: not found"); return; }
    for (;;) {
        w32(FS + 8, (uint16_t)line); w32(FS + 12, sizeof line);
        if (fs_cmd(3)) break;
        n = r32(FS + 12); if (!n) break;
        for (i = 0; i < n; i++) k_chrout(line[i]);
    }
    fs_cmd(5);
    if (cx) newline();
}

typedef void (*fn_t)(void);
static void video_init(void);
static void run_at(uint16_t a)
{
    call_prog(a);
    video_init();                    /* the program may have reconfigured VICKe */
    cls();
}
static void cmd_run(const char *p)
{
    uint8_t d; uint32_t a; const char *q = p;
    while (ishex(*q)) q++;
    if (*p && *q && *q != ' ') {                  /* not a hex number: RUN name.prg */
        char name[64]; uint8_t st;
        getname(&p, name);
        st = do_load(name, USER, 0);
        if (st == 1 && !is_prg(name) && strlen(name) < 59) { strcat(name, ".prg"); st = do_load(name, USER, 0); }   /* RUN ehbasic -> ehbasic.prg */
        if (st == 1) { error("run: not found"); return; }
        if (st) { error("run: bad file"); return; }
        if (!last_run) { error("run: not a program"); return; }
        run_at(last_run); return;
    }
    a = last_run ? last_run : (last_addr ? last_addr : xam);
    if (*p) a = parsehex(&p, &d);
    if (a >= 0x10000UL) { error("run: 16-bit address"); return; }
    run_at((uint16_t)a);
}

static void cmd_fill(const char *p)
{
    uint8_t d; uint32_t from, to, v;
    from = parsehex(&p, &d); if (!d || *p != '.') { error("fill: from.to value"); return; }
    p++; to = parsehex(&p, &d); if (!d || to < from) { error("fill: from.to value"); return; }
    skipsp(&p); v = parsehex(&p, &d); if (!d) { error("fill: from.to value"); return; }
    dma_fill((uint8_t)v, from, to - from + 1);
    putdec(to - from + 1); puts_(" bytes filled"); newline();
}

static void cmd_copy(const char *p)
{
    uint8_t d; uint32_t from, to, dst;
    from = parsehex(&p, &d); if (!d || *p != '.') { error("copy: from.to dest"); return; }
    p++; to = parsehex(&p, &d); if (!d || to < from) { error("copy: from.to dest"); return; }
    skipsp(&p); dst = parsehex(&p, &d); if (!d) { error("copy: from.to dest"); return; }
    dma_copy(from, dst, to - from + 1);
    putdec(to - from + 1); puts_(" bytes copied to "); puthex28(dst); newline();
}

static void video_init(void);
static void cmd_mode(const char *p)
{
    uint8_t d; uint32_t m;
    if (!*p) { puts_("MODE "); putdec(vmode); puts_(": "); putdec(COLS); k_chrout('x'); putdec(ROWS); puts_(" text, ");
               puts_(vmode == 0 ? "640x480" : vmode == 1 ? "640x240" : "320x240"); puts_(" pixels, margin "); putdec(margin); puts_("   (MODE 0|1|2 [0|1])"); newline(); return; }
    m = parsehex(&p, &d); if (!d || m > 2) { error("mode: 0 = 80x60 (640x480), 1 = 80x30 (640x240), 2 = 40x30 (320x240)  [0|1: margin]"); return; }
    vmode = (uint8_t)m; skipsp(&p);
    if (*p) { m = parsehex(&p, &d); if (!d || m > 1) { error("mode: second value 0 = full screen, 1 = one-cell margin"); return; } margin = (uint8_t)m; }
    video_init(); cls();
}

static void cmd_color(const char *p)
{
    uint8_t d; uint32_t f, b = bg;
    f = parsehex(&p, &d); if (!d) { error("color: fg [bg]  (palette indices, hex)"); return; }
    skipsp(&p); if (*p) b = parsehex(&p, &d);
    fg = (uint8_t)f; bg = (uint8_t)b; REG(VICKE + 1) = bg;
    cls();
}

/* ---- INFO ----------------------------------------------------------------- */
static const char *const daynames[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *const modenames[4] = { "bitmap", "tile", "text8", "text32" };

#pragma code-name (push, "CODE2")      /* INFO, HELP and the banner live in the $E000 half of the ROM */
static void info_version(void)
{
    uint8_t i;
    label("SYSTEM"); puts_("BMC-K4510 system ROM " ROM_VERSION ", emulator ");
    for (i = 0; i < 16 && REG(SYS + 0x10 + i); i++) k_chrout(REG(SYS + 0x10 + i));
    newline();
}

static void info_cpu(void)
{
    uint16_t it; uint32_t mhz100;
    label("CPU"); puts_("45GS02: 4510 + Q register + 32-bit flat + 28-bit MAP, "); putdec(r16(SYS)); puts_(" kHz"); newline();
    it = speed_loop();                                  /* 18 cycles per iteration, one frame */
    mhz100 = (uint32_t)it * 18UL * 60UL / 10000UL;
    pad(8); puts_("measured "); putdec(mhz100 / 100); k_chrout('.'); putdec2(mhz100 % 100); puts_(" MHz  (");
    putdec(it); puts_(" loop iterations per frame, 18 cycles each)"); newline();
}

static void info_mem(void)
{
    uint16_t rombase = (uint16_t)REG(SYS + 0x20) << 8;
    label("MEMORY"); putdec(r16(SYS + 2)); puts_(" MB physical, 28-bit, MAP + DMA + flat addressing"); newline();
    pad(8); puts_("CPU view: zp $0000-$00FF  stack $0100-$01FF  ROM data $0200-$02FF, $0440-$07FF"); newline();
    pad(8); puts_("user $0800-$9FFF ("); putdec((USER_END - USER) / 1024); puts_(" KB); ROM out: $0800-$BFFF + $E000-$FEFF (54 KB)"); newline();
    pad(8); puts_("(banks 5->$A000 7->$E000; ROM keeps $C000-$CFFF, $FF00 page)"); newline();
    pad(8); puts_("text screen at $030000 (far)"); newline();
    pad(8); puts_("I/O $D000-$DFFF  ROM $"); puthex16(rombase); puts_("-$FFFF ("); putdec((0x10000UL - rombase) / 1024); puts_(" KB)"); newline();
    pad(8); puts_("font at $0010000, MAP window convention $2000-$BFFF"); newline();
    { uint8_t b, any = 0;
      for (b = 0; b < 8; b++) if (REG(BANK + 4 * b + 3) != 0xFF) {
          if (!any) { pad(8); puts_("banks:"); any = 1; }
          puts_(" "); putdec(b); puts_("=$"); puthex28(r32(BANK + 4 * b)); }
      if (any) newline(); }
    if (last_len) { pad(8); puts_("last load: "); puts_(last_name); puts_(", "); putdec(last_len); puts_(" bytes at $"); puthex28(last_addr); if (last_run) { puts_(", run $"); puthex16(last_run); } newline(); }
}

static void info_video(void)
{
    uint8_t ctrl = REG(VICKE), n, L, lc, cnt = 0; uint32_t t; uint8_t i;
    label("VIDEO"); puts_("VICKe "); puts_((ctrl & 2) ? "320x240" : (ctrl & 4) ? "640x240" : "640x480"); puts_(" (MODE "); putdec(vmode); puts_(")"); puts_(", display ");
    onoff(ctrl & 1); puts_(", bg colour $"); puthex(REG(VICKE + 1)); puts_(", raster "); putdec(r16(VICKE + 2) & 0x1FF);
    puts_(", irq mask $"); puthex(REG(VICKE + 5)); newline();
    for (n = 0; n < 4; n++) {
        L = 0x10 + n * 0x10; lc = REG(VICKE + L);
        pad(8); puts_("layer "); k_chrout('0' + n); puts_(": ");
        if (!(lc & 1)) { puts_("off"); newline(); continue; }
        puts_(modenames[(lc >> 1) & 3]); k_chrout(' '); putdec(1 << ((lc >> 3) & 3)); puts_(" bpp");
        if (((lc >> 1) & 3) == 1) { puts_(", "); putdec(8 << ((lc >> 5) & 3)); puts_("px tiles"); }
        if (((lc >> 1) & 3) >= 2) { puts_(", 8x"); putdec((lc & 0x20) ? 16 : 8); puts_(" cells"); }
        puts_(", stride "); putdec(r16(VICKE + L + 6)); puts_(", scroll "); putdec(r16(VICKE + L + 2)); k_chrout(','); putdec(r16(VICKE + L + 4)); newline();
        pad(17); puts_("data $"); puthex28(r32(VICKE + L + 8)); puts_("  map $"); puthex28(r32(VICKE + L + 12)); newline();
    }
    t = r32(VICKE + 0x0A);
    pad(8); puts_("sprites "); onoff(REG(VICKE + 0x0E) & 1);
    if (REG(VICKE + 0x0E) & 1) {
        for (i = 0; i < 128; i++) if (peek(t + (uint32_t)i * 16 + 8) & 1) cnt++;
        puts_(", table $"); puthex28(t); puts_(", "); putdec(cnt); puts_(" of 128 enabled");
    }
    newline();
    pad(8); puts_("SHEILA "); onoff(REG(VICKE + 0x64) & 1); puts_(", list $"); puthex28(r32(VICKE + 0x60)); newline();
}

static void info_sound(void)
{
    uint8_t c, v, gates;
    label("SOUND"); puts_("4 x SID 6581 (reSID) at $D400 $D420 $D440 $D460, mono mix"); newline();
    for (c = 0; c < 4; c++) {
        uint16_t b = SID0 + c * 0x20;
        gates = 0; for (v = 0; v < 3; v++) if (REG(b + 4 + v * 7) & 1) gates |= 1 << v;
        pad(8); puts_("SID "); k_chrout('0' + c); puts_(": volume "); putdec(REG(b + 0x18) & 15);
        puts_(", gates "); k_chrout((gates & 1) ? '1' : '-'); k_chrout((gates & 2) ? '2' : '-'); k_chrout((gates & 4) ? '3' : '-');
        puts_(", filter $"); puthex(REG(b + 0x17)); newline();
    }
    pad(8); puts_("OPL2 at $D480: not fitted yet"); newline();
}

static void info_files(void)
{
    char name[64]; uint16_t count = 0; uint32_t total = 0;
    label("FILES"); puts_("host filesystem at $D300 (the emulator's fs/)");
    if (fs_cmd(6)) { puts_(": no device"); newline(); return; }
    for (;;) { w32(FS + 8, (uint16_t)name); if (fs_cmd(7)) break; total += r32(FS + 16); count++; }
    puts_(": "); putdec(count); puts_(" files, "); putdec(total); puts_(" bytes"); newline();
}

static void info_time(void)
{
    uint32_t f; uint32_t s;
    { volatile uint8_t d = REG(SYS + 4); (void)d; }      /* latch the host clock */
    label("TIME"); putdec(r16(SYS + 0x0A)); k_chrout('-'); putdec2(REG(SYS + 9)); k_chrout('-'); putdec2(REG(SYS + 8));
    k_chrout(' '); putdec2(REG(SYS + 7)); k_chrout(':'); putdec2(REG(SYS + 6)); k_chrout(':'); putdec2(REG(SYS + 5));
    k_chrout(' '); puts_(daynames[REG(SYS + 12) % 7]);
    f = (uint32_t)REG(SYS + 0x0D) | ((uint32_t)REG(SYS + 0x0E) << 8) | ((uint32_t)REG(SYS + 0x0F) << 16);
    s = f / 60;
    puts_(", up "); putdec(s / 3600); k_chrout(':'); putdec2((s / 60) % 60); k_chrout(':'); putdec2(s % 60);
    puts_(" ("); putdec(f); puts_(" frames)"); newline();
}

static void cmd_info(const char *p)
{
    uint8_t flags = 0;
    while (*p) {
        if (*p == '-') { p++; continue; }
        switch (*p | 0x20) {
        case 'v': flags |= 1; break;   case 'c': flags |= 2; break;   case 'm': flags |= 4; break;
        case 'g': flags |= 8; break;   case 's': flags |= 16; break;  case 'f': flags |= 32; break;
        case 't': flags |= 64; break;  case 'a': flags |= 127; break;
        case ' ': break;
        default: error("info [-v -c -m -g -s -f -t]  version cpu memory graphics sound files time"); return;
        }
        p++;
    }
    if (!flags) flags = 127;
    if (flags & 1)  info_version();
    if (flags & 2)  info_cpu();
    if (flags & 4)  info_mem();
    if (flags & 8)  info_video();
    if (flags & 16) info_sound();
    if (flags & 32) info_files();
    if (flags & 64) info_time();
}

uint8_t k_shell(const char *p);
static void cmd_mon(const char *p);
/* DUMP [note]: the emulator writes dumps/dump-NNN.txt with the machine state,
 * the screen, the PC history and the shell log; the note goes into the log */
static void cmd_dump(const char *p)
{
    uint8_t n;
    if (is_cmd(&p, "ON"))  { REG(SYS + 0xF2) = 1; puts_("auto dump on: every 15 s of run time"); newline(); return; }
    if (is_cmd(&p, "OFF")) { REG(SYS + 0xF2) = 0; puts_("auto dump off"); newline(); return; }
    REG(SYS + 0xF1) = '#'; while (*p) REG(SYS + 0xF1) = *p++; REG(SYS + 0xF1) = '\n';
    REG(SYS + 0xF0) = 1;
    n = REG(SYS + 0xF0);
    if (n) { puts_("dump "); putdec(n); puts_(" written (dumps/dump-"); if (n < 100) k_chrout('0'); if (n < 10) k_chrout('0'); putdec(n); puts_(".txt)"); newline(); }
    else error("dump: failed");
}
static void cmd_help(void)
{
    uint8_t o = fg;
    fg = C_HI; puts_("monitor: "); fg = o; puts_("MON [line]   then  addr   addr.addr   addr:b b b   addrR   X"); newline();
    fg = C_HI; puts_("files:   "); fg = o; puts_("DIR  CD [dir]  MKDIR dir  RM name  RMDIR dir  TYPE name  LOAD name [addr]"); newline();
    pad(9); puts_("SAVE name from.to  RUN [name.prg|addr]    (bare names also look in /PRG, /BASIC)"); newline();
    fg = C_HI; puts_("memory:  "); fg = o; puts_("FILL from.to value   COPY from.to dest"); newline();
    fg = C_HI; puts_("system:  "); fg = o; puts_("INFO [-vcmgsft]  TIME  MODE [0-2] [0|1]  COLOR fg [bg]  ECHO  CLS  RESET  DUMP [note|ON|OFF]"); newline();
}

static void shell_line(const char *p)
{
    uint8_t d; uint32_t v;
    skipsp(&p);
    if (!*p) return;
    { const char *q = p; while (*q) REG(SYS + 0xF1) = *q++; REG(SYS + 0xF1) = '\n'; }   /* the shell log, for DUMP */
    if (is_cmd(&p, "DIR"))   { cmd_dir(); return; }
    if (is_cmd(&p, "CD") || is_cmd(&p, "CHDIR")) { cmd_cd(p); return; }
    if (is_cmd(&p, "MKDIR")) { cmd_mkdir(p); return; }
    if (is_cmd(&p, "RM") || is_cmd(&p, "ERASE") || is_cmd(&p, "DEL")) { cmd_rm(p); return; }
    if (is_cmd(&p, "RMDIR")) { cmd_rmdir(p); return; }
    if (is_cmd(&p, "LOAD"))  { cmd_load(p); return; }
    if (is_cmd(&p, "SAVE"))  { cmd_save(p); return; }
    if (is_cmd(&p, "TYPE"))  { cmd_type(p); return; }
    if (is_cmd(&p, "RUN"))   { cmd_run(p); return; }
    if (is_cmd(&p, "FILL"))  { cmd_fill(p); return; }
    if (is_cmd(&p, "COPY"))  { cmd_copy(p); return; }
    if (is_cmd(&p, "INFO"))  { cmd_info(p); return; }
    if (is_cmd(&p, "TIME"))  { info_time(); return; }
    if (is_cmd(&p, "COLOR") || is_cmd(&p, "COLOUR")) { cmd_color(p); return; }
    if (is_cmd(&p, "MODE"))  { cmd_mode(p); return; }
    if (is_cmd(&p, "ECHO"))  { puts_(p); newline(); return; }
    if (is_cmd(&p, "CLS"))   { cls(); return; }
    if (is_cmd(&p, "RESET")) { ((fn_t)(*(uint16_t *)0xFFFC))(); return; }
    if (is_cmd(&p, "HELP"))  { cmd_help(); return; }
    if (is_cmd(&p, "DUMP"))  { cmd_dump(p); return; }
    if (is_cmd(&p, "MON"))   { cmd_mon(p); return; }
    error("? (HELP lists the commands; MON is the monitor)");
}

/* the Wozmon grammar: addr  addr.addr  addr:b b b  addrR */
static void mon_line(const char *p)
{
    uint8_t d; uint32_t v;
    mode = 0;
    for (;;) {
        skipsp(&p);
        if (!*p) return;
        if (*p == ':') { mode = 1; p++; continue; }
        if (*p == '.') { mode = 2; p++; continue; }
        if (*p == 'R' || *p == 'r') { if (xam < 0x10000UL) run_at((uint16_t)xam); else error("run: 16-bit address"); return; }
        v = parsehex(&p, &d);
        if (!d) { error("?"); return; }
        if (mode == 1) { poke(xam++, (uint8_t)v); continue; }
        if (mode == 2) { dump(xam, v); xam = v + 1; mode = 0; continue; }
        xam = v; dump(v, v);
    }
}

/* MON: the machine monitor, Wozmon's grammar at a * prompt; X leaves (back to
 * the shell, or to BASIC when entered with @MON). Shell commands work too. */
static void cmd_mon(const char *p)
{

    if (*p) { mon_line(p); return; }                     /* MON E000.E00F : one line, no prompt */
    { uint8_t o = fg; fg = C_DIM; puts_("monitor: addr  addr.addr  addr:b b b  addrR  (28-bit hex)   X leaves"); newline(); fg = o; }
    for (;;) {
        const char *q;
        puts_("*");
        readline(line, sizeof line);   /* the shell line buffer: its previous contents were consumed above */
        q = line; skipsp(&q);
        if (!*q) continue;
        if (is_cmd(&q, "X") || is_cmd(&q, "EXIT") || is_cmd(&q, "Q")) return;
        if (ishex(*q) || *q == ':' || *q == '.') mon_line(q); else shell_line(q);
    }
}

static const uint8_t c64pal[16][3] = {
    {0,0,0},{255,255,255},{136,0,0},{170,255,238},{204,68,204},{0,204,85},{0,0,170},{238,238,119},
    {221,136,85},{102,68,0},{255,119,119},{51,51,51},{119,119,119},{170,255,102},{0,136,255},{187,187,187} };

static void video_init(void)
{
    uint8_t i;
    PCOLS = vmode == 2 ? 40 : 80; PROWS = vmode == 0 ? 60 : 30;
    COLS = PCOLS - margin; ROWS = PROWS - margin;
    REG(VICKE + 0) = 0;
    REG(VICKE + 1) = C_BG;
    for (i = 0; i < 16; i++) { REG(VICKE + 6) = i; REG(VICKE + 7) = c64pal[i][0]; REG(VICKE + 8) = c64pal[i][1]; REG(VICKE + 9) = c64pal[i][2]; }
    /* layer 0: text32, 8x8, map SCREEN, glyphs FONT, 80 cells/row */
    w16(VICKE + 0x16, PCOLS);
    w32(VICKE + 0x1C, SCREEN);
    w32(VICKE + 0x18, FONT);
    w16(VICKE + 0x12, 0); w16(VICKE + 0x14, 0);
    REG(VICKE + 0x11) = 0;
    REG(VICKE + 0x10) = 0x01 | (3 << 1);      /* enable | text32 */
    for (i = 1; i < 4; i++) REG(VICKE + 0x10 + i * 0x10) = 0;
    REG(VICKE + 0x0E) = 0; REG(VICKE + 0x64) = 0;
    REG(VICKE + 5) = 1;                        /* IRQ on vblank */
    REG(VICKE + 0) = (uint8_t)(1 | (vmode == 2 ? 2 : vmode == 1 ? 4 : 0));
}

/* the VIDEO system call ($FF92): the text screen's mode and palette back, screen contents kept */
void k_video(void) { video_init(); }
/* the SHELL system call ($FF8F): run one command line from a program (EhBASIC's @) */
uint8_t k_shell(const char *p) { shell_line(p); if (cx) newline(); return 0; }

/* box-drawing glyphs of the CP437 font */
#define B_H 0xC4
#define B_V 0xB3
#define B_TL 0xDA
#define B_TR 0xBF
#define B_BL 0xC0
#define B_BR 0xD9
#define B_LT 0xC3
#define B_RT 0xB4
static void hline(uint8_t l, uint8_t r, uint8_t w) { uint8_t i; k_chrout(l); for (i = 0; i < w; i++) k_chrout(B_H); k_chrout(r); newline(); }
static void row_open(void) { k_chrout(B_V); k_chrout(' '); }
static void row_close(uint8_t w) { pad(w + 1); k_chrout(B_V); newline(); }
static void field(const char *name, const char *text) { uint8_t o = fg; fg = C_HI; puts_(name); fg = o; puts_(text); }

/* the logo: an hourglass of colour blocks, left-aligned, five rows; the
 * machine's name, speed and memory on its right. Everything else is INFO. */
static void banner(void)
{
    static const uint8_t width[5] = { 12, 8, 4, 8, 12 };
    static const uint8_t colour[5] = { 2, 8, 7, 5, 14 };      /* red, orange, yellow, green, light blue */
    uint8_t r, i, obg = bg, ofg = fg;
    cls();
    newline();
    for (r = 0; r < 5; r++) {
        k_chrout(' '); k_chrout(' ');
        bg = colour[r]; fg = colour[r];
        for (i = 0; i < width[r]; i++) k_chrout(' ');
        bg = obg;
        pad(18);
        switch (r) {
        case 0: fg = C_HI;  puts_("BMC-K4510"); break;
        case 1: fg = C_DIM; puts_("a fantasy 8/16-bit computer"); break;
        case 3: fg = C_FG;  puts_("45GS02 at 40.5 MHz"); break;
        case 4: fg = C_FG;  puts_("256 MB"); break;
        }
        fg = ofg;
        newline();
    }
    newline();
}

int main(void)
{
    vmode = 1; margin = 1;                   /* MODE 1 1: 640x240, 79x29 with a one-cell margin */
    video_init();
    fg = C_FG;
    banner();
    for (;;) {
        put_cwd(); puts_("] ");
        readline(line, sizeof line);
        shell_line(line);
    }
    return 0;
}
