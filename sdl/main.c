/* K4510 desktop frontend -- spike version.
 *
 * SDL2 window, 60 Hz. Each frame: run the 45GS10 for a frame's worth of
 * cycles, feed keys into the keyboard register, let VICKe render screen
 * RAM. The ROM (Wozmon) does everything else.
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/vicke.h"
#include "../core/sid.h"
#include "../core/host.h"
#include "../core/ui/settings.h"
#include "../core/ui/menu.h"
#include "../core/ui/ui_draw.h"
#include "../core/state.h"
#include <sys/stat.h>
#include <time.h>

#define SCALE 2
#define AUDIO_RATE 48000

/* Audio: core renders into a ring per scanline; SDL drains it in its thread. */
static int16_t ring[1 << 15]; static volatile unsigned ring_w, ring_h;
#define RING_MASK ((1 << 15) - 1)
static void audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud; int16_t *out = (int16_t *)stream; int n = len / 2;
    for (int i = 0; i < n; i++) out[i] = (ring_h != ring_w) ? ring[ring_h++ & RING_MASK] : 0;
}
#define CPU_HZ 40500000           /* MEGA65-class; the ceiling is ours, per the design */
#define CYCLES_PER_FRAME (CPU_HZ / 60)
#define CYCLES_PER_LINE  (CYCLES_PER_FRAME / VICKE_HEIGHT)

static int load_file(const char *path, uint8_t *buf, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, max, f);
    fclose(f);
    return (int)n;
}

/* A PETSCII chargen (512 glyphs, both sets) rearranged into the ASCII/CP437
 * order the ROM prints in: letters, digits and punctuation from the
 * lower-case set, the box glyphs the ROM and JIM use from the graphics. */
static void petscii_to_ascii(const uint8_t *cg, uint8_t *out)
{
    const uint8_t *lo = cg + 2048;                      /* set 2: upper/lower case */
    static const struct { uint8_t ascii, glyph; } box[] = {
        { 0xC4, 0x40 }, { 0xB3, 0x5D }, { 0xDA, 0x70 }, { 0xBF, 0x6E }, { 0xC0, 0x6D }, { 0xD9, 0x7D },
        { 0xC3, 0x6B }, { 0xB4, 0x73 }, { 0xC5, 0x5B }, { 0xC1, 0x71 }, { 0xC2, 0x72 }, { 0xDB, 0xE0 },
        { 0xB0, 0x66 }, { 0xB1, 0x66 }, { 0xB2, 0x66 }, { 0xCD, 0x40 }, { 0xBA, 0x5D }, { 0xC9, 0x70 },
        { 0xBB, 0x6E }, { 0xC8, 0x6D }, { 0xBC, 0x7D }, { 0xFB, 0xBA }, { 0x10, 0x3E }, { 0x1B, 0x3C } };
    memset(out, 0, 2048);
    for (int c = 0x20; c < 0x80; c++) {
        int g;
        if (c < 0x40) g = c;                                /* punctuation and digits: same codes */
        else if (c == 0x40) g = 0;                          /* @ */
        else if (c <= 0x5A) g = c;                          /* A-Z at 65-90 in the lower-case set */
        else if (c == 0x5B) g = 0x1B; else if (c == 0x5C) g = 0x1C; else if (c == 0x5D) g = 0x1D;
        else if (c == 0x5E) g = 0x1E; else if (c == 0x5F) g = 0x64;   /* ^ as the up arrow, _ as the low bar */
        else if (c == 0x60) g = 0x27;                       /* ` as ' */
        else if (c <= 0x7A) g = c - 0x60;                   /* a-z at 1-26 */
        else if (c == 0x7B) g = 0x73; else if (c == 0x7C) g = 0x5D; else if (c == 0x7D) g = 0x6B; else g = 0x40;
        memcpy(out + c * 8, lo + g * 8, 8);
    }
    for (unsigned i = 0; i < sizeof box / sizeof box[0]; i++) memcpy(out + box[i].ascii * 8, lo + box[i].glyph * 8, 8);
}
static uint8_t font_kernel8[2048], font_menu[2048];
static const char *slot_path(int n) { static char p[32]; snprintf(p, sizeof p, "k4510-slot%d.k4s", n + 1); return p; }
static void slot_refresh(int n)                      /* the slot's row: its file's date, or "empty" */
{
    struct stat st; char b[24];
    if (stat(slot_path(n), &st)) { menu_slot(n, ""); return; }
    struct tm *tm = localtime(&st.st_mtime);
    if (tm) strftime(b, sizeof b, "%b %d %H:%M", tm); else snprintf(b, sizeof b, "%ld KB", (long)(st.st_size >> 10));
    menu_slot(n, b);
}
/* The C64 chargen lives in the machine's own filesystem, not the host's data/:
 * drop chargen.bin into /SYSTEM and the menu can wear it. 4096 bytes, PETSCII
 * order, so the same converter the open-roms chargens use rearranges it. */
