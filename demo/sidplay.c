/* BMC-K4510 SID player. PSID tunes from /SID, played by their own 6502 code
 * on SID 0, the way a C64 would: the player lives under the ROM at $E000
 * (K-05) so the tune may own $0400-$CFFF; the zero page is swapped around
 * every call into the tune; play() runs at the tune's rate (50 Hz PAL,
 * 60 Hz NTSC or CIA) from the frame counter. VICKe registers a tune may
 * poke (it thinks they are the VIC-II) are put back every frame.
 *   chooser: cursor keys, PgUp/PgDn, Enter plays, Esc leaves
 *   playing: +/- next/previous song, space next file, Esc back to the list */
#include "k4510.h"
#include "far.h"

void __fastcall__ tune_call(unsigned addr);
extern unsigned char tune_a;

#define SCREEN   0x00030000UL        /* the ROM's text map, 80 cells x 4 bytes; margin 1 */
#define LISTBUF  0x00300000UL        /* file names, 32 bytes each */
#define FILEBUF  0x00310000UL        /* the loaded .sid */
#define MAXFILES 400
#define COLS 79
#define ROWS 29
#define C_WHITE 1
#define C_YEL 7
#define C_GREY 12
#define C_LBLUE 14
#define C_GREEN 13
#define C_BLUE 6
#define KEY_UP 0x80
#define KEY_DOWN 0x81
#define KEY_LEFT 0x82
#define KEY_RIGHT 0x83
#define KEY_HOME 0x84
#define KEY_END 0x85
#define KEY_PGUP 0x86
#define KEY_PGDN 0x87

static void put(uint8_t x, uint8_t y, uint8_t ch, uint8_t fg)
{
    uint32_t c = SCREEN + ((uint32_t)(y + 1) * 80 + x + 1) * 4;
    far_poke(c, ch); far_poke(c + 1, 0); far_poke(c + 2, fg); far_poke(c + 3, C_BLUE);
}
static void text(uint8_t x, uint8_t y, const char *s, uint8_t fg) { while (*s) put(x++, y, (uint8_t)*s++, fg); }
static void textn(uint8_t x, uint8_t y, uint32_t fa, uint8_t n, uint8_t fg) { uint8_t i; for (i = 0; i < n; i++) { uint8_t ch = far_peek(fa + i); put(x + i, y, ch ? ch : ' ', fg); } }
static void clear_rows(uint8_t y0, uint8_t y1) { uint8_t x, y; for (y = y0; y <= y1; y++) for (x = 0; x < COLS; x++) put(x, y, ' ', C_YEL); }
static void dec(uint8_t x, uint8_t y, uint16_t v, uint8_t fg) { char b[6]; uint8_t i = 5; b[i] = 0; do { b[--i] = '0' + v % 10; v /= 10; } while (v); text(x, y, b + i, fg); }
static void dec2(uint8_t x, uint8_t y, uint8_t v, uint8_t fg) { put(x, y, '0' + v / 10, fg); put(x + 1, y, '0' + v % 10, fg); }

