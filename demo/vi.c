/* BMC-K4510: VI name -- a modal editor that keeps the file in far memory.
 *
 * Nothing about the text lives in the 64 KB. Every line is a 256-byte slot out
 * at $0E000000 -- a length byte and up to 255 characters -- and only the line
 * under the cursor is held down here, loaded when the cursor arrives and
 * written back when it leaves. So the file size is bounded by far memory
 * (32000 lines) rather than by the CPU's address space, and there is one code
 * path whatever the size: no small-file case to disagree with the big one.
 *
 * The DMA engine has memmove semantics, so opening or closing a line is a
 * single transfer of everything below it however long the file is, and
 * redrawing pulls each visible line straight out of far memory.
 *
 *   normal  h j k l arrows  0 $  G gg  PgUp PgDn   move
 *           i a I A  o O    insert     x  dd       delete
 *           :w :q :q! :wq :x
 *   insert  Esc leaves; Backspace, Enter, printable
 */
#include "k4510.h"

#define TERM   0xDA00u
#define SLOTS  0x0E000000UL                 /* one 256-byte slot per line */
#define FLAT   0x0E800000UL                 /* the file, flat, for LOAD and SAVE */
#define SLOT(n) (SLOTS + ((uint32_t)(n) << 8))
#define MAXLINES 32000u
#define NAMEMAX 64

void __fastcall__ rom_chrout(unsigned char c);
unsigned char rom_getin(void);
static unsigned char rom_args(void) { return ((unsigned char (*)(void))0xFF95)(); }
static unsigned char rom_load(void) { return ((unsigned char (*)(void))0xFF89)(); }
static unsigned char rom_save(void) { return ((unsigned char (*)(void))0xFF8C)(); }
static void rom_video(void) { ((void (*)(void))0xFF92)(); }
static void zp16(uint8_t a, uint16_t v) { REG(a) = v; REG(a + 1) = v >> 8; }
static void zp32(uint8_t a, uint32_t v) { REG(a)=v; REG(a+1)=v>>8; REG(a+2)=v>>16; REG(a+3)=v>>24; }
static uint32_t zpr32(uint8_t a) { return (uint32_t)REG(a) | ((uint32_t)REG(a+1)<<8) | ((uint32_t)REG(a+2)<<16) | ((uint32_t)REG(a+3)<<24); }
static void far_get(uint32_t p, void *d, unsigned n) { dma_copy(p, (uint32_t)(uint16_t)d, n); }
static void far_put(const void *s, uint32_t p, unsigned n) { dma_copy((uint32_t)(uint16_t)s, p, n); }

static uint8_t ln[256], tmp[256];           /* the cursor's line; a line being drawn */
static char name[NAMEMAX], cmd[NAMEMAX];
static unsigned nlines = 1, cy, top;
static uint8_t cx, cols, rows, mode, dirty, running = 1, pend, cmdlen;
static const char *note = "";

/* ---- screen ------------------------------------------------------------- */
static void put(char c) { REG(TERM) = (uint8_t)c; }
static void say(const char *s) { while (*s) put(*s++); }
static void num(unsigned long v) { char b[8]; uint8_t i = 0; if (!v) { put('0'); return; } while (v) { b[i++] = (char)('0' + v % 10); v /= 10; } while (i) put(b[--i]); }
static void at(uint8_t r, uint8_t c) { put(27); put('['); num((unsigned long)r + 1); put(';'); num((unsigned long)c + 1); put('H'); }
static void eeol(void) { put(27); put('['); put('K'); }
static void sgr(const char *s) { put(27); put('['); say(s); put('m'); }

/* ---- lines -------------------------------------------------------------- */
static void line_in(unsigned n)  { far_get(SLOT(n), ln, 256); }
static void line_out(unsigned n) { far_put(ln, SLOT(n), 256); }
static uint8_t zero;             /* its own byte: tmp belongs to whoever is mid-edit */
static void blank(unsigned n)    { zero = 0; far_put(&zero, SLOT(n), 1); }

static void open_at(unsigned n)             /* make room for a new line at n */
{
    if (nlines >= MAXLINES) return;
    if (n < nlines) dma_copy(SLOT(n), SLOT(n + 1), (uint32_t)(nlines - n) << 8);
    nlines++;
    blank(n);
}
static void close_at(unsigned n)
{
    if (nlines <= 1) { ln[0] = 0; line_out(0); return; }
    if (n + 1 < nlines) dma_copy(SLOT(n + 1), SLOT(n), (uint32_t)(nlines - n - 1) << 8);
    nlines--;
}
static void goline(unsigned n)
{
    if (n >= nlines) n = nlines - 1;
    if (n == cy) return;
    line_out(cy);
    cy = n; line_in(cy);
    if (cx > ln[0]) cx = ln[0];
}

