/* K4510 desktop frontend -- spike version.
 *
 * SDL2 window, 60 Hz. Each frame: run the 45GS02 for a frame's worth of
 * cycles, feed keys into the keyboard register, let VICKe render screen
 * RAM. The ROM (Wozmon) does everything else.
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/vicke.h"
#include "../core/sid.h"

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

int main(int argc, char **argv)
{
    const char *rom = (argc > 1) ? argv[1] : "rom/kernal.bin";
    if (argc > 2) fs_set_root(argv[2]);
    uint8_t font[256 * 8];
    if (load_file("data/font8.bin", font, sizeof font) != sizeof font) {
        fprintf(stderr, "need data/font8.bin (run from repo root)\n");
        return 1;
    }
    if (mem_init() != 0) { fprintf(stderr, "cannot reserve %u MB\n", K4510_PHYS_SIZE >> 20); return 1; }
    mem_load(K4510_FONT8_PHYS, font, sizeof font);       /* the ROM points VICKe at this */


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
    SDL_RenderSetLogicalSize(ren, VICKE_WIDTH, VICKE_HEIGHT);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         VICKE_WIDTH, VICKE_HEIGHT);

    static uint8_t fb[VICKE_WIDTH * VICKE_HEIGHT];

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
                case SDLK_F12: cpu65_reset(); break;
                default:
                    if (k >= SDLK_F1 && k <= SDLK_F11) kbd_push((uint8_t)(KEY_F1 + (k - SDLK_F1)));
                    break;
                }
                break; }
            }
        }
        vicke_begin_frame(fb, VICKE_WIDTH);
        for (int y = 0; y < VICKE_HEIGHT; y++) {
            cpu65.irqLevel = vicke_irq() ? 1 : 0;
            cpu65_step(CYCLES_PER_LINE);
            vicke_line(y);
            { int16_t tmp[256]; int n = sid_render(CYCLES_PER_LINE, tmp, 256);
              for (int i = 0; i < n; i++) if (((ring_w - ring_h) & RING_MASK) < RING_MASK) ring[ring_w++ & RING_MASK] = tmp[i]; }
        }
        vicke_end_frame();
        cpu65.irqLevel = vicke_irq() ? 1 : 0;

        void *pixels; int pitch;
        SDL_LockTexture(tex, NULL, &pixels, &pitch);
        for (int y = 0; y < VICKE_HEIGHT; y++) {
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + y * pitch);
            const uint8_t *src = fb + y * VICKE_WIDTH;
            for (int x = 0; x < VICKE_WIDTH; x++) dst[x] = 0xFF000000u | vicke_palette_rgb(src[x]);
        }
        SDL_UnlockTexture(tex);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