static void chargen_path(char *out, int max) { snprintf(out, (size_t) max, "%s/SYSTEM/chargen.bin", fs_get_root()); }
static int chargen_present(void)
{
    char p[512]; chargen_path(p, sizeof p);
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fclose(f);
    return n >= 2048;
}
static void apply_font(int which)
{
    static const char *paths[FONT_COUNT] = { "data/font8.bin", "data/fonts/unscii/font8-unscii.bin",
                                             "data/fonts/openroms/chargen_openroms.rom", "data/fonts/openroms/chargen_pxlfont_2.3.rom", 0 };
    char cg[512]; const char *path = paths[which];
    if (which == FONT_CHARGEN) { chargen_path(cg, sizeof cg); path = cg; }
    uint8_t buf[4096], font[2048]; int n = which ? load_file(path, buf, sizeof buf) : 0;
    if (which == FONT_KERNEL8 || n < 2048) memcpy(font, font_kernel8, 2048);
    else if (n == 4096) petscii_to_ascii(buf, font);
    else memcpy(font, buf, 2048);
    mem_load(K4510_FONT8_PHYS, font, 2048);
}

int k4510_frontend_main(int argc, char **argv)
{
    const char *rom = (argc > 1) ? argv[1] : "rom/kernal.bin";
    const char *cfg = "k4510.cfg";
    if (argc > 2) fs_set_root(argv[2]);
    if (load_file("data/font8.bin", font_kernel8, sizeof font_kernel8) != sizeof font_kernel8) {
        fprintf(stderr, "need data/font8.bin (run from repo root)\n");
        return 1;
    }
    if (load_file("data/fonts/unscii/font8-unscii.bin", font_menu, sizeof font_menu) != sizeof font_menu) memcpy(font_menu, font_kernel8, sizeof font_menu);
    ui_font(font_menu);                                  /* the menu's own font: it must draw whatever the guest did */
    settings_load(cfg);
    if (mem_init() != 0) { fprintf(stderr, "cannot reserve %u MB\n", K4510_PHYS_SIZE >> 20); return 1; }
    settings_label(SET_VIDEO_FONT, FONT_CHARGEN, chargen_present() ? "C64 chargen" : "C64 chargen (none)");
    int font_applied = settings_get(SET_VIDEO_FONT); apply_font(font_applied);   /* the ROM points VICKe at $010000 */
    for (int i = 0; i < MENU_SLOTS; i++) slot_refresh(i);
    menu_info(INFO_VERSION, "k4510 0.3"); menu_info(INFO_ROM, rom); menu_info(INFO_FS, argc > 2 ? argv[2] : "fs");
#ifdef K4510_PI
    menu_info(INFO_HOST, "Raspberry Pi 3B+, Circle");
#else
    menu_info(INFO_HOST, "desktop, SDL2");
#endif


    if (mem_load_rom(rom) <= 0) {
        fprintf(stderr, "cannot load ROM %s\n", rom);
        return 1;
    }
    cpu65_reset();

    sid_init((double)CPU_HZ, AUDIO_RATE);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("BMC-K4510", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       VICKE_WIDTH * SCALE, VICKE_HEIGHT * SCALE, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);      /* no GPU (the dummy driver, a screenshot run) */
    SDL_RenderSetLogicalSize(ren, VICKE_WIDTH, VICKE_HEIGHT);
    /* Two rows of texture per line of the machine, so scanlines cost a second
     * store rather than a second surface: with them off only the top half is
     * written and copied, so it costs nothing at all. */
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         VICKE_WIDTH, VICKE_HEIGHT * 2);
    int scan_applied = -1, smooth_applied = -1, logical_tall = -1;
    const int shooting = getenv("K4510_SHOT") != NULL && getenv("K4510_SHOT_FX") == NULL;
                                     /* the guide's figures want a clean picture; K4510_SHOT_FX asks for one with the effects */

    static uint8_t fb[VICKE_WIDTH * VICKE_HEIGHT], ov[UI_W * UI_H];
    static uint32_t pal[256], dpal[256];          /* the machine's colours, full and scanline-dimmed */
    static uint32_t mpal[256], mdpal[256];        /* the same, half-lit: the picture behind the menu */
    static uint32_t upal[UIC_COUNT], udpal[UIC_COUNT];   /* the menu's own colours */
    int fullscreen_applied = 0;
    int mode_pending = 0;                          /* (mode + 1) the ROM has been asked for, 0 = nothing */