/* ---- files -------------------------------------------------------------- */
static void load_file(void)
{
    uint32_t l, off = 0; unsigned n = 0, i, chunk;
    zp16(0xF0, (uint16_t)name); zp32(0xF2, FLAT);
    if (!name[0] || rom_load()) { nlines = 1; ln[0] = 0; line_out(0); note = "new file"; return; }
    l = zpr32(0xF6);
    ln[0] = 0;
    while (off < l && n < MAXLINES) {
        chunk = (l - off) > 128 ? 128 : (unsigned)(l - off);
        far_get(FLAT + off, tmp, chunk);
        for (i = 0; i < chunk; i++) {
            if (tmp[i] == '\n') { line_out(n); n++; ln[0] = 0; if (n >= MAXLINES) break; }
            else if (tmp[i] != '\r' && ln[0] < 255) { ln[0]++; ln[ln[0]] = tmp[i]; }
        }
        off += chunk;
    }
    if (ln[0] && n < MAXLINES) { line_out(n); n++; }
    nlines = n ? n : 1;
    if (!n) { ln[0] = 0; line_out(0); }
    cy = 0; line_in(0);
}
static void save_file(void)
{
    uint32_t off = 0; unsigned n; uint8_t l8;
    line_out(cy);
    for (n = 0; n < nlines; n++) {
        far_get(SLOT(n), &l8, 1);
        if (l8) dma_copy(SLOT(n) + 1, FLAT + off, l8);
        off += l8;
        dma_fill('\n', FLAT + off, 1); off++;
    }
    zp16(0xF0, (uint16_t)name); zp32(0xF2, FLAT); zp32(0xF6, off);
    note = rom_save() ? "not saved" : "written";
    if (note[0] == 'w') dirty = 0;
    line_in(cy);
}

/* ---- drawing ------------------------------------------------------------ */
static void draw(void)
{
    unsigned r, l; uint8_t c, hoff = 0, w;
    if (cx >= cols) hoff = (uint8_t)(cx - cols + 1);
    for (r = 0; r < (unsigned)(rows - 1); r++) {
        l = top + r;
        at((uint8_t)r, 0);
        if (l < nlines) {
            if (l == cy) { w = ln[0]; for (c = 0; (unsigned)(c + hoff) < w && c < cols; c++) put(ln[1 + c + hoff]); }
            else { far_get(SLOT(l), tmp, 256); w = tmp[0]; for (c = 0; (unsigned)(c + hoff) < w && c < cols; c++) put(tmp[1 + c + hoff]); }
        } else put('~');
        eeol();
    }
    at((uint8_t)(rows - 1), 0);
    if (mode == 2) { put(':'); say(cmd); eeol(); at((uint8_t)(rows - 1), (uint8_t)(cmdlen + 1)); return; }
    sgr("7");
    say(" "); say(name[0] ? name : "[no name]");
    if (dirty) say(" [+]");
    say("  "); num(cy + 1); put('/'); num(nlines); say("  col "); num((unsigned long)cx + 1);
    if (mode == 1) say("   -- INSERT --");
    if (*note) { say("   "); say(note); }
    eeol();
    sgr("0");
    at((uint8_t)(cy - top), (uint8_t)(cx - hoff));
}
static void scroll_fit(void)
{
    if (cy < top) top = cy;
    while (cy >= top + (unsigned)(rows - 1)) top++;
}

/* ---- editing ------------------------------------------------------------ */
static void ins_ch(uint8_t c)
{
    uint8_t i;
    if (ln[0] >= 255) return;
    for (i = ln[0]; i > cx; i--) ln[i + 1] = ln[i];
    ln[cx + 1] = c; ln[0]++; cx++; dirty = 1;
}
static void del_ch(void)
{
    uint8_t i;
    if (cx >= ln[0]) return;
    for (i = (uint8_t)(cx + 1); i < ln[0]; i++) ln[i] = ln[i + 1];
    ln[0]--; dirty = 1;
}
static void split(void)                       /* Enter in insert mode */
{
    uint8_t i, rest = (uint8_t)(ln[0] - cx);
    for (i = 0; i < rest; i++) tmp[i + 1] = ln[cx + 1 + i];
    tmp[0] = rest;
    ln[0] = cx; line_out(cy);
    open_at(cy + 1);
    far_put(tmp, SLOT(cy + 1), 256);
    cy++; cx = 0; line_in(cy); dirty = 1;
}
static void do_cmd(void)
{
    uint8_t i = 0, w = 0, q = 0;
    while (cmd[i]) { if (cmd[i] == 'w') w = 1; if (cmd[i] == 'q') q = 1; if (cmd[i] == 'x') { w = 1; q = 1; } i++; }
    if (w) { if (cmd[1] == ' ' && cmd[2]) { for (i = 0; cmd[i + 2] && i < NAMEMAX - 1; i++) name[i] = cmd[i + 2]; name[i] = 0; } save_file(); }
    if (q) { if (dirty && !w && cmd[i - 1] != '!') note = "unsaved -- :q! or :wq"; else running = 0; }
    mode = 0; cmdlen = 0; cmd[0] = 0;
}

