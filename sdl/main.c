/* K4510 desktop frontend -- spike version.
 *
 * SDL2 window, 60 Hz, VICKe renders the text layer from screen RAM.
 * Step 2: write "K4510" into screen RAM from C and show it.
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/vicke.h"

#define SCALE 2

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

    /* Step 2 payload: say hello from C, straight into screen RAM. */
    const char *msg = "BMC-K4510  --  VICKe text layer, 80x60";
    mem_load(VICKE_SCREEN_BASE + 1 * VICKE_COLS + 2, (const uint8_t *)msg, strlen(msg));
    const char *msg2 = "45GS02 core: Xemu / cpu65.c   memory: 64 KB (spike)";
    mem_load(VICKE_SCREEN_BASE + 3 * VICKE_COLS + 2, (const uint8_t *)msg2, strlen(msg2));
    for (int c = 0; c < 80; c++) mem_poke(VICKE_SCREEN_BASE + 5 * VICKE_COLS + c, 0x20 + (c % 0x60));

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

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }
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