/* ---- the file list ------------------------------------------------------ */
static uint16_t nfiles;
static char fsname[40];
static char oldcwd[40];
static uint8_t fs_cmd(uint8_t c) { REG(0xD300) = c; return REG(0xD301); }
static void fs_setname(const char *n) { far_w32(0xD304, (uint16_t)n); }
static void list_dir(void)
{
    nfiles = 0;
    fs_setname("/SID"); if (fs_cmd(11)) { fs_setname("/"); fs_cmd(11); }
    if (fs_cmd(6)) return;
    for (;;) {
        uint8_t i, len; char *e;
        far_w32(0xD308, (uint16_t)fsname);
        if (fs_cmd(7)) break;
        if (far_r32(0xD310) == 0xFFFFFFFFUL) continue;                 /* a directory */
        len = 0; while (fsname[len]) len++;
        if (len < 5) continue; e = fsname + len - 4;
        if (e[0] != '.' || (e[1] | 0x20) != 's' || (e[2] | 0x20) != 'i' || (e[3] | 0x20) != 'd') continue;
        for (i = 0; i < 31; i++) far_poke(LISTBUF + (uint32_t)nfiles * 32 + i, i < len ? fsname[i] : 0);
        far_poke(LISTBUF + (uint32_t)nfiles * 32 + 31, 0);
        if (++nfiles == MAXFILES) break;
    }
}
static void get_name(uint16_t n, char *out) { uint8_t i; for (i = 0; i < 31; i++) out[i] = far_peek(LISTBUF + (uint32_t)n * 32 + i); out[31] = 0; }
static uint32_t r32far(uint32_t a) { return (uint32_t)far_peek(a) | ((uint32_t)far_peek(a + 1) << 8) | ((uint32_t)far_peek(a + 2) << 16) | ((uint32_t)far_peek(a + 3) << 24); }

/* ---- the chooser -------------------------------------------------------- */
#define PAGE 24
static void draw_list(uint16_t top, uint16_t cur)
{
    uint16_t i; uint8_t r;
    clear_rows(0, ROWS - 1);
    text(0, 0, "BMC-K4510 SID player", C_WHITE); text(22, 0, "/SID", C_GREY);
    dec(30, 0, nfiles, C_GREY); text(35, 0, "tunes", C_GREY);
    text(0, ROWS - 1, "cursor keys, PgUp/PgDn, Enter plays, Esc leaves", C_GREY);
    for (r = 0; r < PAGE * 2; r++) {
        i = top + r; if (i >= nfiles) break;
        { uint8_t x = (r < PAGE) ? 2 : 41, y = 2 + (r % PAGE);
          put(x - 2, y, i == cur ? 0x10 : ' ', C_WHITE);
          textn(x, y, LISTBUF + (uint32_t)i * 32, 31, i == cur ? C_WHITE : C_YEL); }
    }
}

/* ---- the tune ----------------------------------------------------------- */
static uint16_t init_addr, play_addr, load_addr, nsongs, song; static uint8_t is_rsid, rate;
static uint8_t vregs[0x50];
static void save_video(void) { uint8_t i; for (i = 0; i < 0x50; i++) vregs[i] = REG(0xD000 + i); }
static void restore_video(void) { uint8_t i; for (i = 0x10; i < 0x50; i++) REG(0xD000 + i) = vregs[i]; REG(0xD000) = vregs[0]; REG(0xD001) = vregs[1]; }
static void sid_silence(void) { uint8_t i; for (i = 0; i < 25; i++) { REG(0xD400 + i) = 0; REG(0xD420 + i) = 0; } }

static uint8_t load_tune(uint16_t n)
{
    uint16_t hdr, ver, flags; uint32_t size, speed; char name[32];
    get_name(n, name);
    fs_setname(name); far_w32(0xD308, FILEBUF);
    if (fs_cmd(9)) return 1;
    size = far_r32(0xD30C);
    is_rsid = far_peek(FILEBUF) == 'R';
    ver = far_peek(FILEBUF + 5);
    hdr = ((uint16_t)far_peek(FILEBUF + 6) << 8) | far_peek(FILEBUF + 7);
    load_addr = ((uint16_t)far_peek(FILEBUF + 8) << 8) | far_peek(FILEBUF + 9);
    init_addr = ((uint16_t)far_peek(FILEBUF + 10) << 8) | far_peek(FILEBUF + 11);
    play_addr = ((uint16_t)far_peek(FILEBUF + 12) << 8) | far_peek(FILEBUF + 13);
    nsongs    = ((uint16_t)far_peek(FILEBUF + 14) << 8) | far_peek(FILEBUF + 15);
    song      = ((uint16_t)far_peek(FILEBUF + 16) << 8) | far_peek(FILEBUF + 17);
    speed     = ((uint32_t)far_peek(FILEBUF + 18) << 24) | ((uint32_t)far_peek(FILEBUF + 19) << 16) | ((uint32_t)far_peek(FILEBUF + 20) << 8) | far_peek(FILEBUF + 21);
    flags = ver >= 2 ? (((uint16_t)far_peek(FILEBUF + 0x76) << 8) | far_peek(FILEBUF + 0x77)) : 0;
    if (load_addr == 0) { load_addr = far_peek(FILEBUF + hdr) | ((uint16_t)far_peek(FILEBUF + hdr + 1) << 8); hdr += 2; }
    if (size <= hdr) return 2;
    if (!song) song = 1;
    rate = ((flags >> 2) & 3) == 2 ? 60 : 50;                       /* NTSC tunes at 60, PAL at 50 */
    if (speed & 1) rate = 60;                                        /* CIA-timed: ~60 Hz */
    far_copy(load_addr, FILEBUF + hdr, size - hdr);                  /* into the C64's memory: the CPU view, ROM out */
    return 0;
}

