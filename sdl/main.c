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
#include "../core/vicke.h"

#define SCALE 2
#define CPU_HZ 40500000           /* MEGA65-class; the ceiling is ours, per the design */
#define CYCLES_PER_FRAME (CPU_HZ / 60)

/* Build an ASCII-ordered 8x8 font from a C64 chargen (PETSCII screen
 * codes, uppercase set). ASCII $20-$3F map 1:1; $40-$5F and $60-$7F both
 * land on screen codes $00-$1F (so lowercase shows as uppercase for now). */
static void font_from_chargen(const uint8_t *chargen, uint8_t *out)
{
    memset(out, 0, 256 * 8);
    for (int a = 0x20; a < 0x80; a++) {
        int sc = (a < 0x40) ? a : (a & 0x1F);
        if (a >= 0x40 && a < 0x60) sc = a & 0x3F;   /* @ A-Z [ \ ] ^ _ */
        memcpy(&out[a * 8], &chargen[sc * 8], 8);
    }
}

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
    (void)argc; (void)argv;
    uint8_t chargen[4096], font[256 * 8];
    if (load_file("data/chargen", chargen, sizeof chargen) < 2048) {
        fprintf(stderr, "need data/chargen (run from repo root)\n");
        return 1;
    }
    font_from_chargen(chargen, font);

    mem_init();
    vicke_init();
    vicke_set_font(font);

    if (mem_load_rom("rom/wozmon.bin") != 4096) {
        fprintf(stderr, "need rom/wozmon.bin (make rom)\n");
        return 1;
    }
    cpu65_reset();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_Window *win = SDL_CreateWindow("BMC-K4510", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       VICKE_WIDTH * SCALE, VICKE_HEIGHT * SCALE, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, VICKE_WIDTH, VICKE_HEIGHT);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         VICKE_WIDTH, VICKE_HEIGHT);

    static uint8_t fb[VICKE_WIDTH * VICKE_HEIGHT];
    uint32_t pal[256];
    vicke_palette(pal, 256);

    SDL_StartTextInput();
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_TEXTINPUT: {
                /* Wozmon is an uppercase-only 1976 monitor; fold case for it. */
                for (const char *c = e.text.text; *c; c++) {
                    unsigned char ch = (unsigned char)*c;
                    if (ch < 0x80) kbd_push((ch >= 'a' && ch <= 'z') ? ch - 32 : ch);
                }
                break;
            }
            case SDL_KEYDOWN:
                switch (e.key.keysym.sym) {
                case SDLK_RETURN: case SDLK_KP_ENTER: kbd_push(0x0D); break;
                case SDLK_BACKSPACE: kbd_push('_'); break;      /* Wozmon's backspace */
                case SDLK_ESCAPE:
                    if (e.key.keysym.mod & KMOD_SHIFT) running = 0;   /* Shift+Esc quits */
                    else kbd_push(0x1B);
                    break;
                case SDLK_F12: cpu65_reset(); break;
                default: break;
                }
                break;
            }
        }
        cpu65_step(CYCLES_PER_FRAME);
        vicke_render(fb, VICKE_WIDTH);

        void *pixels; int pitch;
        SDL_LockTexture(tex, NULL, &pixels, &pitch);
        for (int y = 0; y < VICKE_HEIGHT; y++) {
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + y * pitch);
            const uint8_t *src = fb + y * VICKE_WIDTH;
            for (int x = 0; x < VICKE_WIDTH; x++) dst[x] = 0xFF000000u | pal[src[x]];
        }
        SDL_UnlockTexture(tex);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