#define MODE_REQ_FRAMES 120                        /* two seconds for the guest to notice, then give up */

    SDL_AudioSpec want = { 0 }, have;
    want.freq = AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024; want.callback = audio_cb;
    SDL_AudioDeviceID adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (adev) SDL_PauseAudioDevice(adev, 0); else fprintf(stderr, "no audio: %s\n", SDL_GetError());
    SDL_StartTextInput();
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_TEXTINPUT: {
                /* The host layout (incl. dead keys) has already produced the character. */
                for (const char *c = e.text.text; *c; c++) {
                    unsigned char ch = (unsigned char)*c;
                    if (ch >= 0x20 && ch < 0x7F) kbd_push(ch);
                }
                break;
            }
            case SDL_KEYDOWN: case SDL_KEYUP: {
                SDL_Keymod m = SDL_GetModState();
                kbd_modifiers(m & KMOD_SHIFT, m & KMOD_CTRL, m & KMOD_ALT);
                if (e.type != SDL_KEYDOWN) break;
                SDL_Keycode k = e.key.keysym.sym;
                { /* the reset chord: a modifier + PageUp ("Commodore + Restore"), or Ctrl+Alt+Del */
                  int ch = settings_get(SET_INPUT_RESET_CHORD), hit = 0;
                  if (k == SDLK_PAGEUP) hit = (ch == CHORD_SUPER_PGUP && (m & KMOD_GUI)) || (ch == CHORD_CTRL_PGUP && (m & KMOD_CTRL)) || (ch == CHORD_ALT_PGUP && (m & KMOD_ALT));
                  if (k == SDLK_DELETE && ch == CHORD_CTRL_ALT_DEL && (m & KMOD_CTRL) && (m & KMOD_ALT)) hit = 1;
                  if (hit) { menu_close(); cpu65_reset(); break; } }
                if ((m & KMOD_CTRL) && k >= 'a' && k <= 'z') { kbd_push((uint8_t)(k - 'a' + 1)); break; }
                switch (k) {
                case SDLK_RETURN: case SDLK_KP_ENTER: kbd_push(KEY_ENTER); break;
                case SDLK_BACKSPACE: kbd_push(KEY_BS); break;
                case SDLK_TAB:       kbd_push(KEY_TAB); break;
                case SDLK_ESCAPE:    if (m & KMOD_SHIFT) running = 0; else kbd_push(KEY_ESC); break;
                case SDLK_UP: kbd_push(KEY_UP); break;     case SDLK_DOWN: kbd_push(KEY_DOWN); break;
                case SDLK_LEFT: kbd_push(KEY_LEFT); break; case SDLK_RIGHT: kbd_push(KEY_RIGHT); break;
                case SDLK_HOME: kbd_push(KEY_HOME); break; case SDLK_END: kbd_push(KEY_END); break;
                case SDLK_PAGEUP: kbd_push(KEY_PGUP); break; case SDLK_PAGEDOWN: kbd_push(KEY_PGDN); break;
                case SDLK_INSERT: kbd_push(KEY_INS); break; case SDLK_DELETE: kbd_push(KEY_DEL); break;
                case SDLK_PAUSE: kbd_push(0x9F); break;
                default:
                    if (k >= SDLK_F1 && k <= SDLK_F12) kbd_push((uint8_t)(KEY_F1 + (k - SDLK_F1)));
                    break;
                }
                break; }
            }
        }
        host_poll_input();                                   /* the Pi: C64 keyboard on GPIO */
        { static const char *feed; static int feed_init, feed_wait, feed_fr;   /* K4510_KEYS: keys typed one per frame, ~ waits 30 */
          if (!feed_init) { feed_init = 1; feed = getenv("K4510_KEYS"); }
          if (feed && *feed && ++feed_fr >= feed_wait) { uint8_t k = (uint8_t)*feed++; if (k == '~') feed_wait = feed_fr + 30; else kbd_push(k == '\n' ? 0x0D : k); } }
        /* The machine's video mode.  Only the ROM can change it -- the console's
         * PCOLS/PROWS/stride are its -- so the menu asks through $D521 bits 5-7
         * and the ROM acts on its next key poll.  Which means the machine has to
         * be running: a frozen one would never see the request, and the point of
         * choosing a resolution in the menu is watching it happen.  So an
         * outstanding request thaws the machine until VICKe's CTRL says it took,
         * or until the wait runs out (a program that never reads a key). */
        { static int mode_shown = -1, margin_shown = -1, mode_req, mode_wait;
          static const uint8_t ctrl_of[VMODE_COUNT] = { 0, 4, 2, 2 | 8, 2 | 8 | 16 };
          uint8_t ctrl = (uint8_t)(vicke_read(VR_CTRL) & (2 | 4 | 8 | 16));
          int machine = -1;
          for (int i = 0; i < VMODE_COUNT; i++) if (ctrl_of[i] == ctrl) machine = i;
          if (mode_shown < 0) { mode_shown = machine < 0 ? settings_get(SET_VIDEO_MODE) : machine;
                                margin_shown = settings_get(SET_VIDEO_MARGIN); }
          if (mode_req) {
              /* Hold it long enough for the machine to actually read the byte.  The
               * machine only runs while the request stands (it is frozen behind the
               * menu otherwise), so clearing it the instant CTRL already matches --
               * which it does for a margin-only change -- would retire the request
               * before the ROM ever saw it. */
              int held = MODE_REQ_FRAMES - mode_wait;
              int done = held >= 10 && machine == mode_req - 1;
              if (done || --mode_wait <= 0) { mode_req = 0; if (machine >= 0) mode_shown = machine; }
          } else if (settings_get(SET_VIDEO_MODE) != mode_shown) {       /* the user picked a mode */
              mode_shown = settings_get(SET_VIDEO_MODE);
              mode_req = mode_shown + 1; mode_wait = MODE_REQ_FRAMES;    /* two seconds to be noticed */
          } else if (settings_get(SET_VIDEO_MARGIN) != margin_shown) {   /* or turned the margin off */
              margin_shown = settings_get(SET_VIDEO_MARGIN);
              mode_req = mode_shown + 1; mode_wait = MODE_REQ_FRAMES;    /* same mode, new margin */
          } else if (machine >= 0 && machine != mode_shown) {            /* or the guest ran MODE itself */
              mode_shown = machine; settings_set(SET_VIDEO_MODE, machine); menu_dirty();
          }
          mode_pending = mode_req; }

        int open = menu_is_open();
        if (!open || mode_pending) {                         /* the machine runs; while the menu is open it is frozen and silent */
            int vol = settings_get(SET_AUDIO_VOLUME);
            vicke_begin_frame(fb, VICKE_WIDTH);
            for (int y = 0; y < VICKE_HEIGHT; y++) {
                cpu65.irqLevel = vicke_irq() ? 1 : 0;
                cpu65_step(CYCLES_PER_LINE);
                vicke_line(y);
                { int16_t tmp[256]; int n = sid_render(CYCLES_PER_LINE, tmp, 256);
                  for (int i = 0; i < n; i++) if (((ring_w - ring_h) & RING_MASK) < RING_MASK) ring[ring_w++ & RING_MASK] = (int16_t)(tmp[i] * vol / 100); }
            }
            vicke_end_frame();
            cpu65.irqLevel = vicke_irq() ? 1 : 0;
        }
        /* what the menu asked for */
        { int act = menu_take_action();
          if (act >= ACT_SAVE_SLOT && act < ACT_SAVE_SLOT + MENU_SLOTS) { state_save(slot_path(act - ACT_SAVE_SLOT)); slot_refresh(act - ACT_SAVE_SLOT); act = ACT_NONE; }
          if (act >= ACT_LOAD_SLOT && act < ACT_LOAD_SLOT + MENU_SLOTS) {
              io_write(IO_TUBE + 3, 2);                    /* the co-processor is not in the file: stopped before the machine changes under it */
              if (state_load(slot_path(act - ACT_LOAD_SLOT)) == 0) font_applied = -1;   /* the font lives in RAM: the file's wins, but the setting reapplies on the next frame */
              act = ACT_NONE; }
        switch (act) {
        case ACT_RESET: cpu65_reset(); break;
        case ACT_POWER_CYCLE: host_zero(k4510_ram, K4510_PHYS_SIZE); mem_reset(); io_reset(); apply_font(font_applied); mem_load_rom(rom); cpu65_reset(); break;
        case ACT_TUBE_STOP: io_write(IO_TUBE + 3, 2); break;
        case ACT_QUIT: running = 0; break;
        } }
        if (menu_closed_pending()) { if (settings_changed()) settings_save(cfg); }
        io_set_opts((settings_get(SET_SHELL_CPMCOM) ? SYSOPT_CPMCOM : 0)   /* the ROM reads this at $D521 */
                    | (settings_get(SET_VIDEO_MARGIN) ? SYSOPT_MARGIN : 0)
                    | (uint8_t)(mode_pending << SYSOPT_MODE_SHIFT));
        if (settings_get(SET_VIDEO_FONT) != font_applied) {
            font_applied = settings_get(SET_VIDEO_FONT); apply_font(font_applied);
            if (open) vicke_repaint(fb, VICKE_WIDTH);    /* frozen: nothing else would draw the new chargen */
        }
        if (settings_get(SET_VIDEO_FULLSCREEN) != fullscreen_applied) { fullscreen_applied = settings_get(SET_VIDEO_FULLSCREEN); SDL_SetWindowFullscreen(win, fullscreen_applied ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }
        /* the menu takes the machine's own row grid: 30 rows over a 240-line
         * mode, 60 over 640x480, so its lines sit on the picture's lines */
        if (ui_cell_h((vicke_read(VR_CTRL) & 6) ? 16 : 8)) menu_dirty();
        menu_draw(ov);

        if (settings_get(SET_VIDEO_SMOOTH) != smooth_applied) {
            smooth_applied = settings_get(SET_VIDEO_SMOOTH);
            SDL_SetTextureScaleMode(tex, smooth_applied == SMOOTH_SHARP ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
            SDL_RenderSetIntegerScale(ren, smooth_applied == SMOOTH_SHARPFIT ? SDL_TRUE : SDL_FALSE);
        }
        /* Scanlines are never a figure's -- but they ARE the menu's: the point
         * of choosing them there is seeing them, so the overlay is drawn
         * through the same path the machine's picture is. */
        scan_applied = shooting ? SCAN_OFF : settings_get(SET_VIDEO_SCANLINES);

        /* the palettes, once a frame instead of once a pixel: the inner loop
         * then reads two tables and stores twice.  Four of them -- the
         * machine's colours and the menu's, each at full and at scanline
         * brightness -- so the menu dims and darkens without a branch. */
        { static const int num[SCAN_COUNT] = { 4, 3, 2, 1 };            /* quarters of full brightness */
          int n = num[scan_applied];
#define SCANDIM(c) (0xFF000000u | ((((c) >> 16 & 255) * n / 4) << 16) \
                                | ((((c) >>  8 & 255) * n / 4) <<  8) \
                                |  (((c)       & 255) * n / 4))
          for (int i = 0; i < 256; i++) {
              uint32_t c = vicke_palette_rgb(i), h = (c >> 1) & 0x7F7F7F;   /* h: half-lit, behind the menu */
              pal[i]  = 0xFF000000u | c;   dpal[i]  = SCANDIM(c);
              mpal[i] = 0xFF000000u | h;   mdpal[i] = SCANDIM(h);
          }
          for (int i = 0; i < UIC_COUNT; i++) {
              uint32_t c = ui_palette_rgb(i);
              upal[i] = 0xFF000000u | c;   udpal[i] = SCANDIM(c);
          }
#undef SCANDIM
        }

        void *pixels; int pitch;
        SDL_LockTexture(tex, NULL, &pixels, &pitch);
        { int tall = scan_applied != SCAN_OFF;         /* two texture rows per line of the machine */
          for (int y = 0; y < VICKE_HEIGHT; y++) {
              const uint8_t *src = fb + y * VICKE_WIDTH, *o = ov + y * UI_W;
              uint32_t *d0 = (uint32_t *)((uint8_t *)pixels + (tall ? 2 * y : y) * pitch);
              uint32_t *d1 = tall ? (uint32_t *)((uint8_t *)pixels + (2 * y + 1) * pitch) : NULL;
              if (!open) {
                  if (tall) for (int x = 0; x < VICKE_WIDTH; x++) { uint8_t c = src[x]; d0[x] = pal[c]; d1[x] = dpal[c]; }
                  else      for (int x = 0; x < VICKE_WIDTH; x++) d0[x] = pal[src[x]];
              } else if (tall) {
                  for (int x = 0; x < VICKE_WIDTH; x++)
                      if (o[x]) { d0[x] = upal[o[x]]; d1[x] = udpal[o[x]]; }
                      else      { d0[x] = mpal[src[x]]; d1[x] = mdpal[src[x]]; }
              } else {
                  for (int x = 0; x < VICKE_WIDTH; x++) d0[x] = o[x] ? upal[o[x]] : mpal[src[x]];
              }
          } }
        SDL_UnlockTexture(tex);
        { int tall = (scan_applied != SCAN_OFF), b = settings_get(SET_VIDEO_BORDER);
          uint32_t bc = vicke_palette_rgb(settings_get(SET_VIDEO_BORDER_COLOUR));
          int k = tall ? 2 : 1;                                  /* logical units per pixel of the machine */
          SDL_Rect half = { 0, 0, VICKE_WIDTH, VICKE_HEIGHT };
          SDL_Rect dr = { b * k, b * k, (VICKE_WIDTH - 2 * b) * k, (VICKE_HEIGHT - 2 * b) * k };
          if (tall != logical_tall) {                            /* 4:3 either way: 640x480, or 1280x960 */
              logical_tall = tall;
              SDL_RenderSetLogicalSize(ren, VICKE_WIDTH * k, VICKE_HEIGHT * k);
          }
          SDL_SetRenderDrawColor(ren, (bc >> 16) & 255, (bc >> 8) & 255, bc & 255, 255);
          SDL_RenderClear(ren);
          SDL_RenderCopy(ren, tex, tall ? NULL : &half, &dr); }
        SDL_RenderPresent(ren);
        { static const char *shot; static int shot_fr, shot_init;      /* K4510_SHOT=file.ppm:frames -- a screenshot of what is on the glass */
          if (!shot_init) { shot_init = 1; shot = getenv("K4510_SHOT"); if (shot) { const char *c = strrchr(shot, ':'); shot_fr = c ? atoi(c + 1) : 120; } }
          if (shot && --shot_fr == 0) {
              char path[256]; snprintf(path, sizeof path, "%.*s", (int)(strrchr(shot, ':') ? strrchr(shot, ':') - shot : (long) strlen(shot)), shot);
              FILE *f = fopen(path, "wb");
              int sh = (scan_applied != SCAN_OFF) ? VICKE_HEIGHT * 2 : VICKE_HEIGHT;   /* the tall texture is two rows a line */
              if (f) { fprintf(f, "P6 %d %d 255\n", VICKE_WIDTH, sh); SDL_LockTexture(tex, NULL, &pixels, &pitch);
                       for (int y = 0; y < sh; y++) for (int x = 0; x < VICKE_WIDTH; x++) { uint32_t p = ((uint32_t *)((uint8_t *)pixels + y * pitch))[x]; fputc((p >> 16) & 255, f); fputc((p >> 8) & 255, f); fputc(p & 255, f); }
                       SDL_UnlockTexture(tex); fclose(f); }
              running = 0; } }
    }
    if (settings_changed()) settings_save(cfg);
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}

#ifndef K4510_PI
int main(int argc, char **argv) { return k4510_frontend_main(argc, argv); }
#endif