static void show_info(uint16_t n)
{
    char name[32];
    get_name(n, name);
    clear_rows(0, ROWS - 1);
    text(0, 0, "BMC-K4510 SID player", C_WHITE); text(22, 0, name, C_GREY);
    text(0, 2, "title   ", C_LBLUE); textn(8, 2, FILEBUF + 0x16, 32, C_WHITE);
    text(0, 3, "author  ", C_LBLUE); textn(8, 3, FILEBUF + 0x36, 32, C_YEL);
    text(0, 4, "released", C_LBLUE); textn(8, 4, FILEBUF + 0x56, 32, C_YEL);
    text(0, 6, "load $", C_GREY); { char h[5]; uint8_t i; static const char hx[] = "0123456789ABCDEF"; uint16_t v = load_addr;
      for (i = 0; i < 4; i++) { h[3 - i] = hx[v & 15]; v >>= 4; } h[4] = 0; text(6, 6, h, C_GREY);
      v = init_addr; for (i = 0; i < 4; i++) { h[3 - i] = hx[v & 15]; v >>= 4; } text(11, 6, "init $", C_GREY); text(17, 6, h, C_GREY);
      v = play_addr; for (i = 0; i < 4; i++) { h[3 - i] = hx[v & 15]; v >>= 4; } text(22, 6, "play $", C_GREY); text(28, 6, h, C_GREY); }
    dec(34, 6, rate, C_GREY); text(37, 6, "Hz", C_GREY);
    text(0, 9,  "voice 1", C_LBLUE); text(0, 11, "voice 2", C_LBLUE); text(0, 13, "voice 3", C_LBLUE);
    text(0, ROWS - 1, "+/- song   space next tune   Esc back to the list", C_GREY);
    if (is_rsid) text(0, 16, "RSID: needs a real C64 (KERNAL, CIA timers) -- not supported here", C_YEL);
    else if (!play_addr) text(0, 16, "play address 0: the tune installs its own interrupt -- not supported here", C_YEL);
}
static void show_song(void) { text(0, 7, "song", C_GREY); dec(5, 7, song, C_WHITE); text(9, 7, "of", C_GREY); dec(12, 7, nsongs, C_WHITE); }
static void show_time(uint16_t sec) { dec2(72, 7, sec / 60, C_WHITE); put(74, 7, ':', C_WHITE); dec2(75, 7, sec % 60, C_WHITE); }
static void show_meters(void)
{
    static const char *const wn[5] = { "   ", "tri", "saw", "pul", "noi" };
    uint8_t v, i;
    for (v = 0; v < 3; v++) {
        uint16_t b = 0xD400 + v * 7; uint8_t ctl = REG(b + 4), hi = REG(b + 1), y = 9 + v * 2, n = (ctl & 1) ? (hi >> 2) + 1 : 0, w;
        w = (ctl & 0x80) ? 4 : (ctl & 0x40) ? 3 : (ctl & 0x20) ? 2 : (ctl & 0x10) ? 1 : 0;
        text(8, y, (ctl & 1) ? wn[w] : wn[0], C_GREY);
        for (i = 0; i < 64; i++) put(12 + i, y, i < n ? 0xDB : 0xFA, (ctl & 1) ? C_GREEN : C_GREY);
    }
}