void main(void)
{
    uint8_t k, n = rom_args(); const char *a = *(const char **)0xF0; uint8_t i = 0;
    while (n && *a == ' ') { a++; n--; }
    while (i < n && i < NAMEMAX - 1 && a[i] != ' ') { name[i] = a[i]; i++; }
    name[i] = 0;
    cols = REG(TERM + 5); rows = REG(TERM + 6);
    if (!cols) cols = 80;
    if (!rows) rows = 30;
    load_file();
    REG(TERM + 4) = 2; REG(TERM + 0x0E) = 1;
    while (running) {
        scroll_fit();
        draw();
        do { k = rom_getin(); } while (!k);
        if (mode == 2) {                                   /* the : line */
            if (k == 0x0D) do_cmd();
            else if (k == 0x1B) { mode = 0; cmdlen = 0; cmd[0] = 0; }
            else if (k == 0x08) { if (cmdlen) cmd[--cmdlen] = 0; else mode = 0; }
            else if (k >= 0x20 && k < 0x7F && cmdlen < NAMEMAX - 2) { cmd[cmdlen++] = (char)k; cmd[cmdlen] = 0; }
            continue;
        }
        note = "";
        if (mode == 1) {                                   /* insert */
            if (k == 0x1B) { mode = 0; if (cx) cx--; }
            else if (k == 0x0D) split();
            else if (k == 0x08) { if (cx) { cx--; del_ch(); } else if (cy) { note = "join: not yet"; } }
            else if (k == 0x89) del_ch();
            else if (k == 0x82) { if (cx) cx--; }
            else if (k == 0x83) { if (cx < ln[0]) cx++; }
            else if (k == 0x80) goline(cy ? cy - 1 : 0);
            else if (k == 0x81) goline(cy + 1);
            else if (k >= 0x20 && k < 0x7F) ins_ch(k);
            continue;
        }
        if (pend == 'd') { pend = 0; if (k == 'd') { line_out(cy); close_at(cy); if (cy >= nlines) cy = nlines - 1; line_in(cy); if (cx > ln[0]) cx = ln[0]; dirty = 1; } continue; }
        if (pend == 'g') { pend = 0; if (k == 'g') { goline(0); cx = 0; } continue; }
        switch (k) {
        case 'h': case 0x82: if (cx) cx--; break;
        case 'l': case 0x83: if (cx < ln[0]) cx++; break;
        case 'k': case 0x80: goline(cy ? cy - 1 : 0); break;
        case 'j': case 0x81: goline(cy + 1); break;
        case '0': case 0x84: cx = 0; break;
        case '$': case 0x85: cx = ln[0] ? (uint8_t)(ln[0] - 1) : 0; break;
        case 'G': goline(nlines - 1); cx = 0; break;
        case 'g': pend = 'g'; break;
        case 'd': pend = 'd'; break;
        case 0x86: goline(cy > (unsigned)(rows - 2) ? cy - (rows - 2) : 0); break;
        case 0x87: goline(cy + rows - 2); break;
        case 'i': mode = 1; break;
        case 'a': if (cx < ln[0]) cx++; mode = 1; break;
        case 'I': cx = 0; mode = 1; break;
        case 'A': cx = ln[0]; mode = 1; break;
        case 'x': del_ch(); if (cx && cx >= ln[0]) cx--; break;
        case 'o': line_out(cy); open_at(cy + 1); cy++; cx = 0; line_in(cy); mode = 1; dirty = 1; break;
        case 'O': line_out(cy); open_at(cy); cx = 0; line_in(cy); mode = 1; dirty = 1; break;
        case ':': mode = 2; cmdlen = 0; cmd[0] = 0; break;
        default: break;
        }
    }
    REG(TERM + 0x0E) = 0; REG(TERM + 4) = 2;
    rom_video();
}