static uint8_t start_song(void)
{
    sid_silence();
    tune_a = (uint8_t)(song - 1);
    tune_call(init_addr);
    restore_video();
    return 0;
}

/* returns 0 = back to the list, 1 = next file */
static uint8_t play_file(uint16_t n)
{
    uint8_t acc = 0, last = 0, k; uint16_t frames = 0, sec = 0;
    if (load_tune(n)) { show_info(n); text(0, 16, "could not load or parse this file", C_YEL); while (!(k = key_get())) ; return k == ' ' ? 1 : 0; }
    show_info(n); show_song();
    if (is_rsid || !play_addr) { while (!(k = key_get())) ; return k == ' ' ? 1 : 0; }
    save_video();
    start_song();
    for (;;) {
        uint8_t f = REG(SYS + 0x0D);
        if (f != last) {                                   /* a new frame */
            last = f;
            acc += rate;
            while (acc >= 60) { acc -= 60; tune_call(play_addr); restore_video(); }
            if (++frames == 60) { frames = 0; show_time(++sec); }
            show_meters();
        }
        k = key_get();
        if (k == 0x1B || k == 'q' || k == 'Q') { sid_silence(); return 0; }
        if (k == ' ' || k == KEY_RIGHT || k == KEY_DOWN) { sid_silence(); return 1; }
        if (k == KEY_LEFT || k == KEY_UP) { sid_silence(); return 2; }
        if (k == '+' || k == '=' || k == KEY_PGDN) { if (song < nsongs) song++; else song = 1; start_song(); show_song(); sec = 0; show_time(0); }
        if (k == '-' || k == KEY_PGUP) { if (song > 1) song--; else song = nsongs; start_song(); show_song(); sec = 0; show_time(0); }
    }
}

void main(void)
{
    uint16_t cur = 0, top = 0; uint8_t k;
    rom_out();                                            /* $0800-$CFFF and $E000-$FEFF are ours */
    far_w32(0xD308, (uint16_t)oldcwd); fs_cmd(15);        /* remember the caller's directory */
    list_dir();
    for (;;) {
        draw_list(top, cur);
        for (;;) {
            while (!(k = key_get())) ;
            if (k == 0x1B || k == 'q' || k == 'Q') { sid_silence(); clear_rows(0, ROWS - 1); fs_setname(oldcwd); fs_cmd(11); return; }
            if (k == KEY_DOWN && cur + 1 < nfiles) cur++;
            else if (k == KEY_UP && cur) cur--;
            else if (k == KEY_RIGHT && cur + PAGE < nfiles) cur += PAGE;
            else if (k == KEY_LEFT && cur >= PAGE) cur -= PAGE;
            else if (k == KEY_PGDN) { cur += PAGE * 2; if (cur >= nfiles) cur = nfiles - 1; }
            else if (k == KEY_PGUP) { cur = cur >= PAGE * 2 ? cur - PAGE * 2 : 0; }
            else if (k == KEY_HOME) cur = 0;
            else if (k == KEY_END) cur = nfiles - 1;
            else if (k == 0x0D && nfiles) {
                uint8_t r;
                do { r = play_file(cur); if (r == 1 && cur + 1 < nfiles) cur++; else if (r == 2 && cur) cur--; else if (r) r = 0; } while (r);
                break;
            }
            else continue;
            if (cur < top) top = cur - cur % (PAGE * 2);
            if (cur >= top + PAGE * 2) top = cur - cur % (PAGE * 2);
            break;
        }
    }
}
